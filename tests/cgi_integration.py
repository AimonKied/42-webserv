#!/usr/bin/env python3
"""Run against a freshly built binary: python3 tests/cgi_integration.py ./webserv.
All configuration, scripts, uploaded data and logs live in a temporary directory.
"""
import concurrent.futures
import json
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time


def exchange(port, path='/', body=b'', extra=b'', fragments=False):
    with socket.create_connection(('127.0.0.1', port), timeout=10) as sock:
        sock.settimeout(10)
        method = b'POST' if body or extra else b'GET'
        headers = method + b' ' + path.encode() + b' HTTP/1.1\r\nHost: localhost\r\n'
        headers += extra or b'Content-Length: ' + str(len(body)).encode() + b'\r\n'
        sock.sendall(headers + b'\r\n')
        if fragments:
            time.sleep(.05)
            try:
                sock.settimeout(.05)
                early = sock.recv(1)
                raise AssertionError(f'response before full body: {early!r}')
            except socket.timeout:
                pass
            finally:
                sock.settimeout(10)
        if body:
            sock.sendall(body)
        response = b''
        while True:
            data = sock.recv(65536)
            if not data:
                break
            response += data
    header, payload = response.split(b'\r\n\r\n', 1)
    status = int(header.split(b' ', 2)[1])
    return status, payload


def main():
    binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else './webserv').resolve())
    with tempfile.TemporaryDirectory(prefix='webserv-cgi-test-') as directory:
        root = Path(directory)
        (root / 'index.html').write_text('static works')
        (root / 'echo.py').write_text('''import json, os, sys
body = sys.stdin.buffer.read()
print("Content-Type: application/json\\n")
print(json.dumps({"body": body.decode(), "port": os.environ["SERVER_PORT"],
 "query": os.environ["QUERY_STRING"], "length": os.environ["CONTENT_LENGTH"],
 "cwd": os.getcwd()}))
''')
        (root / 'duplex.py').write_text('''import sys
sys.stdout.write("Content-Type: text/plain\\n\\n" + "x" * 200000)
sys.stdout.flush()
body = sys.stdin.buffer.read()
sys.stdout.write(str(len(body)))
''')
        (root / 'escape.py').symlink_to('/etc/passwd')
        (root / 'fds.py').write_text('''import os
from pathlib import Path
sockets = []
for fd in Path('/proc/self/fd').iterdir():
    try:
        if os.readlink(fd).startswith('socket:'): sockets.append(fd.name)
    except FileNotFoundError: pass
print('Content-Type: text/plain\\n')
print(len(sockets))
''')
        (root / 'slow.py').write_text('import time\ntime.sleep(30)\n')
        (root / 'bad.py').write_text('print("not CGI headers")\n')
        (root / 'fail.py').write_text('raise SystemExit(7)\n')
        (root / 'flood.py').write_text('import sys\nsys.stdout.write("Content-Type: text/plain\\n\\n" + "x" * (5 * 1024 * 1024))\n')
        (root / 'eof.py').write_text('import os, time\nos.close(1)\ntime.sleep(30)\n')
        # A second process holding stdout open makes exit and EOF independent.
        (root / 'late.py').write_text('''import os, time
if os.fork() == 0:
    time.sleep(.2)
    os.write(1, b"Content-Type: text/plain\\n\\nlate output")
    os._exit(0)
os._exit(0)
''')
        holders = [socket.socket(), socket.socket()]
        for s in holders:
            s.bind(('127.0.0.1', 0))
        ports = [s.getsockname()[1] for s in holders]
        config = root / 'test.conf'
        config.write_text('\n'.join(f'''server {{
 listen 127.0.0.1:{port};
 server_name localhost;
 client_max_body_size 1048576;
 root {root};
 location / {{ index index.html; allow_methods GET POST; cgi_extension .py; }}
}}''' for port in ports))
        for s in holders:
            s.close()
        with (root / 'server.log').open('w') as log:
            proc = subprocess.Popen([binary, str(config)], stdout=log, stderr=log)
            try:
                for _ in range(100):
                    if proc.poll() is not None:
                        raise AssertionError('server exited at startup')
                    try:
                        assert exchange(ports[0]) == (200, b'static works')
                        break
                    except ConnectionRefusedError:
                        time.sleep(.03)
                else:
                    raise AssertionError('startup timed out')
                for port in ports:
                    code, payload = exchange(port, '/echo.py?a=1')
                    data = json.loads(payload)
                    assert code == 200 and data['port'] == str(port) and data['query'] == 'a=1'
                    assert data['cwd'] == str(root) and data['body'] == ''
                print('PASS: CGI GET, environment, working directory and both listeners')
                code, payload = exchange(ports[0], '/echo.py', b'hello', fragments=True)
                assert code == 200 and json.loads(payload)['body'] == 'hello'
                code, payload = exchange(ports[0], '/echo.py', b'5\r\nhello\r\n0\r\n\r\n', b'Transfer-Encoding: chunked\r\n')
                assert code == 200 and json.loads(payload)['length'] == '5'
                print('PASS: fragmented Content-Length and decoded chunked input, stdin EOF')
                code, payload = exchange(ports[0], '/duplex.py', b'a' * 200000)
                assert code == 200 and payload == b'x' * 200000 + b'200000'
                assert exchange(ports[0], '/late.py') == (200, b'late output')
                print('PASS: simultaneous pipe input/output, partial I/O and stdout drain after child exit')
                for path, expected in [('/bad.py', 502), ('/fail.py', 502), ('/flood.py', 502), ('/missing.py', 404), ('/escape.py', 403)]:
                    assert exchange(ports[0], path)[0] == expected, path
                if Path('/proc/self/fd').exists():
                    assert exchange(ports[0], '/fds.py') == (200, b'0\n')
                print('PASS: malformed output, nonzero exit, output cap, path containment and no inherited sockets')
                with concurrent.futures.ThreadPoolExecutor() as pool:
                    slow = pool.submit(exchange, ports[0], '/slow.py')
                    time.sleep(.15)
                    start = time.monotonic()
                    assert exchange(ports[1]) == (200, b'static works')
                    assert time.monotonic() - start < 2
                    assert slow.result()[0] == 504
                assert exchange(ports[0], '/eof.py')[0] == 504
                print('PASS: deadlines, EOF-before-exit and static service while CGI waits')
                # Close a client while its child is running, then check direct
                # children and descriptor counts on Linux for repeated requests.
                fd_path = Path(f'/proc/{proc.pid}/fd')
                children_path = Path(f'/proc/{proc.pid}/task/{proc.pid}/children')
                before = len(list(fd_path.iterdir())) if fd_path.exists() else None
                for _ in range(10):
                    s = socket.create_connection(('127.0.0.1', ports[0]))
                    s.sendall(b'GET /slow.py HTTP/1.1\r\nHost: localhost\r\n\r\n')
                    time.sleep(.03)
                    s.close()
                time.sleep(.3)
                if children_path.exists():
                    assert children_path.read_text().strip() == '', 'unreaped/live children after disconnect'
                    assert len(list(fd_path.iterdir())) == before, 'descriptor leak'
                print('PASS: disconnect cancellation and repeated-job cleanup')
                s = socket.create_connection(('127.0.0.1', ports[0]))
                s.sendall(b'GET /slow.py HTTP/1.1\r\nHost: localhost\r\n\r\n')
                time.sleep(.1)
                children = children_path.read_text().split() if children_path.exists() else []
                proc.terminate()
                assert proc.wait(timeout=3) == 0
                s.close()
                for pid in children:
                    assert not Path(f'/proc/{pid}').exists(), 'child survived shutdown'
                print('PASS: shutdown with active CGI reaps child')
            except BaseException:
                print((root / 'server.log').read_text()[-5000:])
                raise
            finally:
                if proc.poll() is None:
                    proc.terminate()
                    try:
                        proc.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait()


if __name__ == '__main__':
    main()
