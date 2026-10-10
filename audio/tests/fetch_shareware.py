#!/usr/bin/env python3
"""Fetch only the three shareware audio files into an external local cache."""
import argparse
import hashlib
import io
import pathlib
import urllib.request
import zipfile

URL = 'https://beta.wolf3d.net/file-download/download/private/4457'
ARCHIVE_SHA256 = '3ff0798f7d912dcd03f78419b55d63dd1934a93b6b3be1fab425dc2b148612cc'
FILES = {
    'AUDIOHED.WL1': '39351624ae6f8eef4b873e060c1a6f3e5ee7e81c4939c485275a89b145336338',
    'AUDIOT.WL1': '1e2c9ae30398a14c61a4ddd39aabaa0dbcc984cc4924a3e51df75574c259cfb9',
    'VSWAP.WL1': '698f217257e2cbb951a4d110ba09140291f38d0121b3784d1d6be59c03a6b47b',
}

def digest(data):
    return hashlib.sha256(data).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dir', type=pathlib.Path, default=pathlib.Path.home()/'wolf3d-data/shareware')
    parser.add_argument('--check-only', action='store_true')
    args = parser.parse_args()
    dest = args.dir.expanduser().resolve()
    repo = pathlib.Path(__file__).resolve().parents[2]
    if dest == repo or repo in dest.parents:
        raise SystemExit('Keep game data outside the repository')
    if not args.check_only:
        with urllib.request.urlopen(URL, timeout=60) as response:
            archive = response.read(8*1024*1024+1)
        if digest(archive) != ARCHIVE_SHA256:
            raise SystemExit('Archive SHA-256 mismatch; no files written')
        with zipfile.ZipFile(io.BytesIO(archive)) as z:
            content = {name: z.read(name) for name in FILES}
        for name, data in content.items():
            if digest(data) != FILES[name]:
                raise SystemExit('SHA-256 mismatch: '+name)
        dest.mkdir(parents=True, exist_ok=True)
        for name, data in content.items():
            (dest/name).write_bytes(data)
    for name, expected in FILES.items():
        if digest((dest/name).read_bytes()) != expected:
            raise SystemExit('SHA-256 mismatch: '+name)
        print('PASS '+name+' '+expected)

if __name__ == '__main__':
    main()
