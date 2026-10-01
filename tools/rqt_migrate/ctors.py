"""Constructors: where reflect-moc needs `bind();`.

A migrated class is bound to its meta-object by a constructor body that calls
bind() (README, "Binding the class to the instance"); the most derived
constructor body wins, so every constructor of every migrated class gets the
call. A constructor is found by its name, and what follows its parameter list
is read just far enough to tell a declaration from a definition.
"""

from __future__ import annotations

import re
from bisect import bisect_right
from itertools import accumulate

from . import syntax
from .report import Report
from .text import Source, blank, indent_of, matching

DECLARATION = "declaration"
DEFAULTED = "defaulted"
DELETED = "deleted"
BODY = "body"

_SKIP_SPEC = re.compile(r"\s*(?:noexcept\b(?:\s*\()?|\[\[[^\]]*\]\])")
_INIT_NAME = re.compile(r"\s*[\w:]+(?:\s*<[^;{}()]*>)?\s*")


def is_copy_or_move(params: str, cls: str) -> bool:
    """`const C &`, `C &&` and `C &` parameters: the copy and move constructors."""
    return re.match(rf"\s*(?:const\s+)?{re.escape(cls)}\s*&", params) is not None


def tail(blanked: str, close: int) -> tuple[str, int]:
    """What follows a constructor's `)` at `close`: (kind, position).

    The position is the `{` of a body, the `=` of `= default` / `= delete`, or
    the `;` of a declaration.
    """
    pos = close + 1
    while True:
        m = _SKIP_SPEC.match(blanked, pos)
        if not m:
            break
        pos = m.end()
        if blanked[pos - 1] == "(":
            pos = matching(blanked, pos - 1) + 1
    while pos < len(blanked) and blanked[pos].isspace():
        pos += 1
    if pos >= len(blanked):
        return DECLARATION, pos
    if blanked[pos] == ";":
        return DECLARATION, pos
    if blanked[pos] == "=":
        word = re.compile(r"=\s*(default|delete)\b").match(blanked, pos)
        return (DEFAULTED if word and word.group(1) == "default" else DELETED), pos
    if blanked[pos] == ":" and blanked[pos : pos + 2] != "::":
        return _after_initializers(blanked, pos + 1)
    if blanked[pos] == "{":
        return BODY, pos
    return DECLARATION, pos


def _after_initializers(blanked: str, pos: int) -> tuple[str, int]:
    """The `{` after a mem-initializer list that starts at `pos` (just after the `:`)."""
    while True:
        m = _INIT_NAME.match(blanked, pos)
        if not m or m.end() >= len(blanked) or blanked[m.end()] not in "({":
            return DECLARATION, pos
        end = matching(blanked, m.end())
        if end < 0:
            return DECLARATION, pos
        pos = end + 1
        while pos < len(blanked) and blanked[pos].isspace():
            pos += 1
        if blanked.startswith("...", pos):
            pos += 3
            while pos < len(blanked) and blanked[pos].isspace():
                pos += 1
        if pos < len(blanked) and blanked[pos] == ",":
            pos += 1
            continue
        if pos < len(blanked) and blanked[pos] == "{":
            return BODY, pos
        return DECLARATION, pos


def already_binds(blanked: str, body_open: int) -> bool:
    end = matching(blanked, body_open)
    return end > 0 and re.search(r"(?<![\w.>])bind\s*\(", blanked[body_open:end]) is not None


class Edits:
    """Edits to one file addressed by offset in the file's blanked text.

    Apply them back to front, so an earlier offset is never moved by a later edit.
    """

    def __init__(self, src: Source):
        self.src = src
        self.blanked = "".join(src.blank_lines)
        self.starts = list(accumulate(len(line) for line in src.lines))
        self.found: list[tuple[int, str, int]] = []  # (position, how, line)

    def line_of(self, pos: int) -> int:
        return bisect_right(self.starts, pos)

    def bind_after_brace(self, brace: int) -> None:
        self.found.append((brace, "brace", self.line_of(brace)))

    def replace_default(self, equals: int) -> None:
        self.found.append((equals, "default", self.line_of(equals)))

    def apply(self) -> list[int]:
        """Apply every edit; the 1-based line of each."""
        lines = []
        for pos, how, line in sorted(self.found, reverse=True):
            lines.append(line + 1)
            if how == "brace":
                self._after_brace(pos, line)
            else:
                self._default(pos, line)
        return sorted(lines)

    def _offset(self, pos: int, line: int) -> int:
        return pos - (self.starts[line - 1] if line else 0)

    def _after_brace(self, pos: int, line: int) -> None:
        text = self.src.lines[line].rstrip("\r\n")
        col = self._offset(pos, line)
        rest = text[col + 1 :]
        if not rest.strip():
            self.src.insert_before(line + 1, indent_of(text) + "    " + syntax.BIND)
            return
        self.src.replace(line, text[: col + 1] + " " + syntax.BIND + " " + rest.lstrip())

    def _default(self, pos: int, line: int) -> None:
        text = self.src.lines[line].rstrip("\r\n")
        col = self._offset(pos, line)
        m = re.compile(r"=\s*default\s*;").match(text, col)
        if not m:
            return  # `= default` split across lines: left for the report
        self.src.replace(line, text[:col] + "{ " + syntax.BIND + " }" + text[m.end() :])


def bind_out_of_line(src: Source, rel: str, classes: list[str], report: Report) -> dict[str, int]:
    """`C::C(...) : ... { ... }` definitions: add bind(); returns how many each class had."""
    edits = Edits(src)
    hits: dict[str, int] = {}
    for cls in classes:
        ctor = re.compile(rf"(?<![\w~:.>]){re.escape(cls)}\s*::\s*{re.escape(cls)}\s*\(")
        for m in ctor.finditer(edits.blanked):
            close = matching(edits.blanked, m.end() - 1)
            if close < 0 or is_copy_or_move(edits.blanked[m.end() : close], cls):
                continue
            kind, pos = tail(edits.blanked, close)
            if kind != BODY:
                continue
            hits[cls] = hits.get(cls, 0) + 1
            if already_binds(edits.blanked, pos):
                continue
            edits.bind_after_brace(pos)
    for line in edits.apply():
        report.auto(rel, line, "constructor", "bind() added")
    return hits
