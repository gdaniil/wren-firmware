#!/usr/bin/env python3
"""Install the Wren reader fonts (build/wren-fonts) on the reader.

The firmware looks for /fonts/<Family>/<Family>_<n>.cpfont with the families
WrenLiterata, WrenGolos and WrenBitter (src/wren/typography/ReaderTypography.cpp).
build_fonts.py writes them to build/wren-fonts/<Literata|Golos|Bitter>/.

  install_fonts.py --sd /Volumes/READER          copy to a mounted SD card
  install_fonts.py --device crosspoint.local     upload over Wi-Fi (Send tab on)

Wi-Fi upload uses the existing POST /api/fonts/upload. Files already present
with the same size are skipped.
"""
import argparse
import json
import shutil
import sys
import urllib.request
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / 'build/wren-fonts'
FAMILIES = {'Literata': 'WrenLiterata', 'Golos': 'WrenGolos', 'Bitter': 'WrenBitter'}


def font_files():
    out = []
    for folder, family in FAMILIES.items():
        files = sorted((SRC / folder).glob(f'{family}_*.cpfont'))
        if not files:
            sys.exit(f'No {family}_*.cpfont in {SRC / folder}. Run scripts/wren/build_fonts.py first.')
        licence = SRC / folder / 'OFL.txt'
        out.append((family, files, licence if licence.exists() else None))
    return out


def install_sd(card: Path):
    if not card.is_dir():
        sys.exit(f'{card} is not a mounted folder.')
    root = card / 'fonts'
    copied = skipped = 0
    for family, files, licence in font_files():
        dest = root / family
        dest.mkdir(parents=True, exist_ok=True)
        for f in files + ([licence] if licence else []):
            target = dest / f.name
            if target.exists() and target.stat().st_size == f.stat().st_size:
                skipped += 1
                continue
            shutil.copyfile(f, target)
            copied += 1
    print(f'Copied {copied} files, {skipped} already there, into {root}.')


def installed_sizes(base: str):
    with urllib.request.urlopen(f'{base}/api/fonts', timeout=10) as r:
        data = json.load(r)
    have = {}
    for fam in data.get('families', []):
        for f in fam.get('files', []):
            have[(fam.get('name'), f.get('name'))] = f.get('size')
    return have


def upload(base: str, family: str, path: Path):
    boundary = uuid.uuid4().hex
    body = b''.join([
        f'--{boundary}\r\nContent-Disposition: form-data; name="family"\r\n\r\n{family}\r\n'.encode(),
        f'--{boundary}\r\nContent-Disposition: form-data; name="file"; filename="{path.name}"\r\n'
        'Content-Type: application/octet-stream\r\n\r\n'.encode(),
        path.read_bytes(),
        f'\r\n--{boundary}--\r\n'.encode(),
    ])
    req = urllib.request.Request(f'{base}/api/fonts/upload', data=body, method='POST',
                                 headers={'Content-Type': f'multipart/form-data; boundary={boundary}'})
    with urllib.request.urlopen(req, timeout=120) as r:
        if r.status >= 300:
            raise RuntimeError(f'{path.name}: HTTP {r.status}')


def install_device(host: str):
    base = host if host.startswith('http') else f'http://{host}'
    have = installed_sizes(base)
    jobs = [(family, f) for family, files, _ in font_files() for f in files
            if have.get((family, f.name)) != f.stat().st_size]
    total = len(jobs)
    for i, (family, f) in enumerate(jobs, 1):
        print(f'[{i}/{total}] {family}/{f.name}', flush=True)
        upload(base, family, f)
    print(f'Uploaded {total} files.' if total else 'All fonts already on the reader.')


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument('--sd', type=Path, help='mounted SD card root')
    g.add_argument('--device', help='reader address, e.g. crosspoint.local or 192.168.4.1')
    a = p.parse_args()
    if a.sd:
        install_sd(a.sd)
    else:
        install_device(a.device)


if __name__ == '__main__':
    main()
