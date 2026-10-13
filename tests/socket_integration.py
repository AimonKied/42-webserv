#!/usr/bin/env python3
"""Socket lifecycle/backpressure regression checks. No repository fixtures change.
Usage: python3 tests/socket_integration.py ./webserv
"""
import concurrent.futures
import http.client
from pathlib import Path
import socket
import struct
import subprocess
import sys
import tempfile
import time


def get(port):
    connection = http.client.HTTPConnection('127.0.0.1', port, timeout=3)
    try:
        connection.request('GET', '/')
        response = connection.getresponse()
        assert response.status == 200
        assert response.read() == b'still serving'
    finally:
        connection.close()


def reset(sock):
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack('ii', 1, 0))
    sock.close()


def main():
    binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else './webserv').resolve())
    with tempfile.TemporaryDirectory(prefix='webserv-sockets-') as temporary:
        root = Path(temporary)
        (root / 'index.html').write_text('still serving')
        payload = bytes(range(256)) * (32 * 1024)  # 8 MiB, larger than socket buffers
        (root / 'large.bin').write_bytes(payload)
        reservations = [socket.socket(), socket.socket()]
        for sock in reservations:
            sock.bind(('127.0.0.1', 0))
        ports = [sock.getsockname()[1] for sock in reservations]
        config = root / 'test.conf'
        config.write_text('\n'.join(f'''server {{
 listen 127.0.0.1:{port}; server_name localhost;
 client_max_body_size 1048576; root {root};
 location / {{ index index.html; allow_methods GET; }}
}}''' for port in ports))
        for sock in reservations:
            sock.close()
        with (root / 'server.log').open('w') as log:
            process = subprocess.Popen([binary, str(config)], stdout=log, stderr=log)
            try:
                for _ in range(100):
                    if process.poll() is not None:
                        raise AssertionError('server exited during startup')
                    try:
                        for port in ports:
                            get(port)
                        break
                    except ConnectionRefusedError:
                        time.sleep(.02)
                else:
                    raise AssertionError('startup timed out')
                descriptor_path = Path(f'/proc/{process.pid}/fd')
                baseline = len(list(descriptor_path.iterdir())) if descriptor_path.exists() else None
                with socket.create_connection(('127.0.0.1', ports[0]), timeout=3) as client:
                    client.sendall(b'GET / HTTP/1.1\r\nHost: local')
                    client.settimeout(.1)
                    try:
                        early = client.recv(1)
                        raise AssertionError(f'premature response or disconnect: {early!r}')
                    except socket.timeout:
                        pass
                    get(ports[1])
                    client.sendall(b'host\r\n\r\n')
                    client.settimeout(3)
                    response = http.client.HTTPResponse(client)
                    response.begin()
                    assert response.status == 200 and response.read() == b'still serving'
                print('PASS: fragmented request survives read gaps while the other listener serves')

                for port in ports:
                    with socket.socket() as slow:
                        slow.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
                        slow.settimeout(10)
                        slow.connect(('127.0.0.1', port))
                        slow.sendall(b'GET /large.bin HTTP/1.1\r\nHost: localhost\r\n\r\n')
                        # Stop receiving so the server fills its send buffer and
                        # must wait for a later POLLOUT rather than block.
                        time.sleep(.25)
                        start = time.monotonic()
                        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
                            list(pool.map(get, ports * 8))
                        assert time.monotonic() - start < 3, 'slow reader stalled other clients'
                        response = http.client.HTTPResponse(slow)
                        response.begin()
                        assert response.status == 200
                        assert int(response.getheader('Content-Length')) == len(payload)
                        assert response.read() == payload, 'partial sends lost/duplicated bytes'
                    print(f'PASS: backpressure and complete 8-MiB response on listener {port}')

                for port in ports:
                    # Reset during an unfinished request and during a large send.
                    for _ in range(8):
                        client = socket.create_connection(('127.0.0.1', port), timeout=3)
                        client.sendall(b'GET / HTTP/1.1\r\nHost:')
                        reset(client)
                        client = socket.create_connection(('127.0.0.1', port), timeout=3)
                        client.sendall(b'GET /large.bin HTTP/1.1\r\nHost: localhost\r\n\r\n')
                        assert client.recv(1024), 'large response did not start'
                        reset(client)
                    for other in ports:
                        get(other)
                assert process.poll() is None, 'reset client killed the server'
                if baseline is not None:
                    for _ in range(100):
                        if len(list(descriptor_path.iterdir())) == baseline:
                            break
                        time.sleep(.02)
                    else:
                        raise AssertionError('client descriptors were not released')
                print('PASS: repeated resets during reads/writes; server survives and descriptors recover')
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
                    raise AssertionError('server did not stop')
            assert process.returncode == 0, process.returncode
            print('PASS: clean shutdown')


if __name__ == '__main__':
    main()
