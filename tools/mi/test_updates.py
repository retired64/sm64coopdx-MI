#!/usr/bin/env python3
"""Read-only update detection contracts using synthetic public JSON and temp roots."""
import os
from pathlib import Path
import subprocess
import tempfile

from native_sqlite import build_sqlite

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='mi-updates-tests-') as temp:
    executable = Path(temp) / 'updates-test'
    subprocess.run([
        os.environ.get('CXX', 'clang++'), '-std=c++17', '-Wall', '-Wextra', '-Werror', '-pthread',
        '-I' + str(ROOT / 'src'),
        str(ROOT / 'src/pc/mod_integrations/mi_catalog.cpp'),
        str(ROOT / 'src/pc/mod_integrations/mi_updates.cpp'),
        str(ROOT / 'tests/mi/updates_test.cpp'), build_sqlite(temp), '-o', str(executable),
    ], check=True)
    subprocess.run([str(executable), str(ROOT / 'tests/mi/catalog_sample.json'), str(Path(temp) / 'game')], check=True, timeout=30)
