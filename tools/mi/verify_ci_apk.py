#!/usr/bin/env python3
"""Check an ARM64 MI APK before uploading it; never reads a user ROM."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import zipfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def verify(apk, native_dir=None):
    with zipfile.ZipFile(apk) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError('Duplicate APK entries')
        if archive.testzip() is not None:
            raise ValueError('Corrupt APK entry')
        if 'AndroidManifest.xml' not in names or 'classes.dex' not in names:
            raise ValueError('Missing Android application contents')
        if any(name.lower().endswith(('.z64', '.n64', '.v64', '.pk8', '.jks', '.keystore'))
               or 'baserom' in name.lower() for name in names):
            raise ValueError('ROM or private signing material must not be packaged')
        abis = {name.split('/')[1] for name in names if name.startswith('lib/') and name.endswith('.so')}
        if abis != {'arm64-v8a'}:
            raise ValueError('Unexpected native ABI set: ' + repr(abis))
        libraries = {}
        for name in ('libmain.so', 'libSDL2.so', 'libcurl.so', 'libc++_shared.so'):
            entry = 'lib/arm64-v8a/' + name
            libraries[entry] = digest(archive.read(entry))
            if native_dir is not None:
                built = Path(native_dir) / 'arm64-v8a' / name
                if libraries[entry] != digest(built.read_bytes()):
                    raise ValueError('Packaged library differs from stripped build: ' + name)
        for language in ('English', 'Spanish', 'Portuguese'):
            text = archive.read('assets/lang/' + language + '.ini').decode('utf-8-sig')
            if '[MI_CATALOG]' not in text or 'SEARCH = ' not in text:
                raise ValueError('Missing MI language section: ' + language)
        for folder in ('mods', 'dynos', 'palettes'):
            if not any(name.startswith('assets/' + folder + '/') and not name.endswith('/') for name in names):
                raise ValueError('Missing packaged game directory: ' + folder)
    return libraries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('apk', type=Path)
    parser.add_argument('--native-dir', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    libraries = verify(args.apk, args.native_dir)
    report = {
        'commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
        'abi': 'arm64-v8a', 'apk': args.apk.name,
        'bytes': args.apk.stat().st_size, 'sha256': digest(args.apk.read_bytes()),
        'native_libraries': libraries,
        'ndk': os.environ.get('NDK_VERSION'),
        'build_tools': os.environ.get('BUILD_TOOLS_VERSION'),
        'signing': 'inherited public test key; not a private MI release key',
        'rom_source_file_packaged': False,
    }
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
