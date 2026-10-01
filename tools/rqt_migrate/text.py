"""Line-level editing of C++ source, with comments and literals masked.

rqt-migrate does not parse C++. moc's JSON says which line a construct is on;
these helpers find the exact span on that line with regexes that run over a
blanked copy of the text, where comments and the contents of string and
character literals are spaces, so a brace or a keyword inside them never
matches. The blanked copy has the same length and line breaks as the source,
so a position found in it is the same position in the source.
"""

from __future__ import annotations

import re

_RAW_STRING = re.compile(r'R"([^(\s]{0,16})\(')


def blank(text: str) -> str:
    """text with comments and literal contents replaced by spaces (newlines kept)."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c == "/" and nxt == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(_spaces_keeping_newlines(text[i:j]))
            i = j
        elif c == "R" and nxt == '"' and (i == 0 or not (text[i - 1].isalnum() or text[i - 1] == "_")):
            m = _RAW_STRING.match(text, i)
            if not m:
                out.append(c)
                i += 1
                continue
            end = text.find(")" + m.group(1) + '"', m.end())
            end = n if end < 0 else end + len(m.group(1)) + 2
            out.append('R"' + _spaces_keeping_newlines(text[i + 2 : end - 1]) + '"')
            i = end
        elif c in "\"'":
            if c == "'" and i > 0 and text[i - 1].isalnum() and _is_digit_separator(text, i):
                out.append(c)
                i += 1
                continue
            j = i + 1
            while j < n and text[j] != c and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            j = min(j, n - 1)
            out.append(c + " " * (j - i - 1) + text[j])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def _spaces_keeping_newlines(s: str) -> str:
    return "".join("\n" if ch == "\n" else " " for ch in s)


def _is_digit_separator(text: str, i: int) -> bool:
    """A ' between digits (1'000) is a C++14 digit separator, not a char literal."""
    j = i - 1
    while j >= 0 and (text[j].isalnum() or text[j] in "'_"):
        j -= 1
    return text[j + 1].isdigit()


def matching(blanked: str, open_pos: int) -> int:
    """The position of the bracket closing the one at open_pos, or -1."""
    pairs = {"(": ")", "{": "}", "[": "]"}
    opener = blanked[open_pos]
    closer = pairs[opener]
    depth = 0
    for i in range(open_pos, len(blanked)):
        ch = blanked[i]
        if ch == opener:
            depth += 1
        elif ch == closer:
            depth -= 1
            if depth == 0:
                return i
    return -1


def split_top_level(blanked: str, start: int, end: int) -> list[tuple[int, int]]:
    """Spans of the comma-separated items of blanked[start:end] at bracket depth 0."""
    spans = []
    depth = 0
    item_start = start
    for i in range(start, end):
        ch = blanked[i]
        if ch in "([{<":
            depth += 1
        elif ch in ")]}>":
            depth -= 1
        elif ch == "," and depth == 0:
            spans.append((item_start, i))
            item_start = i + 1
    spans.append((item_start, end))
    return spans


def has_top_level(blanked: str, start: int, end: int, char: str) -> int:
    """The first position of char in blanked[start:end] at bracket depth 0, or -1."""
    depth = 0
    for i in range(start, end):
        ch = blanked[i]
        if ch in "([{<":
            depth += 1
        elif ch in ")]}>":
            depth -= 1
        elif ch == char and depth == 0:
            return i
    return -1


def indent_of(line: str) -> str:
    return line[: len(line) - len(line.lstrip(" \t"))]


class Source:
    """One file as lines. Edits address the ORIGINAL line indices: a replaced
    line keeps its index, deleted lines and inserted lines are applied by
    text(), so edits made in any order cannot shift each other."""

    def __init__(self, path: str, text: str):
        self.path = path
        self.original = text
        self.lines = text.splitlines(keepends=True)
        self.deleted: set[int] = set()
        self.inserted_before: dict[int, list[str]] = {}
        self._refresh_blank()

    def _refresh_blank(self) -> None:
        self.blank_lines = blank("".join(self.lines)).splitlines(keepends=True)
        while len(self.blank_lines) < len(self.lines):
            self.blank_lines.append("")

    def eol(self, i: int) -> str:
        line = self.lines[i]
        return line[len(line.rstrip("\r\n")) :] or "\n"

    def replace(self, i: int, new_line: str) -> None:
        """Replace line i; new_line has no line ending (the old one is kept)."""
        self.lines[i] = new_line + self.eol(i)
        self.blank_lines[i] = blank(self.lines[i])

    def replace_span(self, first: int, last: int, new_text: str) -> None:
        """Replace lines first..last with new_text (no trailing line ending)."""
        eol = self.eol(last)
        self.lines[first] = new_text + eol
        self.blank_lines[first] = blank(self.lines[first])
        for i in range(first + 1, last + 1):
            self.deleted.add(i)

    def delete(self, i: int) -> None:
        self.deleted.add(i)

    def insert_before(self, i: int, new_line: str) -> None:
        self.inserted_before.setdefault(i, []).append(new_line)

    def joined(self, first: int, last: int) -> tuple[str, str]:
        """(source, blanked) of lines first..last joined, line endings included."""
        return "".join(self.lines[first : last + 1]), "".join(self.blank_lines[first : last + 1])

    def text(self) -> str:
        out = []
        for i, line in enumerate(self.lines):
            for extra in self.inserted_before.get(i, []):
                out.append(extra + "\n")
            if i not in self.deleted:
                out.append(line)
        for extra in self.inserted_before.get(len(self.lines), []):
            out.append(extra + "\n")
        return "".join(out)

    def find_line_end(self, first: int, char: str) -> int:
        """The first line at or after `first` whose blanked text contains char at depth 0 of that scan."""
        depth = 0
        for i in range(first, len(self.lines)):
            for ch in self.blank_lines[i]:
                if ch in "([{":
                    depth += 1
                elif ch in ")]}":
                    depth -= 1
                elif ch == char and depth == 0:
                    return i
        return -1

    def balanced_end_line(self, line: int, open_col: int) -> tuple[int, int]:
        """(line, column) of the bracket closing the one at (line, open_col)."""
        rest = "".join(self.blank_lines[line:])
        close = matching(rest, open_col)
        if close < 0:
            return -1, -1
        before = rest[:close]
        return line + before.count("\n"), close - (before.rfind("\n") + 1)
