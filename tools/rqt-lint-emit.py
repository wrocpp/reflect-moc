#!/usr/bin/env python3
"""Flags `emit other->sig(v)` where `sig` is an rqt::static_signal (Python 3, stdlib only).

A static signal is one object for the whole class, so `emit other->sig(v)` discards `other`, compiles,
and fires on `this`: Qt's own signals fire on `other`. Write `sig(v).from(other);` instead.

    tools/rqt-lint-emit.py src include          # files and directories; exit 1 if anything is flagged

The names of the static signals come from every scanned file (`rqt::static_signal<...> name`), so a
class declared in one header is checked where it is used in another. A real Qt signal that happens to
share a static signal's name in another class is flagged too: rename it or write the explicit form.
"""

from __future__ import annotations

import argparse
import os
import re
import sys
from typing import Iterable, Iterator, NamedTuple

SOURCE_SUFFIXES = (".cpp", ".cc", ".cxx", ".hpp", ".h", ".hh", ".ipp")

_COMMENT_OR_STRING = re.compile(
    r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)
_STATIC_SIGNAL = re.compile(r"\bstatic_signal\s*<[^;{}]*>\s*(\w+)")


class Finding(NamedTuple):
    path: str
    line: int
    text: str
    name: str


def blank(source: str) -> str:
    """Comments and literals become spaces, newlines stay: columns and line numbers are kept."""
    return _COMMENT_OR_STRING.sub(lambda m: re.sub(r"[^\n]", " ", m.group()), source)


def static_signal_names(source: str) -> set[str]:
    return set(_STATIC_SIGNAL.findall(blank(source)))


def _emit_pattern(names: Iterable[str]) -> re.Pattern[str] | None:
    names = sorted(names)
    if not names:
        return None
    alternatives = "|".join(re.escape(n) for n in names)
    # `emit <expression> -> or . <static signal> (` where the expression is anything but `this`
    return re.compile(rf"\b(?:emit|Q_EMIT)\s+(?!this\s*(?:->|\.)|::)[^;{{}}]*?(?:->|\.)\s*({alternatives})\s*\(")


def lint_source(path: str, source: str, names: Iterable[str]) -> list[Finding]:
    pattern = _emit_pattern(names)
    if pattern is None:
        return []
    clean = blank(source)
    original = source.splitlines()
    found = []
    for match in pattern.finditer(clean):
        line = clean.count("\n", 0, match.start()) + 1
        found.append(Finding(path, line, original[line - 1].strip(), match.group(1)))
    return found


def source_files(paths: Iterable[str]) -> Iterator[str]:
    for path in paths:
        if os.path.isfile(path):
            yield path
            continue
        for root, _, files in os.walk(path):
            for name in sorted(files):
                if name.endswith(SOURCE_SUFFIXES):
                    yield os.path.join(root, name)


def read(path: str) -> str:
    with open(path, encoding="utf-8", errors="replace") as handle:
        return handle.read()


def lint_paths(paths: Iterable[str]) -> list[Finding]:
    files = {path: read(path) for path in source_files(paths)}
    names: set[str] = set()
    for source in files.values():
        names |= static_signal_names(source)
    found: list[Finding] = []
    for path, source in files.items():
        found += lint_source(path, source, names)
    return found


def format_finding(f: Finding) -> str:
    return (f"{f.path}:{f.line}: emit through another object fires on `this`: `{f.name}` is an "
            f"rqt::static_signal; write `{f.name}(...).from(object);`\n    {f.text}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("paths", nargs="+", help="source files or directories")
    args = parser.parse_args(argv)
    found = lint_paths(args.paths)
    for f in found:
        print(format_finding(f))
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main())
