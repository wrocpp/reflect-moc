"""Edits that apply to every source file, not only to classes moc saw."""

from __future__ import annotations

import re
from bisect import bisect_right
from itertools import accumulate

from . import syntax
from .report import Report
from .text import Source, blank, matching

# `emit` / `Q_EMIT` before a call; blanked text, so never inside a literal or comment.
_EMIT = re.compile(r"(?<![\w.>:])(emit|Q_EMIT)\s+(?=[A-Za-z_:(*])")
_FOREVER = re.compile(r"(?<![\w.>:])forever(?!\w)")
_FOREACH = re.compile(r"(?<![\w.>:])foreach(?=\s*\()")
_MOC_INCLUDE = re.compile(r'^\s*#\s*include\s*[<"](moc_[^">]+\.cpp|[^">]+\.moc)[">]')
_INCLUDE = re.compile(r"^\s*#\s*include\b")
_GUARD = re.compile(r"^\s*(#\s*pragma\s+once|#\s*define\s+\w+\s*$)")


def sweep(src: Source, rel: str, report: Report, style: str = syntax.ANNOTATIONS) -> None:
    """moc includes are always dropped. emit, forever and foreach only need rewriting for
    QT_NO_KEYWORDS, which qtlike does not require."""
    for i, blanked in enumerate(src.blank_lines):
        if i in src.deleted:
            continue
        if _MOC_INCLUDE.match(src.lines[i]):
            src.delete(i)
            report.auto(rel, i + 1, "moc include")
            continue
        if style == syntax.QTLIKE:
            continue
        text = src.lines[i].rstrip("\r\n")
        edits = []
        for rx, replacement, construct in (
            (_EMIT, "", "emit keyword"),
            (_FOREVER, syntax.FOREVER, "forever keyword"),
            (_FOREACH, syntax.FOREACH, "foreach keyword"),
        ):
            for m in rx.finditer(blanked):
                edits.append((m.start(), m.end(), replacement, construct))
        if not edits:
            continue
        for start, end, replacement, construct in sorted(edits, reverse=True):
            text = text[:start] + replacement + text[end:]
            report.auto(rel, i + 1, construct)
        src.replace(i, text)


def rewrite_base_initializers(src: Source, rel: str, bases: list[tuple[str, str]], report: Report) -> None:
    """`Name::Name(...) : QObject(parent)` -> `... : rqt::Object<QObject>(parent)`.

    A mem-initializer must name a direct base, and the old base is now one
    level further up. Constructors are found by name; an initializer list is
    the text between the `:` after the parameter list and the next `{`.
    """
    blanked = "".join(src.blank_lines)
    edits: list[tuple[int, int, str]] = []
    for cls, base in bases:
        ctor = re.compile(rf"(?<![\w~:.>])(?:{re.escape(cls)}\s*::\s*)?{re.escape(cls)}\s*\(")
        for m in ctor.finditer(blanked):
            if re.search(r"\bnew\s*$", blanked[max(0, m.start() - 8) : m.start()]):
                continue
            close = matching(blanked, m.end() - 1)
            if close < 0:
                continue
            after = re.compile(r"\s*(?:noexcept\b\s*)?:(?!:)").match(blanked, close + 1)
            if not after:
                continue
            body = blanked.find("{", after.end())
            semi = blanked.find(";", after.end())
            if body < 0 or (0 <= semi < body):
                continue
            item = re.compile(rf"(?<![\w:]){re.escape(base)}(?=\s*[({{])")
            for init in item.finditer(blanked, after.end(), body):
                if _inside_parens(blanked, after.end(), init.start()):
                    continue
                edits.append((init.start(), init.end(), syntax.object_base(base)))
    if not edits:
        return
    starts = list(accumulate(len(line) for line in src.lines))
    for start, end, replacement in sorted(edits, reverse=True):
        i = bisect_right(starts, start)
        offset = start - (starts[i - 1] if i else 0)
        line = src.lines[i]
        src.lines[i] = line[:offset] + replacement + line[offset + end - start :]
        src.blank_lines[i] = blank(src.lines[i])
        report.auto(rel, blanked.count("\n", 0, start) + 1, "base class initializer")


def _inside_parens(blanked: str, start: int, pos: int) -> bool:
    depth = 0
    for ch in blanked[start:pos]:
        if ch in "({":
            depth += 1
        elif ch in ")}":
            depth -= 1
    return depth > 0


def add_includes(src: Source, before_line: int, includes: list[str]) -> None:
    """Insert includes after the last #include above before_line (or after the guard)."""
    last_include = -1
    guard = -1
    for i in range(0, max(before_line, 0)):
        if _INCLUDE.match(src.lines[i]) and i not in src.deleted:
            last_include = i
        elif guard < 0 and _GUARD.match(src.lines[i]):
            guard = i
    at = last_include + 1 if last_include >= 0 else guard + 1 if guard >= 0 else 0
    for inc in includes:
        src.insert_before(at, inc)


def add_includes_at_end_of_includes(src: Source, includes: list[str]) -> None:
    """For a .cpp: after its last #include."""
    add_includes(src, len(src.lines), includes)
