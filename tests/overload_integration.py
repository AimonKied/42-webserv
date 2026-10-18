#!/usr/bin/env python3
"""Admission, recovery and mixed-load checks. Linux also tests real EMFILE.
Usage: python3 tests/overload_integration.py ./webserv
Defaults match RuntimeLimits.hpp: 128 clients, 16 CGI jobs.
"""
from collections import Counter
import argparse
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
import http.client
from pathlib import Path
import resource
import socket
import subprocess
import sys
import tempfile
import time

CLIENT_LIMIT = 128
CGI_LIMIT = 16


def wait_for(check, seconds=3):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if check():
            return
        time.sleep(.02)
    raise AssertionError('condition did not become true before deadline')


def connect(port, path=None):
    sock = socket.create_connection(('127.0.0.1', port), timeout=4)
    sock.settimeout(8)
    if path is not None:
        sock.sendall(f'GET {path} HTTP/1.1\r\nHost: localhost\r\n\r\n'.encode())
    return sock


def receive(sock):
    response = http.client.HTTPResponse(sock)
    response.begin()
    result = response.status, dict(response.getheaders()), response.read()
    response.close()
    return result


def request(port, path='/'):
    with connect(port, path) as sock:
        return receive(sock)


def fd_count(process):
    return len(list(Path(f'/proc/{process.pid}/fd').iterdir()))


def children(process):
    return Path(f'/proc/{process.pid}/task/{process.pid}/children').read_text().split()


@contextmanager
def server(binary, root, nofile=None):
    holders = [socket.socket(), socket.socket()]
    for sock in holders:
        sock.bind(('127.0.0.1', 0))
    ports = [sock.getsockname()[1] for sock in holders]
    config = root / 'test.conf'
    config.write_text('\n'.join(f'''server {{
 listen 127.0.0.1:{port}; server_name localhost; client_max_body_size 1048576;
 root {root}; location / {{ index index.html; allow_methods GET POST; cgi_extension .py; }}
}}''' for port in ports))
    for sock in holders:
        sock.close()
    def set_limit():
        resource.setrlimit(resource.RLIMIT_NOFILE, (nofile, nofile))
    with (root / 'server.log').open('w') as log:
        process = subprocess.Popen([binary, str(config)], stdout=log, stderr=log,
                                   preexec_fn=set_limit if nofile else None)
        try:
            def started():
                assert process.poll() is None, 'server exited during startup'
                try:
                    return request(ports[0])[0] == 200
                except ConnectionRefusedError:
                    return False
            wait_for(started)
            yield process, ports
        except BaseException:
            print((root / 'server.log').read_text()[-4000:])
            raise
        finally:
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                raise AssertionError('shutdown timed out')
            assert process.returncode == 0, process.returncode


def fixtures(root):
    (root / 'index.html').write_text('static OK')
    (root / 'large.bin').write_bytes(b'x' * (256 * 1024))
    (root / 'echo.py').write_text('print("Content-Type: text/plain\\n\\nCGI OK")\n')
    (root / 'hold.py').write_text('''import os, time
from pathlib import Path
Path('started-' + os.environ['QUERY_STRING']).write_text(str(os.getpid()))
while not Path('release').exists(): time.sleep(.01)
print('Content-Type: text/plain\\n\\nreleased')
''')


def cgi_overload(process, ports, root):
    linux = Path(f'/proc/{process.pid}/fd').exists()
    baseline = fd_count(process) if linux else None
    for phase in ('complete', 'cancel', 'timeout'):
        active = []
        try:
            for i in range(CGI_LIMIT):
                active.append(connect(ports[i % 2], f'/hold.py?{phase}-{i}'))
            wait_for(lambda: all((root / f'started-{phase}-{i}').exists() for i in range(CGI_LIMIT)))
            if linux:
                assert len(children(process)) == CGI_LIMIT
            for port in ports:
                code, headers, _ = request(port, '/echo.py')
                assert code == 503 and headers.get('Retry-After') == '1', (code, headers)
                assert request(port)[::2] == (200, b'static OK')
            if phase == 'complete':
                (root / 'release').touch()
                for sock in active:
                    assert receive(sock)[0] == 200
                (root / 'release').unlink()
            elif phase == 'timeout':
                for sock in active:
                    assert receive(sock)[0] == 504
        finally:
            for sock in active:
                sock.close()
        if linux:
            wait_for(lambda: not children(process) and fd_count(process) == baseline)
        wait_for(lambda: request(ports[0], '/echo.py')[0] == 200)
        print(f'PASS: CGI cap/503, static availability and capacity recovery after {phase}')


def clients_overload(process, ports):
    idle = []
    pending = None
    linux = Path(f'/proc/{process.pid}/fd').exists()
    baseline = fd_count(process) if linux else None
    try:
        for i in range(CLIENT_LIMIT):
            idle.append(connect(ports[i % 2]))
        if linux:
            wait_for(lambda: fd_count(process) == baseline + CLIENT_LIMIT)
        pending = connect(ports[1], '/')
        pending.settimeout(.25)
        try:
            data = pending.recv(1)
            raise AssertionError(f'client beyond admission cap was served: {data!r}')
        except socket.timeout:
            pass
        if linux:
            assert fd_count(process) == baseline + CLIENT_LIMIT
        # An existing client can still complete and free a slot at capacity.
        idle[0].sendall(b'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n')
        assert receive(idle[0])[0] == 200
        pending.settimeout(4)
        assert receive(pending)[0] == 200
    finally:
        if pending:
            pending.close()
        for sock in idle:
            sock.close()
    if linux:
        wait_for(lambda: fd_count(process) == baseline)
    for port in ports:
        assert request(port)[0] == 200
    print('PASS: client cap pauses acceptance, existing work continues, listeners resume')


def mixed_load(process, ports, requests, workers):
    def operation(index):
        path = ('/', '/echo.py', '/large.bin', '/missing')[index % 4]
        code, _, body = request(ports[(index // 4) % 2], path)
        if path == '/echo.py':
            assert code in (200, 503)
            if code == 200:
                assert body == b'CGI OK\n'
        elif path == '/missing':
            assert code == 404
        else:
            assert code == 200
            assert body == (b'static OK' if path == '/' else b'x' * (256 * 1024))
        return code
    linux = Path(f'/proc/{process.pid}/fd').exists()
    baseline = fd_count(process) if linux else None
    started = time.monotonic()
    with ThreadPoolExecutor(max_workers=workers) as pool:
        counts = Counter(pool.map(operation, range(requests)))
    assert process.poll() is None
    if linux:
        wait_for(lambda: not children(process) and fd_count(process) == baseline)
    print(f'PASS: {requests} mixed requests / {workers} workers in '
          f'{time.monotonic() - started:.2f}s, response integrity and cleanup: {dict(counts)}')


def exhausted_descriptors(binary, root):
    if not sys.platform.startswith('linux'):
        print('SKIP: real descriptor-exhaustion check requires Linux /proc')
        return
    with server(binary, root, nofile=32) as (process, ports):
        baseline = fd_count(process)
        idle = []
        pending = None
        try:
            for _ in range(32 - baseline):
                idle.append(connect(ports[0]))
            wait_for(lambda: fd_count(process) == 32)
            pending = connect(ports[1])
            log = root / 'server.log'
            wait_for(lambda: 'Accept paused after error' in log.read_text())
            before = log.read_text().count('Accept paused after error')
            time.sleep(.8)
            after = log.read_text().count('Accept paused after error')
            assert 1 <= after - before <= 5, 'accept retry is spinning or not recovering'
            assert process.poll() is None
            # Already accepted client requests CGI with no descriptors available
            # for pipes. This must be 503, and must not terminate the server.
            idle[0].sendall(b'GET /echo.py HTTP/1.1\r\nHost: localhost\r\n\r\n')
            assert receive(idle[0])[0] == 503
            for sock in idle[:8]:
                sock.close()
            wait_for(lambda: fd_count(process) < 27)
            pending.sendall(b'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n')
            assert receive(pending)[0] == 200
            assert request(ports[0], '/echo.py')[0] == 200
        finally:
            if pending:
                pending.close()
            for sock in idle:
                sock.close()
        wait_for(lambda: fd_count(process) == baseline and not children(process))
        print('PASS: real EMFILE accept cooldown, pipe-failure 503 and recovery under RLIMIT_NOFILE=32')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', nargs='?', default='./webserv')
    parser.add_argument('--requests', type=int, default=600)
    parser.add_argument('--workers', type=int, default=32)
    args = parser.parse_args()
    if args.requests < 1 or args.workers < 1:
        parser.error('requests and workers must be positive')
    binary = str(Path(args.binary).resolve())
    with tempfile.TemporaryDirectory(prefix='webserv-overload-') as temporary:
        root = Path(temporary)
        fixtures(root)
        with server(binary, root) as (process, ports):
            cgi_overload(process, ports, root)
            clients_overload(process, ports)
            mixed_load(process, ports, args.requests, args.workers)
        exhausted_descriptors(binary, root)


if __name__ == '__main__':
    main()
