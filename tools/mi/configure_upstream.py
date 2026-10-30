#!/usr/bin/env python3
"""Configure MI's local remotes safely, or audit them without network/mutations.
Run: python tools/mi/configure_upstream.py setup|check
No checkout, merge, reset, push, global config or worktree edits are performed.
"""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = 'https://github.com/ManIsCat2/sm64coopdx.git'
READ_ONLY = 'disabled://upstream-read-only'
FETCH = 'refs/heads/android:refs/remotes/upstream/android'
ORIGINS = {'git@github.com:retired64/sm64coopdx-MI.git',
           'https://github.com/retired64/sm64coopdx-MI.git'}
SETTINGS = {
    'remote.upstream.fetch': FETCH,
    'remote.upstream.tagOpt': '--no-tags',
    'remote.upstream.prune': 'true',
    'remote.upstream.pushurl': READ_ONLY,
    'remote.pushDefault': 'origin',
    'push.default': 'simple',
    'pull.ff': 'only',
    'rerere.enabled': 'true',
    'rerere.autoupdate': 'false',
    'branch.mi-main.remote': 'origin',
    'branch.mi-main.merge': 'refs/heads/mi-main',
    'branch.mi-main.pushRemote': 'origin',
    'branch.android.remote': 'upstream',
    'branch.android.merge': 'refs/heads/android',
    'branch.android.pushRemote': 'origin',
}


def git(*args, optional=False):
    result = subprocess.run(['git', '-C', str(ROOT), *args], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        if optional and result.returncode == 1:
            return ''
        raise RuntimeError(result.stderr.strip() or 'git failed: ' + ' '.join(args))
    return result.stdout.strip()


def values(key):
    result = git('config', '--local', '--get-all', key, optional=True)
    return result.splitlines() if result else []


def audit():
    errors = []
    if values('remote.upstream.url') != [UPSTREAM]:
        errors.append('remote.upstream.url')
    for key, expected in SETTINGS.items():
        if values(key) != [expected]:
            errors.append(key)
    if errors:
        raise RuntimeError('Unexpected local configuration: ' + ', '.join(errors))
    base = git('merge-base', 'mi-main', 'upstream/android')
    ahead, behind = git('rev-list', '--left-right', '--count',
                       'mi-main...upstream/android').split()
    print('MI HEAD:', git('rev-parse', 'mi-main'))
    print('upstream/android (last fetch):', git('rev-parse', 'upstream/android'))
    print('common base:', base)
    print('MI-only commits:', ahead, '| upstream commits not in MI:', behind)
    print('android mirror:', git('rev-parse', 'android'))
    mirror_ahead, mirror_behind = git('rev-list', '--left-right', '--count',
                                    'android...upstream/android').split()
    if mirror_ahead != '0' or mirror_behind != '0':
        print('NOTICE: android mirror differs; review without force:',
              mirror_ahead, mirror_behind)
    print('Configuration OK. check is offline; it does not certify remote freshness,')
    print('gameplay compatibility, CI, GitHub protection or a completed integration.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('setup', 'check'))
    args = parser.parse_args()
    if git('rev-parse', '--show-toplevel') != str(ROOT):
        raise RuntimeError('Run from an intact MI checkout containing this script.')
    if values('remote.origin.url') not in [[url] for url in ORIGINS]:
        raise RuntimeError('origin is not the expected MI fork; inspect it manually.')
    pushes = values('remote.origin.pushurl')
    if pushes and pushes not in [[url] for url in ORIGINS]:
        raise RuntimeError('Unexpected origin push URL; no configuration changed.')
    if args.action == 'setup':
        # Validate existing identity before any mutation. Configuration is local.
        urls = values('remote.upstream.url')
        if urls and urls != [UPSTREAM]:
            raise RuntimeError('upstream already points elsewhere; no changes made.')
        git('show-ref', '--verify', 'refs/heads/mi-main')
        if not urls:
            git('remote', 'add', '-t', 'android', '--no-tags', 'upstream', UPSTREAM)
        for key, value in SETTINGS.items():
            git('config', '--local', '--replace-all', key, value)
        print('Local configuration applied. Fetching without merging or moving branches.', flush=True)
        previous = git('for-each-ref', '--format=%(objectname)', 'refs/remotes/upstream/android')
        git('fetch', '--no-tags', 'origin')
        git('fetch', '--no-tags', 'upstream')
        # Remote-tracking refs can change non-fast-forward even without '+'.
        # Detect a rewrite explicitly; no local branch is advanced by this tool.
        if previous:
            current = git('rev-parse', 'upstream/android')
            result = subprocess.run(['git', '-C', str(ROOT), 'merge-base',
                                     '--is-ancestor', previous, current])
            if result.returncode:
                raise RuntimeError('upstream history changed non-fast-forward; '
                                   'tracking ref fetched, branches untouched. Review history.')
        git('merge-base', 'mi-main', 'upstream/android')
        if not git('for-each-ref', '--format=%(objectname)', 'refs/heads/android'):
            git('branch', '--track', 'android', 'upstream/android')
    audit()


if __name__ == '__main__':
    try:
        main()
    except (RuntimeError, OSError) as error:
        print('ERROR:', error, file=sys.stderr)
        print('No merge/push was performed. If setup stopped after applying config,',
              'inspect remotes and rerun after resolving the cause.', file=sys.stderr)
        sys.exit(1)
