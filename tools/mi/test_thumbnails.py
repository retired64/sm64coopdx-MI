#!/usr/bin/env python3
"""Bounded thumbnail HTTP/decode/cache tests. Synthetic images, temporary roots."""
import argparse
import hashlib
import http.server
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import threading
import time
import urllib.request
import zlib

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--bundled-curl', action='store_true')
parser.add_argument('--live', action='store_true')
args = parser.parse_args()


def chunk(kind, data):
    return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data))


def png(width=2, height=2):
    data = (b'\0' + b'\xff\0\0\xff' * width) * height
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!IIBBBBB', width, height, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(data)) + chunk(b'IEND', b'')


counts = {}


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        counts[self.path] = counts.get(self.path, 0) + 1
        data = png()
        status = 200
        if self.path == '/404':
            status = 404
        elif self.path == '/bad':
            data = b'<html>error</html>'
        elif self.path == '/corrupt':
            data = b'\x89PNG\r\n\x1a\nbad'
        elif self.path == '/huge':
            data = png(1025, 1)
        self.send_response(status)
        self.send_header('Content-Length', str(3 * 1024 * 1024 if self.path == '/oversize' else len(data) + (10 if self.path == '/truncated' else 0)))
        self.end_headers()
        if self.path == '/slow':
            time.sleep(1)
        try:
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass
        self.close_connection = True


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    with tempfile.TemporaryDirectory(prefix='mi-thumbnails-') as temp:
        directory = Path(temp)
        texture_test = directory / 'texture-test'
        subprocess.run([os.environ.get('CC', 'clang'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                        '-I' + str(ROOT / 'tests/mi/stubs'), '-I' + str(ROOT / 'src'),
                        str(ROOT / 'src/pc/mod_integrations/mi_thumbnail_texture.c'),
                        str(ROOT / 'tests/mi/thumbnail_texture_test.c'), '-o', str(texture_test)], check=True)
        subprocess.run([str(texture_test)], check=True)
        executable = directory / 'thumbnail-cli'
        command = [os.environ.get('CXX', 'clang++'), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
                   '-I' + str(ROOT / 'lib/curl/include'), '-I' + str(ROOT / 'include'), '-I' + str(ROOT / 'src'),
                   str(ROOT / 'src/pc/mod_integrations/mi_thumbnail.cpp'), str(ROOT / 'tests/mi/thumbnail_cli.cpp')]
        env = os.environ.copy()
        if args.bundled_curl:
            library = ROOT / 'lib/curl/android/arm64-v8a'
            command += ['-L' + str(library)]
            env['LD_LIBRARY_PATH'] = str(library) + os.pathsep + os.environ.get('LD_LIBRARY_PATH', '')
        command += ['-lcurl', '-o', str(executable)]
        subprocess.run(command, check=True)
        base = f'http://127.0.0.1:{server.server_port}'

        def run(path, name='game', cancel=False):
            game = directory / name
            game.mkdir(exist_ok=True)
            url = path if path.startswith('https://') else base + path
            command = [str(executable), str(game), url]
            if cancel:
                command.append('close')
            return json.loads(subprocess.check_output(command, env=env, text=True, timeout=40))

        result = run('/good')
        assert result['state'] == 1 and result['pixels'], result
        assert result['sha256'] == hashlib.sha256(b'\xff\0\0\xff' * 128 * 128).hexdigest(), result
        assert run('/good')['pixels'] and counts['/good'] == 1
        cache = next((directory / 'game/mi/thumbnails').glob('*.rgba'))
        cache.write_bytes(b'corrupt')
        assert run('/good')['pixels'] and counts['/good'] == 2
        os.utime(cache, (time.time() - 8 * 86400, time.time() - 8 * 86400))
        assert run('/good')['pixels'] and counts['/good'] == 3
        cache_dir = directory / 'game/mi/thumbnails'
        (cache_dir / 'preserve.txt').write_text('preserve')
        for number in range(65):
            old = cache_dir / (f'{number:064x}' + '.rgba')
            old.write_bytes(b'old')
            os.utime(old, (1, 1))
        assert run('/evict')['pixels']
        assert len(list(cache_dir.glob('*.rgba'))) == 64
        assert (cache_dir / 'preserve.txt').read_text() == 'preserve'
        for path in ['/bad', '/404', '/corrupt', '/huge', '/oversize', '/truncated']:
            result = run(path)
            assert result['state'] == 2 and not result['pixels'], (path, result)
        blocked = directory / 'blocked'
        blocked.mkdir()
        (blocked / 'mi').write_text('preserve')
        assert run('/good', 'blocked')['pixels']
        assert (blocked / 'mi').read_text() == 'preserve'
        assert run('/slow', 'cancel', True)['closed']
        assert not list(directory.rglob('*.tmp-*'))
        assert not list(directory.rglob('mods'))
        if args.live:
            with urllib.request.urlopen('https://raw.githubusercontent.com/retired64/sm64cdpy.releases/r2-phase6-flutter-fallback/assets/db/database_sm64coopdx.json', timeout=40) as response:
                catalog = json.load(response)
            urls = [m.get('image_url') or m.get('icon_url') for m in catalog['mods']]
            url = next(u for u in urls if u and u.startswith('https://'))
            result = run(url, 'live')
            assert result['pixels'], result
            print('Published catalog HTTPS thumbnail decoded and cached:', result['sha256'])
        print('MI thumbnails: RGBA resize, bounded HTTP, corruption, dimensions, cache, optional storage, close/queue churn passed.')
finally:
    server.shutdown()
    server.server_close()
