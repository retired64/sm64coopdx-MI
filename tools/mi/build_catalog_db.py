#!/usr/bin/env python3
"""Build the read-only native catalog database from the public JSON contract."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sqlite3
import tempfile

SCHEMA = '''
PRAGMA page_size=4096;
PRAGMA application_id=1296646978;
PRAGMA user_version=1;
CREATE TABLE metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL) WITHOUT ROWID;
CREATE TABLE mods(ordinal INTEGER PRIMARY KEY,id TEXT UNIQUE NOT NULL,name TEXT,author TEXT,category TEXT,description TEXT,version TEXT,image_url TEXT);
CREATE TABLE translations(mod INTEGER,locale TEXT,text TEXT,PRIMARY KEY(mod,locale)) WITHOUT ROWID;
CREATE TABLE versions(mod INTEGER,ordinal INTEGER,id TEXT,label TEXT,date TEXT,stamp TEXT,provider_date TEXT,resolved INTEGER,PRIMARY KEY(mod,ordinal)) WITHOUT ROWID;
CREATE TABLE files(mod INTEGER,version INTEGER,ordinal INTEGER,id TEXT,filename TEXT,label TEXT,role TEXT,recommended INTEGER,requires_primary INTEGER,url TEXT,status TEXT,size INTEGER,sha256 TEXT,PRIMARY KEY(mod,version,ordinal)) WITHOUT ROWID;
'''


def build_database(source, output):
    """Runtime schema covers current native DTOs; full public JSON remains the build source."""
    data = json.loads(source.read_text())
    canonical = json.dumps(data['mods'], ensure_ascii=False, sort_keys=True, separators=(',', ':')).encode()
    if (data.get('schema_version') != 2 or data.get('catalog_status') != 'complete'
            or data.get('mod_count') != len(data['mods'])
            or data.get('catalog_id') != 'sha256:' + hashlib.sha256(canonical).hexdigest()):
        raise ValueError('Invalid public source catalog')
    fd, name = tempfile.mkstemp(prefix='.catalog-benchmark-', suffix='.db', dir=output.parent)
    os.close(fd)
    try:
        with sqlite3.connect(name) as connection:
            connection.executescript(SCHEMA)
            connection.executemany('INSERT INTO metadata VALUES (?,?)',
                [(key, json.dumps(value, ensure_ascii=False)) for key, value in data.items() if key != 'mods'])
            for i, mod in enumerate(data['mods']):
                connection.execute('INSERT INTO mods VALUES (?,?,?,?,?,?,?,?)',
                    (i,str(mod['id']),mod.get('name',''),mod.get('author',''),mod.get('category',''),
                     mod.get('description',''),mod.get('version',''),mod.get('image_url') or mod.get('icon_url','')))
                connection.executemany('INSERT INTO translations VALUES (?,?,?)',
                    [(i, locale, text) for locale, text in mod.get('description_translations', {}).items()])
                for j, version in enumerate(mod['versions']):
                    connection.execute('INSERT INTO versions VALUES (?,?,?,?,?,?,?,?)',
                        (i,j,version['version_id'],version.get('version',''),version.get('release_date',''),
                         version.get('release_timestamp',''),version.get('provider_release_date',''),int(version['resolved'])))
                    for k, file in enumerate(version['files']):
                        connection.execute('INSERT INTO files VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)',
                            (i,j,k,file['file_id'],file['filename'],file.get('display_name',''),file.get('role',''),
                             int(file.get('recommended',False)),int(file.get('requires_primary',False)),
                             file.get('mirror_url',''),file.get('mirror_status','unavailable'),
                             file.get('mirror_size_bytes',0),file.get('sha256','')))
            connection.commit()
            connection.execute('VACUUM')
            assert connection.execute('PRAGMA integrity_check').fetchall() == [('ok',)]
            assert connection.execute('SELECT count(*) FROM mods').fetchone()[0] == data['mod_count']
        os.replace(name, output)
    finally:
        if os.path.exists(name): os.unlink(name)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args()
    if args.source.resolve()==args.output.resolve():parser.error('Input and output must differ')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    build_database(args.source,args.output)
    print('Built native catalog database:',args.output.stat().st_size,'bytes')

if __name__=='__main__':main()
