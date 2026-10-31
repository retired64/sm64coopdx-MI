#!/usr/bin/env python3
"""Experimental JSON/SQLite comparison. Does not change the game's storage backend."""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import random
import sqlite3
import statistics
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
from build_catalog_db import build_database

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=ROOT/'assets/mi/catalog.json')
    parser.add_argument('--output-dir',type=Path,required=True)
    parser.add_argument('--rounds',type=int,default=9)
    parser.add_argument('--sqlite-object',type=Path,help='Use a precompiled pinned game SQLite object instead of the system library')
    args = parser.parse_args()
    if not 3 <= args.rounds <= 30: parser.error('--rounds must be between 3 and 30')
    if args.source.read_bytes() != (ROOT/'assets/mi/catalog.json').read_bytes():
        parser.error('This benchmark must use the current bundled public catalog')
    subprocess.run([sys.executable,str(ROOT/'tools/mi/embed_catalog.py'),'--check'],check=True)
    args.output_dir.mkdir(parents=True,exist_ok=True)
    database=args.output_dir/'catalog-experimental.db'
    if args.source.resolve() == database.resolve(): parser.error('Input cannot be overwritten')
    source_hash=hashlib.sha256(args.source.read_bytes()).hexdigest()
    build_database(args.source,database)
    with tempfile.TemporaryDirectory(prefix='mi-storage-benchmark-') as folder:
        executable=Path(folder)/'benchmark'
        command=[os.environ.get('CXX','clang++'),'-std=c++17','-O2','-Wall','-Wextra','-Werror',
            '-I'+str(ROOT/'src'),str(ROOT/'tests/mi/catalog_storage_benchmark.cpp')]
        command += [str(args.sqlite_object.resolve())] if args.sqlite_object else ['-lsqlite3']
        command += ['-pthread','-o',str(executable)]
        subprocess.run(command,check=True)
        def run(mode):
            result=subprocess.run([str(executable),mode,str(args.source),str(database)],check=True,capture_output=True,text=True)
            return json.loads(result.stdout)
        assert run('verify')['equivalent']
        modes=['json-bundled','json','sqlite-full','sqlite-bundled','sqlite-lazy']
        run('json-bundled');run('sqlite-full')
        results={mode:[] for mode in modes}
        for iteration in range(args.rounds):
            order=modes[:];random.Random(iteration).shuffle(order)
            for mode in order: results[mode].append(run(mode))
        summary={mode:{key:statistics.median(row[key] for row in rows)
            for key in ('load_ms','query_ms','peak_kib')} for mode,rows in results.items()}
        report={'experimental':True,'equivalent_current_dtos_es':True,'rounds':args.rounds,
            'sqlite_version':sqlite3.sqlite_version,'source_sha256':source_hash,
            'sqlite_provider':'game-object' if args.sqlite_object else 'system-library',
            'reader_sha256':hashlib.sha256((ROOT/'src/pc/mod_integrations/mi_catalog_database.inc').read_bytes()).hexdigest(),
            'bytes':{'json':args.source.stat().st_size,'sqlite':database.stat().st_size,
                'json_gzip':len(gzip.compress(args.source.read_bytes(),mtime=0)),
                'sqlite_gzip':len(gzip.compress(database.read_bytes(),mtime=0))},
            'median':summary,'samples':results,
            'limits':['Optimized C++ CLI on this Termux device; not a rendered game or FPS benchmark.',
                'Fresh process per sample, warm OS file cache; no claim of cold storage latency.',
                'sqlite-full materializes the current DTOs; sqlite-lazy is a different architecture.',
                'Database projects current DTOs; JSON remains the full public interchange contract.',
                'sqlite-full/lazy use a prototype loader; sqlite-bundled uses the hardened production reader.',
                'Provider is recorded separately; production benchmarks can link the exact pinned game object.',
                'Lazy SQL search uses raw descriptions; it is not certified equivalent to truncated/sanitized UI substring search.',
                'Peak RSS uses Linux /proc/self/status VmHWM after exec, covering 100 search/page operations; no per-allocator memory claim.',
                'A production SQLite dependency and full packaged APK size are not measured here.']}
        assert hashlib.sha256(args.source.read_bytes()).hexdigest()==source_hash
        (args.output_dir/'storage-benchmark.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
        print(json.dumps({key:report[key] for key in ('bytes','median','rounds')},indent=2))


if __name__=='__main__':main()
