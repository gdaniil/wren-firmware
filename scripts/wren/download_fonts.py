#!/usr/bin/env python3
"""Download the pinned Google Fonts sources and their OFL licenses."""

from pathlib import Path
from urllib.parse import quote
from urllib.request import urlopen

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / ".cache" / "wren-fonts"
REVISION = "29b4ec6df2522a8aee279414b78a07910b065874"
FILES = {
    "literata": ("Literata[opsz,wght].ttf", "Literata-Italic[opsz,wght].ttf", "OFL.txt"),
    "golostext": ("GolosText[wght].ttf", "OFL.txt"),
    "bitter": ("Bitter[wght].ttf", "Bitter-Italic[wght].ttf", "OFL.txt"),
}


def main():
    for family, names in FILES.items():
        folder = CACHE / family
        folder.mkdir(parents=True, exist_ok=True)
        for name in names:
            path = folder / name
            if not path.exists():
                url = f"https://raw.githubusercontent.com/google/fonts/{REVISION}/ofl/{family}/{quote(name)}"
                with urlopen(url, timeout=60) as response:
                    data = response.read()
                path.write_bytes(data)
            print(path.relative_to(ROOT))


if __name__ == "__main__":
    main()
