#!/usr/bin/env python3
"""Emit Wren's pixel icons as C++ row masks.

Source: the "Wren Reading" page of the Wren design canvas. Each icon is the SVG path the canvas draws with
shape-rendering="crispEdges" (only axis-aligned `Mx yhWvHh-Wz` rectangles), at its native grid. Code scales
them by whole pixels.

Usage: python scripts/wren/build_icons.py
Output: src/wren/ui/WrenIcons.generated.h
"""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "src" / "wren" / "ui" / "WrenIcons.generated.h"

# name, width, height, path
ICONS = [
    ("ChevronRight", 3, 6, "M0 0h1v6h-1zM1 1h1v4h-1zM2 2h1v2h-1z"),
    ("ChevronLeft", 3, 6, "M0 2h1v2h-1zM1 1h1v4h-1zM2 0h1v6h-1z"),
    ("ArrowUp", 6, 3, "M2 0h2v1h-2zM1 1h4v1h-4zM0 2h6v1h-6z"),
    ("ArrowDown", 6, 3, "M0 0h6v1h-6zM1 1h4v1h-4zM2 2h2v1h-2z"),
    ("Bookmark", 12, 12,
     "M2 0h8v1h-8zM2 1h1v11h-1zM9 1h1v11h-1zM3 10h1v1h-1zM4 9h1v1h-1zM5 8h2v1h-2zM7 9h1v1h-1zM8 10h1v1h-1z"
     "M5 2h2v4h-2zM4 3h4v2h-4z"),
    ("Home", 12, 12, "M0 1h1v10h-1zM1 1h4v1h-4zM1 10h4v1h-4zM3 5h5v2h-5zM8 3h1v6h-1zM9 4h1v4h-1zM10 5h1v2h-1z"),
    ("Refresh", 32, 32,
     "M15 1h1v1h-1zM15 2h2v1h-2zM15 3h4v1h-4zM15 4h6v1h-6zM12 5h11v1h-11zM10 6h13v1h-13zM9 7h4v1h-4z"
     "M15 7h7v1h-7zM8 8h3v1h-3zM15 8h5v1h-5zM7 9h3v1h-3zM15 9h3v1h-3zM6 10h3v1h-3zM15 10h1v1h-1z"
     "M6 11h2v1h-2zM5 12h3v1h-3zM5 13h2v1h-2zM25 13h2v1h-2zM5 14h2v1h-2zM25 14h2v1h-2zM5 15h2v1h-2z"
     "M25 15h2v1h-2zM5 16h2v1h-2zM25 16h2v1h-2zM5 17h2v1h-2zM25 17h2v1h-2zM5 18h2v1h-2zM25 18h2v1h-2z"
     "M5 19h3v1h-3zM24 19h3v1h-3zM6 20h2v1h-2zM24 20h2v1h-2zM6 21h3v1h-3zM23 21h3v1h-3zM7 22h3v1h-3z"
     "M22 22h3v1h-3zM8 23h3v1h-3zM21 23h3v1h-3zM9 24h4v1h-4zM19 24h4v1h-4zM10 25h12v1h-12zM12 26h8v1h-8z"),
    # Library page of the canvas.
    ("Wren", 32, 32,
     "M6 2h2v1h-2zM6 3h3v1h-3zM16 3h3v1h-3zM6 4h4v1h-4zM15 4h5v1h-5zM6 5h4v1h-4zM14 5h7v1h-7zM7 6h4v1h-4z"
     "M14 6h9v1h-9zM7 7h14v1h-14zM8 8h12v1h-12zM8 9h11v1h-11zM8 10h11v1h-11zM8 11h11v1h-11zM8 12h11v1h-11z"
     "M9 13h9v1h-9zM10 14h7v1h-7zM13 15h1v1h-1zM17 15h1v1h-1zM13 16h1v1h-1zM17 16h1v1h-1zM13 17h1v1h-1z"
     "M17 17h1v1h-1zM7 18h18v1h-18zM7 19h18v1h-18zM7 20h2v1h-2zM23 20h2v1h-2zM7 21h2v1h-2zM23 21h2v1h-2z"
     "M7 22h2v1h-2zM11 22h14v1h-14zM7 23h2v1h-2zM23 23h2v1h-2zM7 24h2v1h-2zM11 24h14v1h-14zM7 25h2v1h-2z"
     "M23 25h2v1h-2zM7 26h2v1h-2zM11 26h8v1h-8zM23 26h2v1h-2zM7 27h2v1h-2zM23 27h2v1h-2zM7 28h2v1h-2z"
     "M23 28h2v1h-2zM7 29h7v1h-7zM18 29h7v1h-7zM7 30h7v1h-7zM18 30h7v1h-7zM7 31h18v1h-18z"),
    ("WrenDone", 32, 32,
     "M27 0h1v1h-1zM27 1h1v1h-1zM26 2h3v1h-3zM18 3h1v1h-1zM24 3h7v1h-7zM17 4h3v1h-3zM26 4h3v1h-3zM18 5h1v1h-1z"
     "M27 5h1v1h-1zM27 6h1v1h-1zM2 9h3v1h-3zM31 9h1v1h-1zM2 10h4v1h-4zM2 11h5v1h-5zM15 11h5v1h-5zM24 11h1v1h-1z"
     "M2 12h5v1h-5zM14 12h10v1h-10zM2 13h6v1h-6zM13 13h4v1h-4zM19 13h4v1h-4zM3 14h5v1h-5zM13 14h3v1h-3z"
     "M17 14h2v1h-2zM20 14h2v1h-2zM4 15h18v1h-18zM4 16h20v1h-20zM5 17h18v1h-18zM5 18h15v1h-15zM5 19h2v1h-2z"
     "M9 19h10v1h-10zM5 20h2v1h-2zM11 20h8v1h-8zM5 21h3v1h-3zM10 21h1v1h-1zM13 21h6v1h-6zM5 22h4v1h-4z"
     "M11 22h1v1h-1zM14 22h5v1h-5zM6 23h5v1h-5zM12 23h2v1h-2zM15 23h3v1h-3zM7 24h10v1h-10zM8 25h8v1h-8z"
     "M11 26h1v1h-1zM15 26h1v1h-1zM11 27h1v1h-1zM15 27h1v1h-1zM11 28h1v1h-1zM15 28h1v1h-1zM9 29h3v1h-3z"
     "M13 29h3v1h-3z"),
    ("Trash", 32, 32,
     "M12 3h8v1h-8zM12 4h8v1h-8zM12 5h2v1h-2zM18 5h2v1h-2zM12 6h2v1h-2zM18 6h2v1h-2zM4 7h24v1h-24zM4 8h24v1h-24z"
     "M6 10h20v1h-20zM6 11h2v1h-2zM23 11h3v1h-3zM6 12h3v1h-3zM23 12h3v1h-3zM6 13h3v1h-3zM23 13h3v1h-3z"
     "M6 14h3v1h-3zM13 14h2v1h-2zM18 14h2v1h-2zM23 14h3v1h-3zM7 15h2v1h-2zM13 15h2v1h-2zM18 15h2v1h-2z"
     "M23 15h2v1h-2zM7 16h2v1h-2zM13 16h2v1h-2zM18 16h2v1h-2zM23 16h2v1h-2zM7 17h2v1h-2zM13 17h2v1h-2z"
     "M18 17h2v1h-2zM23 17h2v1h-2zM7 18h2v1h-2zM13 18h2v1h-2zM18 18h2v1h-2zM23 18h2v1h-2zM7 19h2v1h-2z"
     "M13 19h2v1h-2zM18 19h2v1h-2zM23 19h2v1h-2zM7 20h2v1h-2zM13 20h2v1h-2zM18 20h2v1h-2zM23 20h2v1h-2z"
     "M7 21h2v1h-2zM13 21h2v1h-2zM18 21h2v1h-2zM23 21h2v1h-2zM7 22h3v1h-3zM13 22h2v1h-2zM18 22h2v1h-2z"
     "M22 22h3v1h-3zM7 23h3v1h-3zM13 23h2v1h-2zM18 23h2v1h-2zM22 23h3v1h-3zM8 24h2v1h-2zM13 24h2v1h-2z"
     "M18 24h2v1h-2zM22 24h2v1h-2zM8 25h2v1h-2zM22 25h2v1h-2zM8 26h2v1h-2zM22 26h2v1h-2zM8 27h16v1h-16z"
     "M8 28h16v1h-16z"),
    ("EmptyShelf", 32, 32,
     "M20 8h2v1h-2zM24 8h2v1h-2zM28 8h2v1h-2zM20 9h1v1h-1zM29 9h1v1h-1zM0 12h2v1h-2zM20 12h1v1h-1zM29 12h1v1h-1z"
     "M0 13h3v1h-3zM20 13h1v1h-1zM29 13h1v1h-1zM0 14h4v1h-4zM11 14h4v1h-4zM0 15h4v1h-4zM10 15h6v1h-6z"
     "M0 16h5v1h-5zM9 16h4v1h-4zM15 16h2v1h-2zM20 16h1v1h-1zM29 16h1v1h-1zM1 17h4v1h-4zM9 17h4v1h-4z"
     "M15 17h4v1h-4zM20 17h1v1h-1zM24 17h1v1h-1zM29 17h1v1h-1zM1 18h17v1h-17zM23 18h3v1h-3zM2 19h15v1h-15z"
     "M24 19h1v1h-1zM2 20h14v1h-14zM20 20h1v1h-1zM29 20h1v1h-1zM2 21h3v1h-3zM6 21h9v1h-9zM20 21h1v1h-1z"
     "M29 21h1v1h-1zM2 22h2v1h-2zM8 22h7v1h-7zM2 23h3v1h-3zM9 23h6v1h-6zM3 24h3v1h-3zM8 24h1v1h-1z"
     "M11 24h3v1h-3zM20 24h1v1h-1zM29 24h1v1h-1zM3 25h11v1h-11zM20 25h1v1h-1zM29 25h1v1h-1zM4 26h9v1h-9z"
     "M7 27h3v1h-3zM11 27h1v1h-1zM8 28h1v1h-1zM11 28h1v1h-1zM20 28h1v1h-1zM29 28h1v1h-1zM8 29h1v1h-1z"
     "M11 29h1v1h-1zM20 29h2v1h-2zM24 29h2v1h-2zM28 29h2v1h-2zM0 30h32v1h-32zM0 31h32v1h-32z"),
    ("ChevronBig", 32, 32,
     "M9 5h4v1h-4zM9 6h5v1h-5zM9 7h6v1h-6zM11 8h5v1h-5zM12 9h5v1h-5zM13 10h5v1h-5zM14 11h5v1h-5zM15 12h5v1h-5z"
     "M16 13h5v1h-5zM17 14h5v1h-5zM18 15h4v1h-4zM19 16h3v1h-3zM18 17h4v1h-4zM17 18h5v1h-5zM16 19h5v1h-5z"
     "M15 20h5v1h-5zM14 21h5v1h-5zM13 22h5v1h-5zM12 23h5v1h-5zM11 24h5v1h-5zM10 25h5v1h-5zM9 26h5v1h-5z"
     "M9 27h4v1h-4zM9 28h3v1h-3z"),
    ("WrenScanning", 32, 32,
     "M3 8h3v1h-3zM3 9h4v1h-4zM3 10h4v1h-4zM3 11h5v1h-5zM3 12h6v1h-6zM3 13h6v1h-6zM4 14h6v1h-6zM4 15h7v1h-7z"
     "M5 16h6v1h-6zM17 16h3v1h-3zM5 17h8v1h-8zM15 17h6v1h-6zM5 18h17v1h-17zM5 19h2v1h-2zM9 19h14v1h-14z"
     "M4 20h3v1h-3zM10 20h9v1h-9zM21 20h2v1h-2zM4 21h4v1h-4zM10 21h9v1h-9zM21 21h2v1h-2zM4 22h4v1h-4z"
     "M11 22h12v1h-12zM5 23h3v1h-3zM9 23h1v1h-1zM11 23h13v1h-13zM5 24h4v1h-4zM12 24h9v1h-9zM24 24h1v1h-1z"
     "M6 25h5v1h-5zM12 25h6v1h-6zM7 26h4v1h-4zM12 26h5v1h-5zM8 27h8v1h-8zM10 28h5v1h-5zM10 29h1v1h-1z"
     "M14 29h1v1h-1zM28 29h1v1h-1zM8 30h3v1h-3zM12 30h3v1h-3zM25 30h1v1h-1zM30 30h1v1h-1z"),
    ("WrenGlasses", 32, 32,
     "M3 4h3v1h-3zM3 5h4v1h-4zM3 6h5v1h-5zM21 6h1v1h-1zM3 7h5v1h-5zM18 7h7v1h-7zM3 8h6v1h-6zM17 8h3v1h-3z"
     "M25 8h1v1h-1zM3 9h6v1h-6zM18 9h1v1h-1zM21 9h3v1h-3zM3 10h7v1h-7zM16 10h2v1h-2zM20 10h5v1h-5z"
     "M26 10h1v1h-1zM4 11h7v1h-7zM16 11h3v1h-3zM20 11h2v1h-2zM24 11h1v1h-1zM26 11h4v1h-4zM4 12h7v1h-7z"
     "M12 12h7v1h-7zM20 12h2v1h-2zM24 12h1v1h-1zM26 12h4v1h-4zM5 13h14v1h-14zM21 13h3v1h-3zM26 13h1v1h-1z"
     "M5 14h15v1h-15zM25 14h1v1h-1zM6 15h20v1h-20zM7 16h18v1h-18zM6 17h4v1h-4zM12 17h12v1h-12zM6 18h3v1h-3z"
     "M10 18h1v1h-1zM13 18h11v1h-11zM6 19h4v1h-4zM12 19h1v1h-1zM15 19h9v1h-9zM6 20h5v1h-5zM13 20h2v1h-2z"
     "M17 20h7v1h-7zM7 21h6v1h-6zM14 21h2v1h-2zM18 21h5v1h-5zM7 22h7v1h-7zM15 22h3v1h-3zM19 22h4v1h-4z"
     "M8 23h14v1h-14zM9 24h12v1h-12zM12 25h6v1h-6zM19 25h1v1h-1zM14 26h1v1h-1zM19 26h1v1h-1zM14 27h1v1h-1z"
     "M19 27h1v1h-1zM14 28h1v1h-1zM19 28h1v1h-1zM12 29h3v1h-3zM17 29h3v1h-3z"),
    # Setup page of the canvas (Files).
    ("Folder", 32, 32,
     "M3 5h10v1h-10zM3 6h11v1h-11zM3 7h2v1h-2zM12 7h3v1h-3zM3 8h2v1h-2zM13 8h16v1h-16zM3 9h2v1h-2z"
     "M14 9h15v1h-15zM3 10h2v1h-2zM27 10h2v1h-2zM3 11h2v1h-2zM27 11h2v1h-2zM3 12h2v1h-2zM27 12h2v1h-2z"
     "M3 13h2v1h-2zM27 13h2v1h-2zM3 14h2v1h-2zM27 14h2v1h-2zM3 15h2v1h-2zM27 15h2v1h-2zM3 16h2v1h-2z"
     "M27 16h2v1h-2zM3 17h2v1h-2zM27 17h2v1h-2zM3 18h2v1h-2zM27 18h2v1h-2zM3 19h2v1h-2zM27 19h2v1h-2z"
     "M3 20h2v1h-2zM27 20h2v1h-2zM3 21h2v1h-2zM27 21h2v1h-2zM3 22h2v1h-2zM27 22h2v1h-2zM3 23h2v1h-2z"
     "M27 23h2v1h-2zM3 24h2v1h-2zM27 24h2v1h-2zM3 25h26v1h-26zM3 26h26v1h-26z"),
    ("BookFile", 32, 32,
     "M7 3h17v1h-17zM7 4h18v1h-18zM7 5h5v1h-5zM23 5h3v1h-3zM7 6h5v1h-5zM24 6h2v1h-2zM7 7h5v1h-5z"
     "M24 7h2v1h-2zM7 8h5v1h-5zM24 8h2v1h-2zM7 9h5v1h-5zM15 9h7v1h-7zM24 9h2v1h-2zM7 10h5v1h-5z"
     "M15 10h7v1h-7zM24 10h2v1h-2zM7 11h5v1h-5zM24 11h2v1h-2zM7 12h5v1h-5zM24 12h2v1h-2zM7 13h5v1h-5z"
     "M15 13h7v1h-7zM24 13h2v1h-2zM7 14h5v1h-5zM15 14h7v1h-7zM24 14h2v1h-2zM7 15h5v1h-5zM24 15h2v1h-2z"
     "M7 16h5v1h-5zM24 16h2v1h-2zM7 17h5v1h-5zM24 17h2v1h-2zM7 18h5v1h-5zM24 18h2v1h-2zM7 19h5v1h-5z"
     "M24 19h2v1h-2zM7 20h5v1h-5zM24 20h2v1h-2zM7 21h5v1h-5zM24 21h2v1h-2zM7 22h5v1h-5zM24 22h2v1h-2z"
     "M7 23h5v1h-5zM24 23h2v1h-2zM7 24h5v1h-5zM24 24h2v1h-2zM7 25h5v1h-5zM24 25h2v1h-2zM7 26h5v1h-5z"
     "M23 26h3v1h-3zM7 27h18v1h-18zM7 28h17v1h-17z"),
    ("ImageFile", 32, 32,
     "M5 5h22v1h-22zM4 6h24v1h-24zM3 7h3v1h-3zM26 7h3v1h-3zM3 8h2v1h-2zM27 8h2v1h-2zM3 9h2v1h-2z"
     "M20 9h4v1h-4zM27 9h2v1h-2zM3 10h2v1h-2zM20 10h4v1h-4zM27 10h2v1h-2zM3 11h2v1h-2zM20 11h4v1h-4z"
     "M27 11h2v1h-2zM3 12h2v1h-2zM20 12h4v1h-4zM27 12h2v1h-2zM3 13h2v1h-2zM27 13h2v1h-2zM3 14h2v1h-2z"
     "M27 14h2v1h-2zM3 15h2v1h-2zM27 15h2v1h-2zM3 16h2v1h-2zM10 16h2v1h-2zM27 16h2v1h-2zM3 17h2v1h-2z"
     "M9 17h4v1h-4zM19 17h2v1h-2zM27 17h2v1h-2zM3 18h2v1h-2zM9 18h5v1h-5zM18 18h4v1h-4zM27 18h2v1h-2z"
     "M3 19h2v1h-2zM8 19h7v1h-7zM17 19h6v1h-6zM27 19h2v1h-2zM3 20h2v1h-2zM7 20h16v1h-16zM27 20h2v1h-2z"
     "M3 21h2v1h-2zM7 21h17v1h-17zM27 21h2v1h-2zM3 22h2v1h-2zM6 22h19v1h-19zM27 22h2v1h-2zM3 23h23v1h-23z"
     "M27 23h2v1h-2zM3 24h3v1h-3zM26 24h3v1h-3zM4 25h24v1h-24zM5 26h22v1h-22z"),
    # System page of the canvas (sleep, boot, no card, crash, battery empty).
    ("WrenSleeping", 32, 32,
     "M25 0h5v1h-5zM28 1h1v1h-1zM27 2h1v1h-1zM26 3h1v1h-1zM25 4h5v1h-5zM19 5h3v1h-3zM21 6h1v1h-1z"
     "M20 7h1v1h-1zM19 8h3v1h-3zM2 9h3v1h-3zM2 10h4v1h-4zM2 11h5v1h-5zM15 11h5v1h-5zM2 12h5v1h-5z"
     "M14 12h7v1h-7zM2 13h6v1h-6zM13 13h3v1h-3zM17 13h2v1h-2zM20 13h1v1h-1zM3 14h5v1h-5zM13 14h4v1h-4z"
     "M19 14h4v1h-4zM4 15h20v1h-20zM4 16h17v1h-17zM5 17h16v1h-16zM5 18h15v1h-15zM5 19h2v1h-2z"
     "M9 19h10v1h-10zM5 20h2v1h-2zM11 20h8v1h-8zM5 21h3v1h-3zM10 21h1v1h-1zM13 21h6v1h-6zM5 22h4v1h-4z"
     "M11 22h1v1h-1zM14 22h5v1h-5zM6 23h5v1h-5zM12 23h2v1h-2zM15 23h3v1h-3zM7 24h10v1h-10zM8 25h8v1h-8z"
     "M11 26h1v1h-1zM15 26h1v1h-1zM11 27h1v1h-1zM15 27h1v1h-1zM11 28h1v1h-1zM15 28h1v1h-1zM9 29h3v1h-3z"
     "M13 29h3v1h-3z"),
    ("WrenHello", 32, 32,
     "M3 4h3v1h-3zM3 5h4v1h-4zM3 6h5v1h-5zM21 6h1v1h-1zM3 7h5v1h-5zM18 7h7v1h-7zM3 8h6v1h-6zM17 8h9v1h-9z"
     "M3 9h6v1h-6zM17 9h9v1h-9zM3 10h7v1h-7zM16 10h6v1h-6zM24 10h3v1h-3zM4 11h7v1h-7zM16 11h6v1h-6z"
     "M24 11h6v1h-6zM4 12h7v1h-7zM12 12h18v1h-18zM5 13h22v1h-22zM5 14h21v1h-21zM6 15h20v1h-20z"
     "M7 16h18v1h-18zM6 17h4v1h-4zM12 17h12v1h-12zM6 18h3v1h-3zM10 18h1v1h-1zM13 18h11v1h-11zM6 19h4v1h-4z"
     "M12 19h1v1h-1zM15 19h9v1h-9zM6 20h5v1h-5zM13 20h2v1h-2zM17 20h7v1h-7zM7 21h6v1h-6zM14 21h2v1h-2z"
     "M18 21h5v1h-5zM7 22h7v1h-7zM15 22h3v1h-3zM19 22h4v1h-4zM8 23h14v1h-14zM9 24h12v1h-12zM12 25h6v1h-6z"
     "M19 25h1v1h-1zM14 26h1v1h-1zM19 26h1v1h-1zM14 27h1v1h-1zM19 27h1v1h-1zM14 28h1v1h-1zM19 28h1v1h-1z"
     "M12 29h3v1h-3zM17 29h3v1h-3z"),
    ("WrenNoCard", 32, 32,
     "M25 0h6v1h-6zM24 1h1v1h-1zM30 1h1v1h-1zM23 2h1v1h-1zM26 2h1v1h-1zM28 2h1v1h-1zM30 2h1v1h-1z"
     "M23 3h1v1h-1zM26 3h1v1h-1zM28 3h1v1h-1zM30 3h1v1h-1zM23 4h1v1h-1zM30 4h1v1h-1zM23 5h1v1h-1z"
     "M30 5h1v1h-1zM23 6h1v1h-1zM30 6h1v1h-1zM23 7h1v1h-1zM30 7h1v1h-1zM23 8h1v1h-1zM30 8h1v1h-1z"
     "M2 9h3v1h-3zM23 9h8v1h-8zM2 10h4v1h-4zM2 11h5v1h-5zM15 11h5v1h-5zM2 12h5v1h-5zM14 12h3v1h-3z"
     "M19 12h2v1h-2zM2 13h6v1h-6zM13 13h3v1h-3zM20 13h1v1h-1zM3 14h5v1h-5zM13 14h3v1h-3zM19 14h4v1h-4z"
     "M4 15h13v1h-13zM19 15h5v1h-5zM4 16h17v1h-17zM5 17h16v1h-16zM5 18h15v1h-15zM5 19h2v1h-2z"
     "M9 19h10v1h-10zM5 20h2v1h-2zM11 20h8v1h-8zM5 21h3v1h-3zM10 21h1v1h-1zM13 21h6v1h-6zM5 22h4v1h-4z"
     "M11 22h1v1h-1zM14 22h5v1h-5zM6 23h5v1h-5zM12 23h2v1h-2zM15 23h3v1h-3zM7 24h10v1h-10zM8 25h8v1h-8z"
     "M11 26h1v1h-1zM15 26h1v1h-1zM11 27h1v1h-1zM15 27h1v1h-1zM11 28h1v1h-1zM15 28h1v1h-1zM9 29h3v1h-3z"
     "M13 29h3v1h-3z"),
    ("WrenConfused", 32, 32,
     "M26 0h2v1h-2zM26 1h2v1h-2zM26 2h2v1h-2zM26 3h2v1h-2zM26 4h2v1h-2zM17 5h1v1h-1zM26 5h2v1h-2z"
     "M17 6h1v1h-1zM12 7h1v1h-1zM17 7h1v1h-1zM21 7h2v1h-2zM26 7h2v1h-2zM12 8h2v1h-2zM17 8h1v1h-1z"
     "M20 8h2v1h-2zM26 8h2v1h-2zM2 9h3v1h-3zM13 9h1v1h-1zM17 9h1v1h-1zM20 9h1v1h-1zM2 10h4v1h-4z"
     "M14 10h1v1h-1zM20 10h1v1h-1zM2 11h5v1h-5zM15 11h5v1h-5zM2 12h5v1h-5zM14 12h3v1h-3zM19 12h2v1h-2z"
     "M2 13h6v1h-6zM13 13h3v1h-3zM20 13h1v1h-1zM3 14h5v1h-5zM13 14h3v1h-3zM19 14h4v1h-4zM4 15h13v1h-13z"
     "M19 15h5v1h-5zM4 16h17v1h-17zM5 17h16v1h-16zM5 18h15v1h-15zM5 19h2v1h-2zM9 19h10v1h-10zM5 20h2v1h-2z"
     "M11 20h8v1h-8zM5 21h3v1h-3zM10 21h1v1h-1zM13 21h6v1h-6zM5 22h4v1h-4zM11 22h1v1h-1zM14 22h5v1h-5z"
     "M6 23h5v1h-5zM12 23h2v1h-2zM15 23h3v1h-3zM7 24h10v1h-10zM8 25h8v1h-8zM11 26h1v1h-1zM15 26h1v1h-1z"
     "M11 27h1v1h-1zM15 27h1v1h-1zM11 28h1v1h-1zM15 28h1v1h-1zM9 29h3v1h-3zM13 29h3v1h-3z"),
    ("WrenLowBattery", 32, 32,
     "M18 2h11v1h-11zM18 3h1v5h-1zM28 3h1v5h-1zM20 4h2v2h-2zM29 4h1v2h-1zM19 7h9v1h-9zM15 11h5v3h-5z"
     "M14 12h1v11h-1zM20 12h1v6h-1zM13 13h1v9h-1zM15 14h2v11h-2zM19 14h1v5h-1zM21 14h2v2h-2zM9 15h4v5h-4z"
     "M17 15h2v8h-2zM23 15h1v1h-1zM7 16h2v3h-2zM6 17h1v7h-1zM5 18h1v7h-1zM11 20h2v1h-2zM3 21h2v6h-2z"
     "M7 21h1v4h-1zM10 21h1v1h-1zM8 22h1v4h-1zM11 22h1v1h-1zM9 23h2v3h-2zM12 23h2v3h-2zM17 23h1v1h-1z"
     "M11 24h1v6h-1zM14 24h1v2h-1zM2 25h1v2h-1zM15 25h1v5h-1zM9 29h2v1h-2zM13 29h2v1h-2z"),
    ("WakeArrow", 9, 12, "M4 0h1v1h-1zM3 1h3v1h-3zM2 2h5v1h-5zM1 3h7v1h-7zM4 4h1v4h-1z"),
    # Library tab header: wren-classic from wren-icons/source/wren_icons.json.
    ("WrenClassic", 32, 32,
     "M3 4h3v1h-3zM3 5h4v1h-4zM3 6h5v1h-5zM21 6h1v1h-1zM3 7h5v1h-5zM18 7h7v1h-7zM3 8h6v1h-6zM17 8h9v1h-9z"
     "M3 9h6v1h-6zM17 9h9v1h-9zM3 10h7v1h-7zM16 10h6v1h-6zM24 10h3v1h-3zM4 11h7v1h-7zM16 11h6v1h-6z"
     "M24 11h6v1h-6zM4 12h7v1h-7zM12 12h18v1h-18zM5 13h22v1h-22zM5 14h21v1h-21zM6 15h20v1h-20z"
     "M7 16h18v1h-18zM6 17h4v1h-4zM12 17h12v1h-12zM6 18h3v1h-3zM10 18h1v1h-1zM13 18h11v1h-11zM6 19h4v1h-4z"
     "M12 19h1v1h-1zM15 19h9v1h-9zM6 20h5v1h-5zM13 20h2v1h-2zM17 20h7v1h-7zM7 21h6v1h-6zM14 21h2v1h-2z"
     "M18 21h5v1h-5zM7 22h7v1h-7zM15 22h3v1h-3zM19 22h4v1h-4zM8 23h14v1h-14zM9 24h12v1h-12zM12 25h6v1h-6z"
     "M19 25h1v1h-1zM14 26h1v1h-1zM19 26h1v1h-1zM14 27h1v1h-1zM19 27h1v1h-1zM14 28h1v1h-1zM19 28h1v1h-1z"
     "M12 29h3v1h-3zM17 29h3v1h-3z"),
    # Tab header (send tab), from the design canvas.
    ("WrenSend", 32, 32,
     "M21 1h8v1h-8zM19 2h3v1h-3zM28 2h3v1h-3zM18 3h2v1h-2zM30 3h2v1h-2zM17 4h2v1h-2zM22 4h6v1h-6z"
     "M31 4h1v1h-1zM20 5h3v1h-3zM27 5h3v1h-3zM20 6h1v1h-1zM29 6h1v1h-1zM23 7h4v1h-4zM22 8h2v1h-2z"
     "M26 8h2v1h-2zM2 9h3v1h-3zM2 10h4v1h-4zM24 10h2v1h-2zM2 11h5v1h-5zM15 11h5v1h-5zM24 11h2v1h-2z"
     "M2 12h5v1h-5zM14 12h7v1h-7zM2 13h6v1h-6zM13 13h4v1h-4zM19 13h2v1h-2zM3 14h5v1h-5zM13 14h4v1h-4z"
     "M19 14h4v1h-4zM4 15h20v1h-20zM4 16h17v1h-17zM5 17h16v1h-16zM5 18h15v1h-15zM5 19h2v1h-2z"
     "M9 19h10v1h-10zM5 20h2v1h-2zM11 20h8v1h-8zM5 21h3v1h-3zM10 21h1v1h-1zM13 21h6v1h-6zM5 22h4v1h-4z"
     "M11 22h1v1h-1zM14 22h5v1h-5zM6 23h5v1h-5zM12 23h2v1h-2zM15 23h3v1h-3zM7 24h10v1h-10zM8 25h8v1h-8z"
     "M11 26h1v1h-1zM15 26h1v1h-1zM11 27h1v1h-1zM15 27h1v1h-1zM11 28h1v1h-1zM15 28h1v1h-1zM9 29h3v1h-3z"
     "M13 29h3v1h-3z"),
    # Tab header (setup tab), from the design canvas.
    ("WrenSetup", 32, 32,
     "M24 0h3v1h-3zM21 1h2v1h-2zM24 1h3v1h-3zM28 1h2v1h-2zM21 2h2v1h-2zM24 2h3v1h-3zM28 2h2v1h-2z"
     "M23 3h5v1h-5zM22 4h2v1h-2zM27 4h4v1h-4zM20 5h4v1h-4zM27 5h4v1h-4zM20 6h4v1h-4zM27 6h4v1h-4z"
     "M23 7h5v1h-5zM21 8h2v1h-2zM24 8h3v1h-3zM28 8h2v1h-2zM2 9h3v1h-3zM21 9h2v1h-2zM24 9h3v1h-3z"
     "M28 9h2v1h-2zM2 10h4v1h-4zM24 10h3v1h-3zM2 11h5v1h-5zM15 11h5v1h-5zM2 12h5v1h-5zM14 12h7v1h-7z"
     "M2 13h6v1h-6zM13 13h4v1h-4zM19 13h2v1h-2zM3 14h5v1h-5zM13 14h4v1h-4zM19 14h4v1h-4zM4 15h20v1h-20z"
     "M4 16h17v1h-17zM5 17h16v1h-16zM5 18h15v1h-15zM5 19h2v1h-2zM9 19h10v1h-10zM5 20h2v1h-2zM11 20h8v1h-8z"
     "M5 21h3v1h-3zM10 21h1v1h-1zM13 21h6v1h-6zM5 22h4v1h-4zM11 22h1v1h-1zM14 22h5v1h-5zM6 23h5v1h-5z"
     "M12 23h2v1h-2zM15 23h3v1h-3zM7 24h10v1h-10zM8 25h8v1h-8zM11 26h1v1h-1zM15 26h1v1h-1zM11 27h1v1h-1z"
     "M15 27h1v1h-1zM11 28h1v1h-1zM15 28h1v1h-1zM9 29h3v1h-3zM13 29h3v1h-3z"),
]

RECT = re.compile(r"M(\d+) (\d+)h(\d+)v(\d+)h-(\d+)z")


def rows(width, height, path):
    grid = [0] * height
    consumed = 0
    for m in RECT.finditer(path):
        x, y, w, h, back = map(int, m.groups())
        if back != w:
            raise ValueError(f"not a rectangle: {m.group(0)}")
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                if xx >= width or yy >= height:
                    raise ValueError(f"outside {width}x{height}: {m.group(0)}")
                grid[yy] |= 1 << (width - 1 - xx)
        consumed += len(m.group(0))
    if consumed != len(path):
        raise ValueError("path has commands other than rectangles")
    return grid


def main():
    out = [
        "// Generated by scripts/wren/build_icons.py. Do not edit.",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace wren::ui {",
        "",
        "// One row per uint32_t, bit (width - 1 - x) set = ink at column x.",
        "struct Icon {",
        "  uint8_t width;",
        "  uint8_t height;",
        "  const uint32_t* rows;",
        "};",
        "",
        "namespace icons {",
    ]
    for name, width, height, path in ICONS:
        data = ", ".join(f"0x{r:X}" for r in rows(width, height, path))
        out.append(f"inline constexpr uint32_t k{name}Rows[] = {{{data}}};")
        out.append(f"inline constexpr Icon {name}{{{width}, {height}, k{name}Rows}};")
    out += ["}  // namespace icons", "", "}  // namespace wren::ui", ""]
    OUT.write_text("\n".join(out))
    print(OUT.relative_to(ROOT))


if __name__ == "__main__":
    main()
