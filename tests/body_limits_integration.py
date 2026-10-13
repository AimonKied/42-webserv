#!/usr/bin/env python3
"""Configured body limits on independent listeners; temp fixtures only.
Usage: python3 tests/body_limits_integration.py ./webserv
"""
import http.client
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time


def exchange(port, headers, body=b'', fragments=False):
    with socket.create_connection(('127.0.0.1', port), timeout=10) as sock:
        sock.settimeout(10)
        sock.sendall(b'POST /size.py HTTP/1.1\r\nHost: localhost\r\n' + headers + b'\r\n')
        if fragments and body:
            sock.sendall(body[:-1])
            sock.settimeout(.05)
            try:
                received = sock.recv(1)
                raise AssertionError(f'response before final body byte: {received!r}')
            except socket.timeout:
                pass
            sock.settimeout(10)
            sock.sendall(body[-1:])
        elif body:
            sock.sendall(body)
        response = http.client.HTTPResponse(sock)
        response.begin()
        return response.status, response.read()


def fixed(port, size, fragments=False):
    return exchange(port, f'Content-Length: {size}\r\n'.encode(), b'x' * size, fragments)


def chunked(port, size):
    chunks = []
    remaining = size
    while remaining:
        length = min(remaining, 16384)
        chunks.append(f'{length:x}\r\n'.encode() + b'x' * length + b'\r\n')
        remaining -= length
    chunks.append(b'0\r\n\r\n')
    return exchange(port, b'Transfer-Encoding: chunked\r\n', b''.join(chunks), True)


def main():
    binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else './webserv').resolve())
    with tempfile.TemporaryDirectory(prefix='webserv-body-limits-') as temporary:
        root = Path(temporary)
        (root / 'index.html').write_text('body limits')
        (root / 'size.py').write_text('import sys\nbody = sys.stdin.buffer.read()\nprint("Content-Type: text/plain\\n")\nprint(len(body))\n')
        limits = [0, 1024 * 1024, 2 * 1024 * 1024]
        reservations = [socket.socket() for _ in limits]
        for sock in reservations:
            sock.bind(('127.0.0.1', 0))
        ports = [sock.getsockname()[1] for sock in reservations]
        config = root / 'test.conf'
        config.write_text('\n'.join(f'''server {{
 listen 127.0.0.1:{port}; server_name localhost; client_max_body_size {limit}; root {root};
 location / {{ index index.html; allow_methods GET POST; cgi_extension .py; }}
}}''' for port, limit in zip(ports, limits)))
        for sock in reservations:
            sock.close()
        with (root / 'server.log').open('w') as log:
            process = subprocess.Popen([binary, str(config)], stdout=log, stderr=log)
            try:
                for _ in range(100):
                    if process.poll() is not None:
                        raise AssertionError('server exited on startup')
                    try:
                        with socket.create_connection(('127.0.0.1', ports[0]), timeout=1):
                            break
                    except ConnectionRefusedError:
                        time.sleep(.02)
                else:
                    raise AssertionError('startup timed out')
                for port, limit in zip(ports, limits):
                    for size in sorted({0, max(0, limit - 1), limit}):
                        assert fixed(port, size, True) == (200, f'{size}\n'.encode())
                    assert chunked(port, limit) == (200, f'{limit}\n'.encode())
                    # Oversized declarations are rejected without waiting for data.
                    assert exchange(port, f'Content-Length: {limit + 1}\r\n'.encode())[0] == 413
                    assert exchange(port, b'Transfer-Encoding: chunked\r\n', f'{limit + 1:x}\r\n'.encode())[0] == 413
                    assert fixed(port, 0) == (200, b'0\n')
                    print(f'PASS: body limit {limit}: exact/below/above, fragmented input, chunked and early rejection')
                # The same declared size succeeds on one listener and fails on another.
                assert exchange(ports[1], b'Content-Length: 2097152\r\n')[0] == 413
                assert fixed(ports[2], 2097152) == (200, b'2097152\n')
                print('PASS: per-listener limits above one MiB are honored')
            except BaseException:
                print((root / 'server.log').read_text()[-3000:])
                raise
            finally:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
                    raise AssertionError('shutdown timed out')
            assert process.returncode == 0


if __name__ == '__main__':
    main()
