#!/usr/bin/env python3
"""Exercise real background HTTP/cache requests against a local fixture server.
Optional --live tests the published HTTPS endpoint; --bundled-curl tests Android's
shipped libcurl rather than the host library. No game folders are touched.
"""
import argparse
import copy
import http.server
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import time

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--live', action='store_true')
parser.add_argument('--bundled-curl', action='store_true')
args = parser.parse_args()
sample = json.loads((ROOT / 'tests/mi/catalog_sample.json').read_text())
good = json.dumps(sample).encode()
shrunk = copy.deepcopy(sample)
shrunk['mods'] = shrunk['mods'][:1]
shrunk['mod_count'] = 1


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        if self.path == '/redirect':
            self.send_response(302)
            self.send_header('Location', '/good')
            self.end_headers()
            return
        data = good
        code = 200
        if self.path == '/bad':
            data = b'{not-json'
        elif self.path == '/shrink':
            data = json.dumps(shrunk).encode()
        elif self.path == '/404':
            code = 404
        self.send_response(code)
        if self.path != '/no-length':
            self.send_header('Content-Length', str(17 * 1024 * 1024 if self.path == '/oversize' else len(data) + (100 if self.path == '/truncated' else 0)))
        self.end_headers()
        if self.path == '/slow':
            time.sleep(2)
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass
        self.close_connection = True


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    with tempfile.TemporaryDirectory(prefix='mi-request-tests-') as temp:
        directory = Path(temp)
        executable = directory / 'request-cli'
        command = [os.environ.get('CXX', 'clang++'), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
                   '-I' + str(ROOT / 'lib/curl/include'), '-I' + str(ROOT / 'src'),
                   str(ROOT / 'src/pc/mod_integrations/mi_catalog.cpp'),
                   str(ROOT / 'src/pc/mod_integrations/mi_catalog_request.cpp'),
                   str(ROOT / 'tests/mi/request_cli.cpp'), '-lz']
        environment = os.environ.copy()
        if args.bundled_curl:
            library = ROOT / 'lib/curl/android/arm64-v8a'
            command += ['-L' + str(library)]
            environment['LD_LIBRARY_PATH'] = str(library) + os.pathsep + os.environ.get('LD_LIBRARY_PATH', '')
        command += ['-lcurl', '-o', str(executable)]
        subprocess.run(command, check=True)
        base = f'http://127.0.0.1:{server.server_port}'

        def run(path, cache='cache', cancel=False, fresh=False):
            url = path if path.startswith('https://') else base + path
            cmd = [str(executable), str(directory / cache), url, 'es']
            if cancel:
                cmd.append('cancel')
            elif fresh:
                cmd.append('fresh')
            return json.loads(subprocess.check_output(cmd, env=environment, text=True, timeout=50))

        result = run('/good')
        assert result['count'] == 8 and result['error'] == 0 and result['saved'], result
        recent = run('/good', fresh=True)
        assert recent['count'] == 8 and recent['cached'] and recent['http'] == 0, recent
        assert run('/redirect')['count'] == 8
        assert run('/no-length')['count'] == 8
        before = (directory / 'cache/catalog-v2.cache').read_bytes()
        for path, error in [('/bad', 4), ('/shrink', 4), ('/404', 2), ('/truncated', 1), ('/oversize', 4)]:
            result = run(path)
            assert result['count'] == 8 and result['error'] == error and result['saw_cache'], result
            assert (directory / 'cache/catalog-v2.cache').read_bytes() == before
        (directory / 'cache/catalog-v2.cache').write_bytes(before.replace(b'MI Sample Lua', b'XX Sample Lua', 1))
        result = run('/404')
        assert result['count'] == 8 and result['saw_cache'] and result['title_is_sample'], result
        assert run('/good')['saved']
        result = run('/404', 'no-cache')
        assert result['count'] == 0 and result['error'] == 2
        (directory / 'not-a-directory').write_text('preserve')
        result = run('/good', 'not-a-directory')
        assert result['count'] == 8 and result['error'] == 6 and not result['saved'], result
        assert (directory / 'not-a-directory').read_text() == 'preserve'
        assert run('/slow', 'cancelled', cancel=True)['cancelled']
        assert not (directory / 'cancelled/catalog-v2.cache').exists()
        assert not list(directory.rglob('*.tmp-*'))
        print('MI requests: background fetch, redirect, cache, last-good, HTTP/error/truncation, shrink, storage and close passed.')
        if args.live:
            result = run('https://raw.githubusercontent.com/retired64/sm64cdpy.releases/main/db/database_sm64coopdx.json', 'live')
            assert result['count'] > 1000 and result['error'] == 0 and result['saved'], result
            assert run('/404', 'live')['count'] == result['count']
            print('Published HTTPS catalog:', result)
finally:
    server.shutdown()
    server.server_close()
