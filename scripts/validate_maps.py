#!/usr/bin/env python3
"""Validate the static stage maps before building or publishing the game."""

from __future__ import annotations

import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAP_DIR = ROOT / "data"
ALLOWED_CELLS = set("01234")


def validate_map(path: Path) -> tuple[int, int]:
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines:
        raise ValueError("map is empty")

    width = len(lines[0])
    if width == 0 or any(len(line) != width for line in lines):
        raise ValueError("rows must be non-empty and have the same width")

    invalid = sorted({cell for line in lines for cell in line} - ALLOWED_CELLS)
    if invalid:
        raise ValueError(f"invalid cell value(s): {', '.join(invalid)}")

    positions = {
        (row, col)
        for row, line in enumerate(lines)
        for col, cell in enumerate(line)
        if cell in "34"
    }
    heads = [
        (row, col)
        for row, line in enumerate(lines)
        for col, cell in enumerate(line)
        if cell == "3"
    ]
    body_count = sum(line.count("4") for line in lines)

    if len(heads) != 1:
        raise ValueError(f"expected one snake head, found {len(heads)}")
    if body_count < 2:
        raise ValueError(f"expected at least two body cells, found {body_count}")
    if sum(line.count("1") for line in lines) < 2:
        raise ValueError("at least two normal wall cells are required for gates")

    visited = {heads[0]}
    pending = [heads[0]]
    while pending:
        row, col = pending.pop()
        for neighbor in ((row - 1, col), (row + 1, col), (row, col - 1), (row, col + 1)):
            if neighbor in positions and neighbor not in visited:
                visited.add(neighbor)
                pending.append(neighbor)

    if visited != positions:
        raise ValueError("snake head and body cells must form one connected chain")

    return len(lines), width


def main() -> int:
    expected = [MAP_DIR / f"map{stage}.txt" for stage in range(1, 6)]
    failed = False

    for path in expected:
        try:
            if not path.is_file():
                raise ValueError("file not found")
            height, width = validate_map(path)
            print(f"OK  {path.relative_to(ROOT)} ({width}x{height})")
        except (OSError, UnicodeError, ValueError) as error:
            failed = True
            print(f"ERROR  {path.relative_to(ROOT)}: {error}", file=sys.stderr)

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
