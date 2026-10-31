#!/usr/bin/env python3
"""Native installer integration tests; only temporary game roots are used."""
import argparse
import hashlib
import http.server
import io
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import time
import zipfile
import urllib.request

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--bundled-curl', action='store_true')
parser.add_argument('--live', action='store_true', help='Install a real catalog Lua into a temporary root over HTTPS')
parser.add_argument('--storage-root', help='Use a temporary game root under this filesystem; executables stay in internal temp')
args = parser.parse_args()
LUA = b'-- name: Fixture\nprint("fixture")\n'


def archive(entries):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, 'w', zipfile.ZIP_DEFLATED) as output:
        for path, data in entries:
            output.writestr(path, data)
    return buffer.getvalue()


link = zipfile.ZipInfo('link')
link.create_system = 3
link.external_attr = 0o120777 << 16
PAYLOADS = {
    '/lua': LUA, '/slow': LUA, '/stream': LUA + b'-- padding\n' * 16384,
    '/root': archive([('main.lua', LUA), ('data/a.luac', b'\x1bLua')]),
    '/wrapper': archive([('outer/mod/main.lua', LUA), ('outer/mod/readme.txt', b'hi')]),
    '/single': archive([('boo2.lua', LUA)]),
    '/traversal': archive([('../outside.lua', LUA), ('main.lua', LUA)]),
    '/symlink': archive([(link, 'elsewhere'), ('main.lua', LUA)]),
    '/duplicate': archive([('main.lua', LUA), ('MAIN.LUA', LUA)]),
    '/ambiguous': archive([('one.lua', LUA), ('two.lua', LUA)]),
    '/html': b'<!DOCTYPE html><html>error</html>',
    '/corrupt': b'PK\x03\x04corrupt', '/truncated': LUA,
}


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        data = PAYLOADS.get(self.path, b'not found')
        self.send_response(200 if self.path in PAYLOADS else 404)
        self.send_header('Content-Length', str(len(data) + (100 if self.path == '/truncated' else 0)))
        self.end_headers()
        if self.path == '/slow':
            time.sleep(1)
        try:
            if self.path == '/stream':
                for offset in range(0, len(data), 4096):
                    self.wfile.write(data[offset:offset + 4096])
                    self.wfile.flush()
                    time.sleep(0.01)
            else:
                self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass
        self.close_connection = True


server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    with tempfile.TemporaryDirectory(prefix='mi-install-tests-') as temp, tempfile.TemporaryDirectory(prefix='mi-install-data-', dir=args.storage_root) as data_root:
        directory = Path(temp)
        games = Path(data_root)
        miniz = directory / 'miniz.o'
        subprocess.run([os.environ.get('CC', 'clang'), '-I' + str(ROOT / 'src'), '-c', str(ROOT / 'src/pc/utils/miniz/miniz.c'), '-o', str(miniz)], check=True)
        executable = directory / 'install-cli'
        command = [os.environ.get('CXX', 'clang++'), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
                   '-I' + str(ROOT / 'lib/curl/include'), '-I' + str(ROOT / 'src'),
                   str(ROOT / 'src/pc/mod_integrations/mi_install.cpp'), str(ROOT / 'tests/mi/install_cli.cpp'), str(miniz)]
        environment = os.environ.copy()
        if args.bundled_curl:
            library = ROOT / 'lib/curl/android/arm64-v8a'
            command += ['-L' + str(library)]
            environment['LD_LIBRARY_PATH'] = str(library) + os.pathsep + os.environ.get('LD_LIBRARY_PATH', '')
        command += ['-lcurl', '-o', str(executable)]
        subprocess.run(command, check=True)

        def run(path, filename='test.zip', name=None, mode='normal', digest=''):
            game = games / (name or path.strip('/'))
            game.mkdir(exist_ok=True)
            result = json.loads(subprocess.check_output([str(executable), str(game), f'http://127.0.0.1:{server.server_port}' + path, filename, mode, digest], text=True, env=environment, timeout=40))
            assert not list((game / 'mi').glob('stage-*')), result
            return game, result

        # Defense in depth: unsupported hosts never connect, even with a fallback supplied.
        for index, url in enumerate(['https://mods.sm64coopdx.com/test.lua',
                                     'https://mods-cdn.sm64cdpy.org.evil.test/test.lua',
                                     'http://mods-cdn.sm64cdpy.org/test.lua']):
            game = games / ('blocked-source-' + str(index))
            game.mkdir()
            result = json.loads(subprocess.check_output([str(executable), str(game), url, 'test.lua',
                'normal', '', f'http://127.0.0.1:{server.server_port}/lua'], text=True, env=environment, timeout=10))
            assert result['state'] == 6 and result['error'] == 6 and not result['saw_download'], result
            assert not list((game / 'mi').glob('stage-*')), result
        game = games / 'no-fallback'
        game.mkdir()
        result = json.loads(subprocess.check_output([str(executable), str(game),
            f'http://127.0.0.1:{server.server_port}/404', 'test.lua', 'normal', '',
            f'http://127.0.0.1:{server.server_port}/lua'], text=True, env=environment, timeout=10))
        assert result['state'] == 6 and result['error'] == 2 and result['http'] == 404, result

        for path, filename, destination in [('/lua', 'boo2.lua', 'boo2.lua'), ('/root', 'test.zip', 'test'), ('/wrapper', 'test.zip', 'test'), ('/single', 'test.zip', 'boo2.lua')]:
            game, result = run(path, filename)
            assert result['state'] == 5 and result['destination'] == destination, result
            receipts = list((game / 'mi/receipts').glob('*.json'))
            assert len(receipts) == 1
            receipt = json.loads(receipts[0].read_text())
            for entry in receipt['files']:
                target = game / 'mods' / destination
                if entry['path']:
                    target /= entry['path']
                assert hashlib.sha256(target.read_bytes()).hexdigest() == entry['sha256']
            _, again = run(path, filename)
            assert again['state'] == 5, again
        for path, filename, error in [('/traversal', 'test.zip', 6), ('/symlink', 'test.zip', 6), ('/duplicate', 'test.zip', 6), ('/ambiguous', 'test.zip', 5), ('/html', 'test.lua', 5), ('/corrupt', 'test.zip', 5), ('/404', 'test.lua', 2), ('/truncated', 'test.lua', 1)]:
            game, result = run(path, filename)
            assert result['state'] == 6 and result['error'] == error, (path, result)
            assert not list((game / 'mods').iterdir())
        _, result = run('/lua', 'check.lua', 'hash-bad', digest='0' * 64)
        assert result['error'] == 10, result
        _, result = run('/lua', 'check.lua', 'hash-good', digest=hashlib.sha256(LUA).hexdigest())
        assert result['state'] == 5, result
        game = games / 'collision'
        (game / 'mods').mkdir(parents=True)
        (game / 'mods/test.lua').write_bytes(b'preserve')
        _, result = run('/lua', 'test.lua', 'collision')
        assert result['error'] == 8 and (game / 'mods/test.lua').read_bytes() == b'preserve', result
        _, result = run('/slow', 'test.lua', 'cancel', mode='cancel')
        assert result['state'] == 8, result
        _, result = run('/slow', 'test.lua', 'busy', mode='busy')
        assert result['state'] == 5, result
        _, result = run('/stream', 'stream.lua', 'progress')
        assert result['state'] == 5 and result['saw_download'] and result['max_bytes'] > 4096, result
        # Verify cross-process exclusion and lock release on the tested filesystem.
        import fcntl
        game = games / 'process-lock'
        (game / 'mi').mkdir(parents=True)
        with (game / 'mi/install.lock').open('w') as lock:
            fcntl.lockf(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            _, result = run('/lua', 'test.lua', game.name)
            assert result['error'] == 9, result
        _, result = run('/lua', 'test.lua', game.name)
        assert result['state'] == 5, result
        # A modified installed file must never be silently called the original installation.
        game, _ = run('/lua', 'test.lua', 'modified')
        (game / 'mods/test.lua').write_bytes(b'changed')
        _, result = run('/lua', 'test.lua', 'modified')
        assert result['error'] == 11, result
        # Recover a crash between atomic delivery and receipt publication.
        for delivered in [False, True]:
            game = games / ('recover-delivered' if delivered else 'recover-staged')
            mi = game / 'mi'
            (mi / 'transactions').mkdir(parents=True)
            (mi / 'receipts').mkdir()
            (game / 'mods').mkdir()
            stage = mi / 'stage-recovery'
            stage.mkdir()
            target = (game / 'mods/recovered.lua') if delivered else (stage / 'download')
            target.write_bytes(LUA)
            key = hashlib.sha256(b'old-mod\nv0\nold-file').hexdigest()
            receipt = {'schema': 1, 'mod_id': 'old-mod', 'version_id': 'v0', 'file_id': 'old-file', 'version': '0', 'destination': 'recovered.lua', 'files': [{'path': '', 'size': len(LUA), 'sha256': hashlib.sha256(LUA).hexdigest()}], 'installed_at': 1}
            stat = target.stat()
            journal = {'schema': 1, 'key': key, 'stage': stage.name, 'candidate': 'download', 'inode': stat.st_ino, 'device': stat.st_dev, 'receipt': receipt}
            (mi / 'transactions/stage-recovery.json').write_text(json.dumps(journal))
            _, result = run('/lua', 'new.lua', game.name)
            assert result['state'] == 5, result
            assert not list((mi / 'transactions').iterdir())
            assert (mi / 'receipts' / (key + '.json')).exists() == delivered
            assert (game / 'mods/recovered.lua').exists() == delivered
        # An unknown journal must stop safely, preserving evidence and local mods.
        game = games / 'bad-journal'
        (game / 'mi/transactions').mkdir(parents=True)
        journal = game / 'mi/transactions/bad.json'
        journal.write_text('{invalid')
        _, result = run('/lua', 'test.lua', game.name)
        assert result['error'] == 11 and journal.read_text() == '{invalid', result
        if args.live:
            with urllib.request.urlopen('https://raw.githubusercontent.com/retired64/sm64cdpy.releases/r2-phase6-flutter-fallback/assets/db/database_sm64coopdx.json', timeout=40) as response:
                catalog = json.load(response)
            sources = [f for m in catalog['mods'] for v in m.get('versions', []) if v.get('resolved') for f in v.get('files', []) if f.get('filename', '').endswith('.lua') and not f.get('requires_primary') and f.get('mirror_status') == 'available' and f.get('mirror_url', '').startswith('https://mods-cdn.sm64cdpy.org/')]
            assert sources, 'No standalone Lua fixture available in live catalog'
            source = min(sources, key=lambda f: f.get('mirror_size_bytes') or 1000000000)
            game = games / 'https-live'
            game.mkdir()
            result = json.loads(subprocess.check_output([str(executable), str(game), source['mirror_url'], source['filename'], 'normal', source.get('sha256') or ''], env=environment, text=True, timeout=60))
            assert result['state'] == 5, result
            print('Real catalog HTTPS Lua installed in temporary root:', source['filename'], result)
        print('MI installer: SHA vectors, Lua/ZIP/wrappers, receipts, idempotence, unsafe paths/links/duplicates, corruption, HTTP/truncation, hash, collision, cancel, busy, local modification and interrupted-install recovery passed.')
finally:
    server.shutdown()
    server.server_close()
