#!/usr/bin/env python3
"""Inline the reflect_moc headers a source file includes, producing one file
that Compiler Explorer builds with no include path.

Usage: amalgamate.py <source.cpp> <include-dir> <output.cpp>
"""
import re
import sys
from pathlib import Path

INCLUDE = re.compile(r"^#include <(reflect_moc/[^>]+)>\s*$")


def expand(path: Path, include_dir: Path, seen: set, out: list) -> None:
    for line in path.read_text().splitlines():
        match = INCLUDE.match(line)
        if match is None:
            if line.strip() != "#pragma once":
                out.append(line)
            continue
        header = include_dir / match.group(1)
        if header not in seen:
            seen.add(header)
            expand(header, include_dir, seen, out)


def main() -> None:
    source, include_dir, output = (Path(arg) for arg in sys.argv[1:])
    out: list = []
    expand(source, include_dir, set(), out)
    output.write_text("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
