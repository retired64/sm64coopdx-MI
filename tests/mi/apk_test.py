#!/usr/bin/env python3
"""Test rejection of incomplete/stale artifacts with tiny synthetic ZIPs."""
from pathlib import Path
import runpy
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[2]
verify = runpy.run_path(str(ROOT / 'tools/mi/verify_ci_apk.py'))['verify']


class ApkContract(unittest.TestCase):
    def test_artifact_gate(self):
        entries = {'AndroidManifest.xml': b'manifest', 'classes.dex': b'dex'}
        for name in ('libmain.so', 'libSDL2.so', 'libcurl.so', 'libc++_shared.so'):
            entries['lib/arm64-v8a/' + name] = name.encode()
        for language in ('English', 'Spanish', 'Portuguese'):
            entries['assets/lang/' + language + '.ini'] = b'[MI_CATALOG]\nSEARCH = "Search"\n'
        for folder in ('mods', 'dynos', 'palettes'):
            entries['assets/' + folder + '/fixture'] = b'fixture'
        with tempfile.TemporaryDirectory(prefix='mi-apk-gate-') as directory:
            temp = Path(directory)
            native = temp / 'native/arm64-v8a'
            native.mkdir(parents=True)
            for name, data in entries.items():
                if name.startswith('lib/'):
                    (native / Path(name).name).write_bytes(data)

            def pack(data):
                apk = temp / 'test.apk'
                with zipfile.ZipFile(apk, 'w') as archive:
                    for name, body in data.items():
                        archive.writestr(name, body)
                return apk

            self.assertEqual(len(verify(pack(entries), native.parent)), 4)
            for missing in ('classes.dex', 'assets/lang/Spanish.ini', 'lib/arm64-v8a/libcurl.so',
                            'assets/mods/fixture'):
                with self.subTest(missing=missing), self.assertRaises((ValueError, KeyError)):
                    verify(pack({k: v for k, v in entries.items() if k != missing}), native.parent)
            for extra in ('assets/baserom.us.z64', 'assets/signing.pk8', 'lib/x86_64/libmain.so'):
                with self.subTest(extra=extra), self.assertRaises(ValueError):
                    verify(pack({**entries, extra: b'unexpected'}), native.parent)
            (native / 'libmain.so').write_bytes(b'stale build')
            with self.assertRaisesRegex(ValueError, 'differs'):
                verify(pack(entries), native.parent)


if __name__ == '__main__':
    unittest.main()
