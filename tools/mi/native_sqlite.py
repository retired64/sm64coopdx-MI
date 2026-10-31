"""Compile the same SQLite wrapper used by the game for native test executables."""
import os
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[2]
def build_sqlite(directory):
    target=Path(directory)/'mi-sqlite.o'
    subprocess.run([os.environ.get('CC','clang'),'-std=c11','-O2','-I'+str(ROOT/'src'),'-c',str(ROOT/'src/pc/mod_integrations/mi_sqlite.c'),'-o',str(target)],check=True)
    return str(target)
