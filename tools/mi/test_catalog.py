#!/usr/bin/env python3
"""Run native catalog contract tests without Android, SDL, ROM or game data."""
import os
from pathlib import Path
import subprocess
import tempfile
import sys

from native_sqlite import build_sqlite

ROOT = Path(__file__).resolve().parents[2]
subprocess.run([sys.executable, str(ROOT / 'tools/mi/embed_catalog.py'), '--check'], check=True)
with tempfile.TemporaryDirectory(prefix='mi-catalog-') as directory:
    executable = str(Path(directory) / 'catalog-test')
    subprocess.run([
        os.environ.get('CXX', 'clang++'), '-std=c++17', '-Wall', '-Wextra', '-Werror',
        '-I' + str(ROOT / 'src'),
        str(ROOT / 'src/pc/mod_integrations/mi_catalog.cpp'),
        str(ROOT / 'tests/mi/catalog_test.cpp'), build_sqlite(directory), '-pthread', '-o', executable,
    ], check=True)
    subprocess.run([executable, str(ROOT / 'tests/mi/catalog_sample.json')], check=True)
    database_test = str(Path(directory) / 'database-test')
    subprocess.run([os.environ.get('CXX','clang++'),'-std=c++17','-Wall','-Wextra','-Werror',
        '-I'+str(ROOT/'src'),str(ROOT/'tests/mi/catalog_database_test.cpp'),str(Path(directory)/'mi-sqlite.o'),'-pthread','-o',database_test],check=True)
    subprocess.run([database_test,str(ROOT/'assets/mi/catalog.json'),str(ROOT/'assets/mi/catalog.db')],check=True)


with tempfile.TemporaryDirectory(prefix='mi-ui-regression-') as directory:
    for name, sources in (
        ('strings', ['src/pc/mod_integrations/mi_catalog_strings.c', 'tests/mi/strings_test.c']),
        ('panel-layout', ['tests/mi/panel_layout_test.c']),
    ):
        executable = str(Path(directory) / name)
        subprocess.run([
            os.environ.get('CC', 'clang'), '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-I' + str(ROOT / 'tests/mi/stubs'), '-I' + str(ROOT / 'src'),
            *[str(ROOT / path) for path in sources], '-lm', '-o', executable,
        ], check=True)
        subprocess.run([executable], check=True)
