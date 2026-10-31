#!/usr/bin/env python3
"""Native individual-update, rollback and interrupted-process contracts. Temp roots only."""
import argparse
import copy
import http.server
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import threading
import time

sys.dont_write_bytecode = True
import test_maintenance as common

COMPLETE, FAILED, CANCELLED = 5, 6, 8
STORAGE, EXISTS, BUSY, INTEGRITY, RECOVERY, CHANGED, RESTORED = 4, 8, 9, 10, 11, 12, 13
MUTATION = None
common.PAYLOADS['/update-slow'] = common.PAYLOADS['/new']
common.PAYLOADS['/new-lua'] = common.LUA + b'print("new")\n'
common.PAYLOADS['/old-empty'] = common.archive([('main.lua', common.LUA), ('old-only.txt', b'old asset'), ('owned-empty/', b'')])
common.PAYLOADS['/new-empty'] = common.archive([('main.lua', common.LUA + b'print("new")\n'), ('new-only.txt', b'new asset'), ('owned-empty/', b'')])


class Handler(common.Handler):
    def do_GET(self):
        if self.path == '/mutating-download':
            assert MUTATION is not None
            MUTATION.write_bytes(common.LUA.replace(b'Fixture', b'Changed'))
            self.path = '/new'
        if self.path == '/update-slow':
            common.REQUESTS.append(self.path)
            body = common.PAYLOADS[self.path]
            self.send_response(200)
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            time.sleep(1)
            try:
                self.wfile.write(body)
            except (BrokenPipeError, ConnectionResetError):
                pass
            self.close_connection = True
        else:
            super().do_GET()


def main():
    global MUTATION
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bundled-curl', action='store_true')
    parser.add_argument('--storage-root', help='Parent for a new isolated temporary directory')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    cases, skipped = [], []
    started = time.monotonic()
    server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory(prefix='mi-replacement-bin-') as temp, tempfile.TemporaryDirectory(prefix='mi-replacement-data-', dir=args.storage_root) as data:
            directory, games = Path(temp), Path(data)
            executable, environment = common.build_cli(directory, args.bundled_curl)
            base = json.loads((common.ROOT / 'tests/mi/catalog_sample.json').read_text())
            base['mods'], base['mod_count'] = [base['mods'][0]], 1
            mod = base['mods'][0]
            mod['version'] = '2.0'
            mod['versions'] = []
            for identity, label, date in [('old', '1.0', '2025-01-01'), ('new', '2.0', '2025-02-01')]:
                mod['versions'].append({'version_id': identity, 'version': label, 'release_date': date, 'resolved': True,
                    'files': [{'file_id': 'primary-' + identity, 'filename': 'pack.zip',
                               'download_url': 'https://example.invalid/' + identity,
                               'role': 'primary', 'recommended': True, 'requires_primary': False}]})
            catalog = directory / 'catalog.json'
            catalog.write_text(json.dumps(base))
            serial = 0

            def source(version='new', endpoint=None, filename='renamed.zip', mod_id='900001'):
                endpoint = endpoint or '/' + version
                body = common.PAYLOADS.get(endpoint, common.PAYLOADS['/new'] if endpoint == '/mutating-download' else b'')
                return {'mod_id': mod_id, 'version_id': version, 'file_id': 'primary-' + version,
                        'version': '1.0' if version == 'old' else '2.0', 'filename': filename,
                        'url': f'http://127.0.0.1:{server.server_port}' + endpoint,
                        'sha256': common.digest(body) if body else '', 'size': len(body),
                        'previous': {'version_id': 'old', 'file_id': 'primary-old', 'destination': 'pack'}}

            def invoke(root, action='update', record=None, mode='normal', crash=False):
                nonlocal serial
                serial += 1
                cmd = [str(executable), action, str(root)]
                if action in ('scan', 'recover'):
                    cmd += [str(catalog)]
                else:
                    path = directory / f'source-{serial}.json'
                    path.write_text(json.dumps(record or source()))
                    cmd += [str(path), mode]
                    if action == 'update-busy-close':
                        cmd += [str(catalog)]
                result = subprocess.run(cmd, env=environment, capture_output=True, text=True, timeout=40)
                if crash:
                    assert result.returncode == 73, (mode, result.returncode, result.stderr, result.stdout)
                    return None
                assert result.returncode == 0, (mode, result.returncode, result.stderr, result.stdout)
                return json.loads(result.stdout)

            def fresh(name, endpoint='/old'):
                root = games / name
                root.mkdir()
                old = source('old', endpoint, filename='pack.zip')
                result = invoke(root, 'install', old)
                assert result['state'] == COMPLETE, result
                return root

            def clean(root):
                assert not list((root / 'mi/transactions').glob('*.json'))
                assert not list((root / 'mi').glob('update-*'))
                assert not list((root / 'mi').glob('stage-*'))

            def verify_new(root, destination='pack', loose=False):
                target = root / 'mods' / destination
                if loose:
                    assert target.read_bytes() == common.PAYLOADS['/new-lua']
                else:
                    assert (target / 'main.lua').read_bytes() == common.LUA + b'print("new")\n'
                    assert (target / 'new-only.txt').read_bytes() == b'new asset'
                    assert not (target / 'old-only.txt').exists()
                receipts = list((root / 'mi/receipts').glob('*.json'))
                own = [json.loads(path.read_text()) for path in receipts if json.loads(path.read_text())['mod_id'] == '900001']
                assert len(own) == 1 and own[0]['version_id'] == 'new' and own[0]['destination'] == destination, own
                for entry in own[0]['files']:
                    payload = target if not entry['path'] else target / entry['path']
                    assert len(payload.read_bytes()) == entry['size'] and common.digest(payload.read_bytes()) == entry['sha256']
                clean(root)

            def failed(root, record=None, mode='normal', error=CHANGED, unchanged=True):
                before = common.protected(root)
                result = invoke(root, record=record, mode=mode)
                assert result['state'] == FAILED and result['error'] == error, result
                if unchanged:
                    assert common.protected(root) == before, (mode, result)
                return result

            def recover(root):
                return invoke(root, 'recover')

            root = games / 'recover-empty'
            root.mkdir()
            count = len(common.REQUESTS)
            assert recover(root)['state'] == COMPLETE and common.tree(root) == {} and len(common.REQUESTS) == count
            cases.append('catalog-entry recovery is a no-op without journals: no folders or download created')

            root = fresh('success')
            result = invoke(root)
            assert result['state'] == COMPLETE and result['destination'] == 'pack', result
            verify_new(root)
            before = common.tree(root)
            result = invoke(root, 'scan')
            assert result['available'] == 0 and result['uncertain'] == 0 and common.tree(root) == before, result
            count = len(common.REQUESTS)
            assert invoke(root)['state'] == COMPLETE and len(common.REQUESTS) == count
            verify_new(root)
            cases.append('ZIP replacement keeps destination despite renamed archive, removes retired files, publishes one receipt and is idempotent')

            root = games / 'loose'
            root.mkdir()
            old = source('old', '/slow', 'boo.lua')
            assert invoke(root, 'install', old)['state'] == COMPLETE
            new = source(endpoint='/new-lua', filename='renamed.lua')
            new['previous']['destination'] = 'boo.lua'
            assert invoke(root, record=new)['state'] == COMPLETE
            verify_new(root, 'boo.lua', loose=True)
            cases.append('standalone Lua replacement keeps original user path and updates receipt')

            root = fresh('owned-empty', '/old-empty')
            assert (root / 'mods/pack/owned-empty').is_dir()
            assert invoke(root)['state'] == COMPLETE
            verify_new(root)
            assert not (root / 'mods/pack/owned-empty').exists()
            cases.append('owned empty directories recorded in schema-1 extension may be removed by replacement')
            root = fresh('new-empty')
            assert invoke(root, record=source(endpoint='/new-empty'))['state'] == COMPLETE
            assert (root / 'mods/pack/owned-empty').is_dir()
            third = source('third', endpoint='/new')
            third['previous'] = {'version_id': 'new', 'file_id': 'primary-new', 'destination': 'pack'}
            assert invoke(root, record=third)['state'] == COMPLETE
            assert not (root / 'mods/pack/owned-empty').exists()
            assert json.loads(next((root / 'mi/receipts').glob('*.json')).read_text())['version_id'] == 'third'
            clean(root)
            cases.append('successive updates preserve directory ownership and replace receipts coherently')
            root = fresh('legacy-receipt')
            receipt_path = next((root / 'mi/receipts').glob('*.json'))
            value = json.loads(receipt_path.read_text())
            value.pop('directories')
            receipt_path.write_text(json.dumps(value))
            assert invoke(root)['state'] == COMPLETE
            verify_new(root)
            cases.append('legacy schema-1 receipts without directory inventory remain supported for owned file trees')
            root = fresh('journal-limit')
            receipt_path = next((root / 'mi/receipts').glob('*.json'))
            value = json.loads(receipt_path.read_text())
            value['padding'] = ''
            overhead = len(json.dumps(value).encode())
            value['padding'] = 'x' * (4 * 1024 * 1024 - overhead - 16)
            receipt_path.write_text(json.dumps(value))
            assert receipt_path.stat().st_size < 4 * 1024 * 1024
            failed(root, error=7)
            clean(root)
            cases.append('oversized combined journal rejected before moving original; no unrecoverable record written')

            for mutation in ['same-size-edit', 'extra-file', 'extra-empty-directory', 'missing-file', 'missing-destination',
                             'receipt-identity', 'receipt-path', 'receipt-version', 'duplicate-association']:
                root = fresh(mutation)
                receipt_path = next((root / 'mi/receipts').glob('*.json'))
                if mutation == 'same-size-edit':
                    (root / 'mods/pack/main.lua').write_bytes(common.LUA.replace(b'Fixture', b'Changed'))
                elif mutation == 'extra-file':
                    (root / 'mods/pack/user.cfg').write_text('keep settings')
                elif mutation == 'extra-empty-directory':
                    (root / 'mods/pack/user-data').mkdir()
                elif mutation == 'missing-file':
                    (root / 'mods/pack/main.lua').unlink()
                elif mutation == 'missing-destination':
                    shutil.rmtree(root / 'mods/pack')
                else:
                    value = json.loads(receipt_path.read_text())
                    if mutation == 'receipt-identity': value['mod_id'] = 'foreign-mod'
                    elif mutation == 'receipt-path': value['destination'] = '../outside'
                    elif mutation == 'receipt-version': value['version_id'] = 'foreign-version'
                    else:
                        value['file_id'] = 'duplicate-file'
                        receipt_path = receipt_path.parent / (common.key(value) + '.json')
                    receipt_path.write_text(json.dumps(value))
                requests = len(common.REQUESTS)
                failed(root)
                assert len(common.REQUESTS) == requests
                clean(root)
                cases.append(mutation + ': refused before download; files, metadata and user additions preserved')

            root = fresh('symlink')
            outside = root / 'outside.lua'
            outside.write_bytes(common.LUA)
            (root / 'mods/pack/main.lua').unlink()
            try:
                (root / 'mods/pack/main.lua').symlink_to(outside)
            except OSError as error:
                skipped.append('local symlink unsupported: ' + str(error.errno))
            else:
                failed(root)
                assert outside.read_bytes() == common.LUA
                cases.append('local symlink rejected without following or changing outside file')

            root = fresh('download-local-race')
            MUTATION = root / 'mods/pack/main.lua'
            failed(root, source(endpoint='/mutating-download'), unchanged=False)
            assert MUTATION.read_bytes() == common.LUA.replace(b'Fixture', b'Changed')
            assert len(list((root / 'mi/receipts').glob('*.json'))) == 1
            clean(root)
            MUTATION = None
            cases.append('local edit during download caught by final revalidation; no original move')

            for label, record, error in [('http', source(endpoint='/404'), 2), ('corrupt', source(endpoint='/corrupt'), 5),
                                         ('hash', dict(source(), sha256='0' * 64), INTEGRITY),
                                         ('size', dict(source(), size=1), INTEGRITY),
                                         ('type-change', source(endpoint='/new-lua', filename='new.lua'), CHANGED)]:
                root = fresh(label)
                failed(root, record, error=error)
                clean(root)
                cases.append(label + ': preparation failure leaves old installation and receipt intact')

            for mode in ['update-cancel-pending', 'update-cancel']:
                root = fresh(mode)
                before = common.protected(root)
                result = invoke(root, record=source(endpoint='/update-slow'), mode=mode)
                assert result['state'] == CANCELLED and common.protected(root) == before, result
                clean(root)
                assert invoke(root)['state'] == COMPLETE
                cases.append(mode + ': no replacement, worker released and retry succeeds')
            root = fresh('close-active')
            before = common.protected(root)
            result = invoke(root, 'update-busy-close', source(endpoint='/update-slow'))
            assert result['closed'] and common.protected(root) == before, result
            clean(root)
            assert invoke(root)['state'] == COMPLETE
            cases.append('close during update is nonblocking, scan is busy, cancellation preserves old and retry succeeds')

            for mode, error in [('update-journal-no-space', STORAGE), ('update-backup-fail', RESTORED),
                                ('update-delivery-fail', RESTORED), ('update-receipt-no-space', RESTORED),
                                ('update-commit-marker-fail', RESTORED)]:
                root = fresh(mode)
                failed(root, mode=mode, error=error)
                clean(root)
                assert invoke(root)['state'] == COMPLETE
                verify_new(root)
                cases.append(mode + ': old version preserved/restored with original receipt; retry completes')

            for mode in ['update-crash-before-backup', 'update-crash-backup', 'update-crash-delivered',
                         'update-crash-receipt', 'update-crash-committed', 'update-crash-cleaning', 'update-crash-rolled-back']:
                root = fresh(mode)
                old = common.protected(root)
                invoke(root, mode=mode, crash=True)
                journals = list((root / 'mi/transactions').glob('*.json'))
                assert len(journals) == 1
                before = common.tree(root)
                result = invoke(root, 'scan')
                assert result['state'] == 2 and result['error'] == 2 and common.tree(root) == before, result
                committed = mode in ('update-crash-committed', 'update-crash-cleaning')
                if mode == 'update-crash-cleaning':
                    # Resume cleanup after part of the already-authorized backup disposal.
                    journal = json.loads(journals[0].read_text())
                    (root / 'mi' / journal['stage'] / 'backup/main.lua').unlink()
                assert recover(root)['state'] == COMPLETE
                if committed:
                    verify_new(root)
                else:
                    assert common.protected(root) == old
                    clean(root)
                    assert invoke(root)['state'] == COMPLETE
                    verify_new(root)
                cases.append(mode + ': process restart resolves durable intent and leaves one coherent version')

            for mode in ['update-rollback-fail', 'update-restore-fail', 'update-rollback-journal-fail']:
                root = fresh(mode)
                failed(root, mode=mode, error=RECOVERY, unchanged=False)
                assert len(list((root / 'mi/transactions').glob('*.json'))) == 1
                assert invoke(root, 'scan')['error'] == 2
                assert invoke(root)['state'] == COMPLETE
                verify_new(root)
                cases.append(mode + ': failed rollback retains evidence; next process recovers before retrying')

            for mutation in ['edit-new', 'edit-backup', 'foreign-destination', 'journal-traversal', 'journal-identity', 'journal-stage-inode', 'journal-association']:
                root = fresh(mutation)
                if mutation == 'foreign-destination':
                    failed(root, mode='update-destination-race', error=RECOVERY, unchanged=False)
                    assert (root / 'mods/pack/user.txt').read_text() == 'preserve concurrent destination'
                else:
                    invoke(root, mode='update-crash-delivered', crash=True)
                journal_path = next((root / 'mi/transactions').glob('*.json'))
                journal = json.loads(journal_path.read_text())
                if mutation == 'edit-new': (root / 'mods/pack/main.lua').write_bytes(b'keep new local edit')
                elif mutation == 'edit-backup': (root / 'mi' / journal['stage'] / 'backup/main.lua').write_bytes(b'keep backup edit')
                elif mutation == 'journal-traversal': journal['stage'] = '../escape'
                elif mutation == 'journal-identity': journal['new_key'] = '0' * 64
                elif mutation == 'journal-stage-inode': journal['stage_inode'] += 1
                elif mutation == 'journal-association':
                    value = copy.deepcopy(journal['old_receipt'])
                    value['file_id'] = 'foreign-file'
                    (root / 'mi/receipts' / (common.key(value) + '.json')).write_text(json.dumps(value))
                if mutation.startswith('journal-'): journal_path.write_text(json.dumps(journal))
                before = common.tree(root)
                result = recover(root)
                assert result['state'] == FAILED and result['error'] == RECOVERY and common.tree(root) == before, result
                cases.append(mutation + ': unsafe recovery stops with both versions/evidence preserved')

            root = fresh('late-cancel')
            result = invoke(root, mode='update-cancel-commit')
            assert result['state'] == COMPLETE
            verify_new(root)
            cases.append('cancellation after commit starts is ignored; transaction completes coherently')

            root = fresh('abandoned-stage')
            abandoned = root / 'mi/update-ABC123'
            abandoned.mkdir()
            (abandoned / 'download').write_bytes(b'abandoned preparation')
            foreign = root / 'mi/update-user-data'
            foreign.mkdir()
            (foreign / 'keep').write_text('preserve')
            assert invoke(root)['state'] == COMPLETE
            assert not abandoned.exists() and (foreign / 'keep').read_text() == 'preserve'
            # The non-managed directory is intentionally preserved by cleanup.
            shutil.rmtree(foreign)
            verify_new(root)
            cases.append('only managed abandoned preparation is cleaned; unrelated private entries preserved')

            report = {'status': 'passed', 'cases': cases, 'skipped': skipped,
                      'storage': str(Path(args.storage_root).resolve()) if args.storage_root else 'internal temporary filesystem',
                      'bundled_curl': args.bundled_curl, 'seconds': round(time.monotonic() - started, 2),
                      'limits': ['Fault injection is not a full disk or power-loss test.',
                                 'No game UI, multiplayer or physical Android lifecycle is exercised.',
                                 'Updates retain the installed path and refuse file/directory type changes.']}
            if args.report: args.report.write_text(json.dumps(report, indent=2) + '\n')
            print(json.dumps(report, indent=2))
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    main()
