#!/usr/bin/env python3
"""Multi-location HTTP/CGI regression tests; fixtures and logs stay in /tmp.
Usage: python3 tests/routing_integration.py ./webserv
"""
from contextlib import contextmanager
import http.client
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time


@contextmanager
def server(binary, directory, locations):
    with socket.socket() as reservation:
        reservation.bind(('127.0.0.1', 0))
        port = reservation.getsockname()[1]
    config = directory / 'test.conf'
    config.write_text(f'''server {{
 listen 127.0.0.1:{port};
 server_name localhost;
 client_max_body_size 1048576;
 root {directory / 'primary'};
 index index.html;
 {locations}
}}
''')
    with (directory / 'server.log').open('w') as log:
        process = subprocess.Popen([binary, str(config)], stdout=log, stderr=log)
        try:
            for _ in range(100):
                if process.poll() is not None:
                    raise AssertionError('server exited during startup')
                try:
                    with socket.create_connection(('127.0.0.1', port), timeout=1):
                        break
                except ConnectionRefusedError:
                    time.sleep(.02)
            else:
                raise AssertionError('startup timed out')
            yield port
        except BaseException:
            print((directory / 'server.log').read_text()[-3000:])
            raise
        finally:
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                raise AssertionError('shutdown timed out')


def request(port, path, method='GET', body=None):
    connection = http.client.HTTPConnection('127.0.0.1', port, timeout=8)
    try:
        connection.request(method, path, body=body)
        response = connection.getresponse()
        return response.status, dict(response.getheaders()), response.read()
    finally:
        connection.close()


def main():
    binary = str(Path(sys.argv[1] if len(sys.argv) > 1 else './webserv').resolve())
    with tempfile.TemporaryDirectory(prefix='webserv-routing-test-') as temporary:
        directory = Path(temporary)
        primary = directory / 'primary'
        alternate = directory / 'alternate'
        for root in (primary, alternate):
            for subdir in ('cgi', 'static', 'error'):
                (root / subdir).mkdir(parents=True)
            (root / 'index.html').write_text(root.name)
            (root / 'static' / 'file.txt').write_text(root.name)
            (root / 'staticity').write_text('boundary fallback')
            (root / 'cgi' / 'echo.py').write_text(
                f'print("Content-Type: text/plain\\n\\n{root.name} executed")\n')
            for code in (404, 502, 504):
                (root / 'error' / f'{code}.html').write_text(f'{root.name} {code}')
        (alternate / 'cgi' / 'bad.py').write_text('print("invalid CGI headers")\n')
        (alternate / 'cgi' / 'fail.py').write_text('raise SystemExit(1)\n')
        (alternate / 'cgi' / 'slow.py').write_text('import time\ntime.sleep(30)\n')
        # Redirect must not execute a script or create a file, even for POST.
        (alternate / 'cgi' / 'redirect.py').write_text('raise RuntimeError("must not execute")\n')

        specific = [
            f'location /cgi {{ root {alternate}; allow_methods GET; cgi_extension .py; }}',
            f'location /static {{ root {alternate}; allow_methods GET; }}',
            'location /redirect { allow_methods GET POST; return 302 /static/file.txt; }',
            'location /cgi/redirect.py { allow_methods GET; cgi_extension .py; return 307 /static/file.txt; }',
            'location /unsupported { allow_methods GET; cgi_extension .php; }',
        ]
        for root_cgi in (False, True):
            root_location = ('location / { allow_methods GET POST; ' +
                             ('cgi_extension .py;' if root_cgi else '') + ' }')
            for reversed_order in (False, True):
                locations = [root_location] + specific
                if reversed_order:
                    locations.reverse()
                with server(binary, directory, '\n'.join(locations)) as port:
                    code, _, body = request(port, '/cgi/echo.py')
                    assert (code, body) == (200, b'alternate executed\n'), (code, body)
                    code, headers, _ = request(port, '/cgi/echo.py', 'POST', 'hello')
                    assert code == 405 and headers.get('Allow') == 'GET', (code, headers)
                    code, _, body = request(port, '/static/file.txt')
                    assert (code, body) == (200, b'alternate')
                    code, headers, _ = request(port, '/static/file.txt', 'POST', 'must not overwrite')
                    assert code == 405 and headers.get('Allow') == 'GET'
                    assert (alternate / 'static' / 'file.txt').read_text() == 'alternate'
                    assert request(port, '/staticity')[2] == b'boundary fallback'
                    for method in ('GET', 'POST'):
                        code, headers, _ = request(port, '/redirect', method)
                        assert code == 302 and headers.get('Location') == '/static/file.txt'
                    assert not (primary / 'redirect').exists()
                    code, headers, _ = request(port, '/cgi/redirect.py')
                    assert code == 307 and headers.get('Location') == '/static/file.txt'
                    assert request(port, '/cgi/redirect.py', 'POST')[0] == 405
                    assert request(port, '/unsupported/file.php')[0] == 501
                    assert request(port, '/cgi/missing.py')[::2] == (404, b'alternate 404')
                    # Errors created after execution must retain the matched location.
                    assert request(port, '/cgi/bad.py')[::2] == (502, b'alternate 502')
                    assert request(port, '/cgi/fail.py')[::2] == (502, b'alternate 502')
                    if not root_cgi and not reversed_order:
                        assert request(port, '/cgi/slow.py')[::2] == (504, b'alternate 504')
                print(f'PASS: dedicated CGI, permissions, roots, redirects and error context '
                      f'(root CGI={root_cgi}, reverse order={reversed_order})')
        with server(binary, directory, specific[0]) as port:
            assert request(port, '/not-a-route')[0] == 404
            assert request(port, '/cgi/echo.py')[2] == b'alternate executed\n'
        print('PASS: missing location returns 404 even when first location enables CGI')


if __name__ == '__main__':
    main()
