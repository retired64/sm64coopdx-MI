#!/usr/bin/env python3
"""End-to-end native MI maintenance contracts, isolated roots and fault injection.

No ROM, live catalog, real mod folder or production fault hook is used.
--storage-root creates and deletes its own temporary child on that filesystem.
"""
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

from native_sqlite import build_sqlite

ROOT = Path(__file__).resolve().parents[2]


def archive(entries):
    data = io.BytesIO()
    with zipfile.ZipFile(data, 'w', zipfile.ZIP_DEFLATED) as output:
        for name, content in entries:
            output.writestr(name, content)
    return data.getvalue()


# The Lua metadata deliberately has no version: the public catalog owns version identity.
LUA = b'-- name: Maintenance Fixture\n-- description: Fixture without a version\n'
PAYLOADS = {
    '/old': archive([('main.lua', LUA), ('old-only.txt', b'old asset')]),
    '/new': archive([('main.lua', LUA + b'print("new")\n'), ('new-only.txt', b'new asset')]),
    '/trigger': b'-- name: Recovery trigger\n',
    '/slow': LUA,
    '/corrupt': b'PK\x03\x04broken',
}
REQUESTS = []


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def do_GET(self):
        REQUESTS.append(self.path)
        body = PAYLOADS.get(self.path, b'not found')
        self.send_response(200 if self.path in PAYLOADS else 404)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        try:
            if self.path == '/slow':
                time.sleep(2)
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass
        self.close_connection = True


def digest(data):
    return hashlib.sha256(data).hexdigest()


def key(record):
    return digest('\n'.join(record[k] for k in ('mod_id', 'version_id', 'file_id')).encode())


def tree(path):
    """Compare contents and entry types, including symlinks without following them."""
    result = {}
    if not path.exists():
        return result
    for directory, dirs, files in os.walk(path, followlinks=False):
        for name in dirs + files:
            entry = Path(directory) / name
            relative = str(entry.relative_to(path))
            if entry.is_symlink():
                result[relative] = ('link', os.readlink(entry))
            elif entry.is_dir():
                result[relative] = ('directory',)
            else:
                result[relative] = ('file', digest(entry.read_bytes()))
    return result


def protected(root):
    return tree(root / 'mods'), tree(root / 'mi/receipts')


def build_cli(directory, bundled_curl):
    miniz, executable = directory / 'miniz.o', directory / 'maintenance-cli'
    subprocess.run([os.environ.get('CC', 'clang'), '-I' + str(ROOT / 'src'), '-c',
                    str(ROOT / 'src/pc/utils/miniz/miniz.c'), '-o', str(miniz)], check=True)
    command = [os.environ.get('CXX', 'clang++'), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
               '-I' + str(ROOT / 'lib/curl/include'), '-I' + str(ROOT / 'src')]
    command += [str(ROOT / ('src/pc/mod_integrations/' + name)) for name in ('mi_catalog.cpp', 'mi_updates.cpp', 'mi_install.cpp')]
    command += [str(ROOT / 'tests/mi/maintenance_cli.cpp'), str(miniz), build_sqlite(directory),
                '-Wl,--wrap=syscall', '-Wl,--wrap=rename']
    environment = os.environ.copy()
    if bundled_curl:
        library = ROOT / 'lib/curl/android/arm64-v8a'
        command += ['-L' + str(library)]
        environment['LD_LIBRARY_PATH'] = str(library) + os.pathsep + environment.get('LD_LIBRARY_PATH', '')
    subprocess.run(command + ['-lcurl', '-o', str(executable)], check=True)
    return executable, environment


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundled-curl', action='store_true')
    parser.add_argument('--storage-root', help='Parent of a new isolated temporary game root')
    parser.add_argument('--report', type=Path, help='Write JSON results after all contracts pass')
    args = parser.parse_args()
    completed, skipped = [], []
    started = time.monotonic()
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory(prefix='mi-maintenance-bin-') as temp, tempfile.TemporaryDirectory(prefix='mi-maintenance-data-', dir=args.storage_root) as data:
            directory, games = Path(temp), Path(data)
            executable, environment = build_cli(directory, args.bundled_curl)
            base = json.loads((ROOT / 'tests/mi/catalog_sample.json').read_text())
            base['mods'] = [base['mods'][0]]
            base['mod_count'] = 1
            mod = base['mods'][0]
            mod['version'] = '2.0'
            mod['versions'] = []
            for identity, label, date in [('old', '1.0', '2025-01-01'), ('new', '2.0', '2025-02-01')]:
                mod['versions'].append({'version_id': identity, 'version': label, 'release_date': date, 'resolved': True,
                    'files': [{'file_id': 'primary-' + identity, 'filename': 'pack.zip',
                               'download_url': 'https://example.invalid/' + identity,
                               'mirror_url': 'https://mods-cdn.sm64cdpy.org/test/' + identity, 'mirror_status': 'available',
                               'role': 'primary', 'recommended': True, 'requires_primary': False}]})
            catalog_path = directory / 'catalog.json'
            catalog_path.write_text(json.dumps(base))
            serial = 0

            def source(version='old', endpoint=None, filename='pack.zip', mod_id='900001'):
                endpoint = endpoint or '/' + version
                body = PAYLOADS.get(endpoint, b'')
                return {'mod_id': mod_id, 'version_id': version, 'file_id': 'primary-' + version,
                        'version': '1.0' if version == 'old' else '2.0', 'filename': filename,
                        'url': f'http://127.0.0.1:{server.server_port}' + endpoint,
                        'sha256': digest(body) if body else '', 'size': len(body)}

            def invoke(action, root, record=None, mode='normal', crash=False):
                nonlocal serial
                serial += 1
                args_cli = [str(executable), action, str(root)]
                if action == 'scan':
                    args_cli += [str(catalog_path)]
                else:
                    path = directory / f'source-{serial}.json'
                    path.write_text(json.dumps(record or source()))
                    args_cli += [str(path), mode]
                    if action == 'busy-close':
                        args_cli += [str(catalog_path)]
                result = subprocess.run(args_cli, env=environment, text=True, capture_output=True, timeout=40)
                if crash:
                    assert result.returncode == 73, (mode, result.returncode, result.stderr)
                    return None
                assert result.returncode == 0, (action, mode, result.returncode, result.stderr, result.stdout)
                return json.loads(result.stdout)

            def scan(root):
                before = tree(root)
                result = invoke('scan', root)
                assert tree(root) == before, ('read-only scan changed files', result)
                return result

            def fresh(name, install=True):
                root = games / name
                root.mkdir()
                if install:
                    result = invoke('install', root)
                    assert result['state'] == 5 and result['revision_delta'] >= 2, result
                return root

            def failed_install(root, record=None, error=None, mode='normal'):
                before = protected(root)
                result = invoke('install', root, record, mode)
                assert result['state'] == 6 and (error is None or result['error'] == error), result
                assert protected(root) == before, ('failed operation changed installed data', result)
                return result

            root = fresh('end-to-end')
            receipts = list((root / 'mi/receipts').glob('*.json'))
            assert len(receipts) == 1
            record = json.loads(receipts[0].read_text())
            assert record['mod_id'] == '900001' and record['version_id'] == 'old' and record['destination'] == 'pack'
            assert receipts[0].stem == key(record)
            for entry in record['files']:
                payload = (root / 'mods/pack' / entry['path']).read_bytes()
                assert digest(payload) == entry['sha256'] and len(payload) == entry['size']
            result = scan(root)
            assert result['available'] == 1 and result['uncertain'] == 0, result
            assert result['candidates'] == [{'mod_id': '900001', 'version_id': 'new', 'file_id': 'primary-new',
                                              'destination': 'pack', 'installed_version': '1.0'}], result
            completed.append('real install -> receipt hashes -> read-only candidate without Lua version metadata')
            count = len(REQUESTS)
            before = protected(root)
            result = invoke('install', root)
            assert result['state'] == 5 and protected(root) == before and len(REQUESTS) == count
            # Opaque IDs are unchanged: ordinary install must not silently replace a binary.
            result = invoke('install', root, source(endpoint='/new'))
            assert result['state'] == 5 and protected(root) == before and len(REQUESTS) == count
            completed.append('repeat/same-ID remote change preserves installed bytes and makes no HTTP request')
            failed_install(root, source('new'), error=8)
            assert scan(root)['available'] == 1
            completed.append('new version through ordinary installer is blocked at existing destination')
            renamed = source('new', filename='renamed.zip')
            assert invoke('install', root, renamed)['state'] == 5
            result = scan(root)
            assert result['available'] == 0 and result['uncertain'] == 2, result
            completed.append('renamed archive can create a separate install; duplicate association blocks automatic selection')

            for mutation in ['same-size-edit', 'extra-file', 'missing-file', 'receipt-identity', 'receipt-traversal']:
                root = fresh(mutation)
                receipt_path = next((root / 'mi/receipts').glob('*.json'))
                if mutation == 'same-size-edit':
                    (root / 'mods/pack/main.lua').write_bytes(LUA.replace(b'Fixture', b'Changed'))
                elif mutation == 'extra-file':
                    (root / 'mods/pack/user-settings.txt').write_text('preserve user settings')
                elif mutation == 'missing-file':
                    (root / 'mods/pack/main.lua').unlink()
                else:
                    receipt = json.loads(receipt_path.read_text())
                    if mutation == 'receipt-identity':
                        receipt['mod_id'] = 'another-mod'
                    else:
                        receipt['destination'] = '../outside'
                    receipt_path.write_text(json.dumps(receipt))
                failed_install(root, error=11)
                result = scan(root)
                expected = 1 if mutation in ('same-size-edit', 'extra-file') else 0
                assert result['available'] == expected, (mutation, result)
                if not expected:
                    assert result['uncertain'] == 1, (mutation, result)
                completed.append(mutation + ': original preserved; integrity and presence checks remain distinct')

            root = fresh('symlink-local')
            target = root / 'mods/pack/main.lua'
            outside = root / 'outside.lua'
            outside.write_bytes(LUA)
            target.unlink()
            try:
                target.symlink_to(outside)
            except OSError as error:
                skipped.append('symlink-local: filesystem does not support symlinks: ' + str(error.errno))
            else:
                failed_install(root, error=11)
                assert scan(root)['uncertain'] == 1 and outside.read_bytes() == LUA
                completed.append('symlink-local rejected without touching outside target')

            for endpoint, filename, error in [('/404', 'pack.zip', 2), ('/corrupt', 'pack.zip', 5)]:
                root = fresh('failed-download-' + endpoint[1:])
                failed_install(root, source('new', endpoint, filename), error=error)
                assert scan(root)['available'] == 1
                completed.append(endpoint + ': download/format failure preserves old receipt and files')

            root = fresh('busy-close')
            before = protected(root)
            result = invoke('busy-close', root, source('new', '/slow', 'slow.lua'))
            assert result['closed'] and result['revision_delta'] >= 2 and protected(root) == before, result
            assert not list((root / 'mi').glob('stage-*'))
            assert invoke('install', root, source('trigger', '/trigger', 'trigger.lua', 'trigger-mod'))['state'] == 5
            completed.append('actual install blocks scan; nonblocking close cancels worker, releases exclusion and permits retry')

            for mode in ['crash-staged', 'crash-delivered', 'crash-receipt']:
                root = fresh(mode, install=False)
                invoke('install', root, mode=mode, crash=True)
                journals = list((root / 'mi/transactions').glob('*.json'))
                assert len(journals) == 1
                before = tree(root)
                result = scan(root)
                assert result['state'] == 2 and result['error'] == 2 and tree(root) == before, result
                delivered = mode != 'crash-staged'
                assert (root / 'mods/pack').exists() == delivered
                result = invoke('install', root, source('trigger', '/trigger', 'trigger.lua', 'trigger-mod'))
                assert result['state'] == 5, result
                assert not list((root / 'mi/transactions').glob('*.json')) and not list((root / 'mi').glob('stage-*'))
                old_receipt = root / 'mi/receipts' / (key(source()) + '.json')
                assert old_receipt.exists() == delivered
                assert (root / 'mods/pack').exists() == delivered
                result = scan(root)
                assert result['available'] == int(delivered), result
                completed.append(mode + ': real process exit leaves evidence; next process recovers and releases kernel lock')

            root = fresh('crash-then-user-edit', install=False)
            invoke('install', root, mode='crash-delivered', crash=True)
            (root / 'mods/pack/main.lua').write_bytes(b'preserve changed delivered file')
            before = tree(root)
            failed_install(root, source('trigger', '/trigger', 'trigger.lua', 'trigger-mod'), error=11)
            assert tree(root) == before
            assert scan(root)['error'] == 2
            completed.append('post-crash local edit stops recovery and preserves journal and mod')

            root = fresh('journal-no-space', install=False)
            failed_install(root, mode='journal-no-space', error=4)
            assert not list((root / 'mi').glob('stage-*'))
            assert not list((root / 'mi/transactions').glob('*.json'))
            assert invoke('install', root)['state'] == 5
            completed.append('simulated ENOSPC publishing journal: no delivery, cleanup, retry succeeds')

            root = fresh('receipt-no-space', install=False)
            result = invoke('install', root, mode='receipt-no-space')
            assert result['state'] == 6 and result['error'] == 11, result
            assert (root / 'mods/pack/main.lua').read_bytes() == LUA
            assert not list((root / 'mi/receipts').glob('*.json'))
            assert len(list((root / 'mi/transactions').glob('*.json'))) == 1
            assert scan(root)['error'] == 2
            result = invoke('install', root)
            assert result['state'] == 5, result
            assert not list((root / 'mi/transactions').glob('*.json'))
            assert scan(root)['available'] == 1
            completed.append('simulated ENOSPC publishing receipt: no false success; journal permits recovery')

            root = fresh('destination-race', install=False)
            result = invoke('install', root, mode='destination-race')
            assert result['state'] == 6 and result['error'] == 8, result
            assert (root / 'mods/pack/user.txt').read_text() == 'preserve concurrent destination'
            assert not (root / 'mods/pack/main.lua').exists()
            assert not list((root / 'mi/receipts').glob('*.json'))
            assert not list((root / 'mi/transactions').glob('*.json')) and not list((root / 'mi').glob('stage-*'))
            completed.append('destination created after preflight survives atomic RENAME_NOREPLACE collision')

            report = {'status': 'passed', 'cases': completed, 'skipped': skipped,
                      'storage': str(Path(args.storage_root).resolve()) if args.storage_root else 'internal temporary filesystem',
                      'bundled_curl': args.bundled_curl, 'seconds': round(time.monotonic() - started, 2),
                      'limits': ['Initial-install contracts; replacement/rollback coverage is in test_replacement.py.',
                                 'ENOSPC is injected at metadata publication, not a full disk.',
                                 'Process exit is not power-loss durability or a physical Android lifecycle test.']}
            if args.report:
                args.report.write_text(json.dumps(report, indent=2) + '\n')
            print(json.dumps(report, indent=2))
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    main()
