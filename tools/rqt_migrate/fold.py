"""Folding a signals-only Q_OBJECT base into the one class template that derives from it.

moc cannot read a class template, so a design that wants typed signals in one splits the class in two: a
non-template base that carries Q_OBJECT and declares every signal with its key boxed in a QVariant, and the
template that derives from it and boxes at each emit. reflect-moc declares the signals in the template itself,
typed. This module finds the split from moc's JSON for the base, moves the signals into the template and
unboxes the keys. What it cannot rewrite with certainty it reports with file and line and leaves as written.
"""

from __future__ import annotations

import re
from bisect import bisect_right
from dataclasses import dataclass
from itertools import accumulate

from . import syntax
from .classes import ClassRewriter, _ACCESS_LABEL, _SIGNALS_LABEL, _dedupe
from .report import Report
from .sweep import add_includes
from .text import Source, blank, indent_of, matching

_ACCESS_WORDS = {"public", "protected", "private", "virtual"}
_HEAD = re.compile(r"\b(?:class|struct)\s+(\w+)\s*(?:final\s*)?:\s*([^{;]*)\{")
_TEMPLATE_PREFIX = re.compile(r"template\s*<[^;{}]*>\s*$")
_VARIANT_PARAM = re.compile(r"^\s*const\s+QVariant\s*&\s*(\w*)\s*$")
_BASE_MEMBER = re.compile(r"^\s*(Q_OBJECT\b|using\s+[\w:]+\s*;)")
_FROM_VALUE = re.compile(r"\s*QVariant\s*::\s*fromValue\s*\(")
_LAMBDA = re.compile(r"\s*\[[^\]]*\]\s*\(")
_LAMBDA_TAIL = re.compile(r"\s*(?:mutable\s*)?(?:noexcept\s*)?\{")
_CLASS_BODY = re.compile(r"\b(?:class|struct)\s+\w+[^;{}()]*\{")
_LOOKBEHIND_USE = r"(?<![\w.>:])"
_KEY_ALIAS = re.compile(r"\busing\s+" + syntax.FOLD_KEY_ALIAS + r"\s*=")
_KEY_MEMBER = re.compile(r"^\s*(?:using\s+" + syntax.FOLD_KEY_ALIAS + r"\s*=|typedef\b[^;]*\b"
                         + syntax.FOLD_KEY_ALIAS + r"\s*;)")


@dataclass
class Fold:
    base: str
    base_rel: str
    qt_base: str  # the base's own base, which the template derives from now
    template: str
    template_rel: str
    template_line: int  # 0-based line of the template's class head
    base_cls: dict  # moc's JSON for the base


@dataclass
class _Head:
    rel: str
    name: str
    line: int
    template: bool
    bases: list[str]


@dataclass
class _Signal:
    name: str
    arguments: list[str]  # the parameter names, "" for an unnamed one


def _split_args(text: str, start: int, end: int) -> list[tuple[int, int]]:
    """Comma-separated items of text[start:end] at depth 0 of (), [] and {} (angle brackets are not counted,
    so a template argument list with a comma splits and the caller's pattern then refuses it)."""
    spans, depth, begin = [], 0, start
    for i in range(start, end):
        ch = text[i]
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        elif ch == "," and depth == 0:
            spans.append((begin, i))
            begin = i + 1
    spans.append((begin, end))
    return spans


def _base_names(text: str) -> list[str]:
    names = []
    for a, b in _split_args(text, 0, len(text)):
        words = [w for w in re.findall(r"\w+", re.sub(r"<[^<>]*>", "", text[a:b])) if w not in _ACCESS_WORDS]
        if words:
            names.append(words[-1])
    return names


def _heads(sources: dict[str, Source]) -> list[_Head]:
    out = []
    for rel, src in sources.items():
        blanked = "".join(src.blank_lines)
        for m in _HEAD.finditer(blanked):
            prefix = blanked[max(0, m.start() - 400) : m.start()]
            out.append(_Head(rel, m.group(1), blanked.count("\n", 0, m.start()),
                             bool(_TEMPLATE_PREFIX.search(prefix)), _base_names(m.group(2))))
    return out


def _signals_only(cls: dict) -> bool:
    """moc's JSON: an object with signals, one base, and nothing else a slot or a property would need."""
    if not (cls.get("object") and cls.get("signals") and len(cls.get("superClasses", [])) == 1):
        return False
    return not any(cls.get(k) for k in ("slots", "properties", "methods", "constructors", "enums", "classInfos"))


def _fold_for(rel: str, cls: dict, heads: list[_Head], report: Report) -> Fold | None:
    name = cls["className"]
    users = [h for h in heads if name in h.bases]
    if not any(h.template for h in users):
        return None
    if len(users) != 1 or len(users[0].bases) != 1:
        report.manual(rel, cls.get("lineNumber", 0), "signals-only base",
                      f"`{name}` is not folded: it needs exactly one derived class, a class template with "
                      f"no other base, and {len(users)} classes derive from it")
        return None
    user = users[0]
    return Fold(name, rel, cls["superClasses"][0]["name"], user.name, user.rel, user.line, cls)


def plan(per_file: list[tuple[str, list[dict]]], sources: dict[str, Source],
         report: Report) -> tuple[list[Fold], list[tuple[str, list[dict]]]]:
    """The folds to apply, and per_file without the folded bases (they are not migrated as classes)."""
    heads = _heads(sources)
    folds: list[Fold] = []
    kept: list[tuple[str, list[dict]]] = []
    for rel, classes in per_file:
        rest = []
        for cls in classes:
            fold = _fold_for(rel, cls, heads, report) if _signals_only(cls) else None
            if fold:
                folds.append(fold)
            else:
                rest.append(cls)
        if rest:
            kept.append((rel, rest))
    return folds, kept


def apply(folds: list[Fold], sources: dict[str, Source], report: Report, compat: bool = False) -> None:
    for fold in folds:
        _Folder(fold, sources, report, compat).run()


class _Edits:
    """Offset edits to one file, taken against a snapshot so they cannot shift each other."""

    def __init__(self, src: Source):
        self.src = src
        self.blanked = "".join(src.blank_lines)
        self.real = "".join(src.lines)
        self.starts = [0] + list(accumulate(len(line) for line in src.lines))
        self.items: list[tuple[int, int, str]] = []

    def line_of(self, pos: int) -> int:
        return bisect_right(self.starts, pos) - 1

    def single_line(self, start: int, end: int) -> bool:
        return self.line_of(start) == self.line_of(max(start, end - 1))

    def add(self, start: int, end: int, text: str) -> None:
        if (start, end, text) not in self.items:
            self.items.append((start, end, text))

    def apply(self) -> None:
        for start, end, text in sorted(self.items, reverse=True):
            i = self.line_of(start)
            line = self.src.lines[i]
            offset = start - self.starts[i]
            updated = line[:offset] + text + line[offset + end - start :]
            self.src.lines[i] = updated
            self.src.blank_lines[i] = blank(updated)
            if not text and not updated.strip():
                self.src.delete(i)


class _Folder:
    def __init__(self, fold: Fold, sources: dict[str, Source], report: Report, compat: bool):
        self.fold = fold
        self.sources = sources
        self.report = report
        self.base_cls = fold.base_cls
        self.compat = compat
        self.edits: dict[str, _Edits] = {}
        self.covered: list[tuple[int, int]] = []  # template spans rewritten as QVariant::fromValue arguments
        self.aliases: dict[str, list[str]] = {}
        self.names: list[str] = []
        self.base_rw: ClassRewriter | None = None

    # --- the base ---------------------------------------------------------------

    def _base_signals(self) -> list[_Signal] | None:
        fold = self.fold
        src = self.sources[fold.base_rel]
        rw = ClassRewriter(src, fold.base_rel, self.base_cls, self.report, False)
        if not rw.locate():
            return None
        self.base_rw = rw
        found = _dedupe(self.base_cls.get("signals", []))
        if len({s["name"] for s in found}) != len(found):
            self.report.manual(fold.base_rel, self.base_cls.get("lineNumber", 0), "signals-only base",
                               f"`{fold.base}` has an overloaded signal, which cannot be a data member: not folded")
            return None
        covered: set[int] = set()
        signals = []
        for sig in found:
            parsed = self._parse_signal(src, sig, covered)
            if parsed is None:
                return None
            signals.append(parsed)
        return signals if self._only_signals(rw, src, covered) else None

    def _parse_signal(self, src: Source, sig: dict, covered: set[int]) -> _Signal | None:
        name, line = sig["name"], sig["lineNumber"] - 1
        rx = re.compile(rf"\bvoid\s+{re.escape(name)}\s*\(")
        for i in (line, line + 1):
            rest = "".join(src.blank_lines[i:])
            m = rx.search(rest) if 0 <= i < len(src.lines) else None
            if m and not rest[: m.start()].strip():
                break
        else:
            return self._refuse(sig, f"could not find the declaration of `{name}`")
        close = matching(rest, m.end() - 1)
        semi = re.compile(r"\s*;").match(rest, close + 1) if close > 0 else None
        if not semi:
            return self._refuse(sig, f"`{name}` is not a plain `void {name}(...);` declaration")
        arguments = self._parameter_names(rest, m.end(), close)
        if arguments is None:
            return self._refuse(sig, f"`{name}` has a parameter that is not `const QVariant&`")
        covered.update(range(i, i + rest[: semi.end()].count("\n") + 1))
        return _Signal(name, arguments)

    @staticmethod
    def _parameter_names(rest: str, start: int, close: int) -> list[str] | None:
        if not rest[start:close].strip():
            return []
        names = []
        for a, b in _split_args(rest, start, close):
            m = _VARIANT_PARAM.match(rest[a:b])
            if not m:
                return None
            names.append(m.group(1))
        return names

    def _refuse(self, sig: dict, why: str) -> None:
        self.report.manual(self.fold.base_rel, sig.get("lineNumber", 0), "signals-only base",
                           f"`{self.fold.base}` is not folded: {why}")
        return None

    def _only_signals(self, rw: ClassRewriter, src: Source, covered: set[int]) -> bool:
        for i in rw.members:
            text = src.blank_lines[i]
            if i == rw.close_line or i in covered or _ACCESS_LABEL.match(text) or _SIGNALS_LABEL.match(text) or _BASE_MEMBER.match(text):
                continue
            self.report.manual(self.fold.base_rel, i + 1, "signals-only base",
                               f"`{self.fold.base}` is not folded: it declares more than signals here")
            return False
        return True

    # --- the template -----------------------------------------------------------

    def _template(self) -> ClassRewriter | None:
        fold = self.fold
        cls = {"className": fold.template, "lineNumber": fold.template_line + 1}
        rw = ClassRewriter(self.sources[fold.template_rel], fold.template_rel, cls, self.report, False)
        if not rw.locate():
            return None
        problem = self._template_problem(rw)
        if problem:
            self.report.manual(fold.template_rel, rw.head_line + 1, "signals-only base",
                               f"`{fold.base}` is not folded: {problem}")
            return None
        return rw

    def _template_problem(self, rw: ClassRewriter) -> str | None:
        src = self.sources[self.fold.template_rel]
        if not any(_KEY_MEMBER.match(src.blank_lines[i]) for i in rw.members):
            return f"`{self.fold.template}` has no `using {syntax.FOLD_KEY_ALIAS} = ...` alias to type the signals with"
        if not src.blank_lines[rw.open_line].rstrip().endswith("{"):
            return "the template's opening brace is not the last thing on its line"
        if not src.blank_lines[rw.close_line].lstrip().startswith("}"):
            return "the template's closing brace does not start its line"
        if not self._base_span(rw):
            return f"could not find `{self.fold.base}` in the template's base list"
        return None

    def _base_span(self, rw: ClassRewriter) -> tuple[int, int] | None:
        src = self.sources[self.fold.template_rel]
        rx = re.compile(r"(:\s*(?:(?:public|protected|private|virtual)\s+)*)" + re.escape(self.fold.base) + r"\b")
        for i in range(rw.head_line, rw.open_line + 1):
            m = rx.search(src.blank_lines[i])
            if m:
                start = sum(len(line) for line in src.lines[:i]) + m.end(1)
                return start, start + len(self.fold.base)
        return None

    def _rewrite_template(self, rw: ClassRewriter, signals: list[_Signal]) -> None:
        fold, src = self.fold, self.sources[self.fold.template_rel]
        e = self.edits[fold.template_rel]
        start, end = self._base_span(rw)
        e.add(start, end, fold.qt_base)
        pad = indent_of(src.lines[rw.head_line]) + "  "
        src.insert_before(rw.open_line + 1, pad + syntax.fold_object_macro(self.compat))
        body = (e.starts[rw.head_line], e.starts[rw.close_line + 1])
        self._base_references(e, (e.starts[rw.open_line + 1], body[1]))
        self._emit_sites(fold.template_rel, e, body)
        self._leftover_variants(fold.template_rel, e, body)
        self._signal_block(rw, signals)
        self.report.auto(fold.template_rel, rw.head_line + 1, "signals-only base",
                         f"`{fold.base}` folded into `{fold.template}`: {len(signals)} signals are now typed members")

    def _base_references(self, e: _Edits, body: tuple[int, int]) -> None:
        """`using Base::Base;` and a `Base(...)` mem-initializer name the Qt base now; body is past the head."""
        base = re.escape(self.fold.base)
        qt = self.fold.qt_base
        last = qt.split("::")[-1]
        using = re.compile(rf"\busing\s+{base}\s*::\s*{base}\s*;")
        mem_init = re.compile(rf"(?<![\w:~.>]){base}(?=\s*[({{])")
        for m in using.finditer(e.blanked, *body):
            e.add(m.start(), m.end(), f"using {qt}::{last};")
        for m in mem_init.finditer(e.blanked, *body):
            e.add(m.start(), m.end(), qt)

    def _emit_sites(self, rel: str, e: _Edits, body: tuple[int, int]) -> None:
        names = "|".join(sorted(self.names, key=len, reverse=True))
        rx = re.compile(rf"(?<![\w.>:])(?:(?:emit|Q_EMIT)\s+)?(?:{names})\s*\(")
        for m in rx.finditer(e.blanked, *body):
            close = matching(e.blanked, m.end() - 1)
            if close < 0:
                continue
            for a, b in _split_args(e.blanked, m.end(), close):
                self._unbox_argument(rel, e, a, b)

    def _unbox_argument(self, rel: str, e: _Edits, a: int, b: int) -> None:
        seg = e.blanked[a:b]
        if not seg.strip():
            return
        start, end = a + len(seg) - len(seg.lstrip()), b - (len(seg) - len(seg.rstrip()))
        m = _FROM_VALUE.match(seg)
        if m:
            opener = a + m.end() - 1
            closer = matching(e.blanked, opener)
            if closer > 0 and not e.blanked[closer + 1 : b].strip() and e.single_line(start, end):
                e.add(start, end, e.real[opener + 1 : closer].strip())
                self.covered.append((start, end))
                self.report.auto(rel, e.line_of(start) + 1, "QVariant boxing", "unboxed at the emit")
                return
        self.covered.append((start, end))
        self.report.manual(rel, e.line_of(start) + 1, "QVariant boxing",
                           "an emit of a folded signal passes something other than `QVariant::fromValue(x)`; "
                           "left as written")

    def _leftover_variants(self, rel: str, e: _Edits, body: tuple[int, int]) -> None:
        for m in re.finditer(r"\bQVariant\b", e.blanked[body[0] : body[1]]):
            pos = body[0] + m.start()
            if not any(a <= pos < b for a, b in self.covered):
                self.report.manual(rel, e.line_of(pos) + 1, "QVariant boxing",
                                   "a QVariant left in the folded template; check it against the typed key")

    @staticmethod
    def _member_indent(rw: ClassRewriter, src: Source) -> str:
        for i in rw.members:
            if src.blank_lines[i].strip() and not _ACCESS_LABEL.match(src.blank_lines[i]):
                return indent_of(src.lines[i])
        return indent_of(src.lines[rw.head_line]) + "  "

    @staticmethod
    def _signal_anchor(rw: ClassRewriter, src: Source) -> tuple[int, bool]:
        """Where the block goes: before the last non-public section, else at the closing brace."""
        guarded = [i for i in rw.members
                   if (m := _ACCESS_LABEL.match(src.blank_lines[i])) and m.group(1) != "public"]
        return (guarded[-1], True) if guarded else (rw.close_line, False)

    def _signal_block(self, rw: ClassRewriter, signals: list[_Signal]) -> None:
        src = self.sources[self.fold.template_rel]
        indent = self._member_indent(rw, src)
        lines = [indent + (syntax.signal_names(s.arguments) + " " if any(s.arguments) else "")
                 + syntax.signal_member([syntax.key_parameter()] * len(s.arguments), s.name) for s in signals]
        at, before_section = self._signal_anchor(rw, src)
        if rw._access_at(at) != "public":
            lines.insert(0, indent_of(src.lines[rw.head_line]) + " public:")
        for line in (lines + [""]) if before_section else ([""] + lines):
            src.insert_before(at, line)

    # --- connect sites ------------------------------------------------------------

    def _connect_sites(self, rel: str) -> None:
        e = self.edits[rel]
        fold = self.fold
        aliases = re.findall(rf"\busing\s+(\w+)\s*=\s*(?:\w+\s*::\s*)*{re.escape(fold.template)}\s*<", e.blanked)
        owners = "|".join(map(re.escape, [fold.base, fold.template, *aliases]))
        names = "|".join(self.names)
        site = re.compile(rf"&\s*(?:\w+\s*::\s*)*({owners})(?:\s*<[^;{{}}()]*?>)?\s*::\s*({names})\b")
        for m in site.finditer(e.blanked):
            self._site(rel, e, m, aliases)
        for m in re.finditer(rf"\b(?:SIGNAL|SLOT)\s*\(\s*({names})\b", e.blanked):
            self.report.manual(rel, e.line_of(m.start()) + 1, "string connect",
                               f"`{m.group(1)}` is connected by SIGNAL(); its QVariant argument is the typed key now")

    def _site(self, rel: str, e: _Edits, m: re.Match, aliases: list[str]) -> None:
        line = e.line_of(m.start()) + 1
        changes: list[tuple[int, int, str]] = []
        if m.group(1) == self.fold.base:
            owner = self._owner_name(rel, e, m.start(), aliases)
            if owner is None:
                self.report.manual(rel, line, "connect", f"`&{self.fold.base}::{m.group(2)}` is not folded: "
                                   f"say which `{self.fold.template}` it connects to")
                return
            changes.append((*m.span(1), owner))
        arg = self._lambda_argument(e, m.end())
        if arg is None:
            self.report.manual(rel, line, "connect", f"`{m.group(2)}` is not connected to a lambda; "
                               "a function taking `const QVariant&` must take the key type")
            return
        edits = self._lambda_edits(rel, e, arg)
        if edits is None:
            return
        for start, end, text in changes + edits:
            e.add(start, end, text)
        self.report.auto(rel, line, "connect", f"`{m.group(2)}`: the key arrives typed")

    def _owner_name(self, rel: str, e: _Edits, pos: int, aliases: list[str]) -> str | None:
        if len(set(aliases)) == 1:
            return aliases[0]
        inside = rel == self.fold.template_rel and self.body[0] <= pos < self.body[1]
        return self.fold.template if inside else None

    @staticmethod
    def _enclosing_call(e: _Edits, pos: int) -> int:
        depth = 0
        for i in range(pos - 1, -1, -1):
            ch = e.blanked[i]
            if ch == ")":
                depth += 1
            elif ch == "(":
                if depth == 0:
                    return i
                depth -= 1
        return -1

    def _lambda_argument(self, e: _Edits, pos: int) -> tuple[int, int] | None:
        opener = self._enclosing_call(e, pos)
        if opener < 0:
            return None
        for a, b in _split_args(e.blanked, opener + 1, matching(e.blanked, opener)):
            if a > pos and e.blanked[a:b].lstrip().startswith("["):
                return a, b
        return None

    @staticmethod
    def _lambda_shape(e: _Edits, arg: tuple[int, int]) -> tuple[int, int, int, int] | None:
        """(first parameter, closing parenthesis, opening brace, closing brace) of the lambda in arg."""
        head = _LAMBDA.match(e.blanked, *arg)
        close = matching(e.blanked, head.end() - 1) if head else -1
        tail = _LAMBDA_TAIL.match(e.blanked, close + 1) if close > 0 else None
        brace = tail.end() - 1 if tail else -1
        end = matching(e.blanked, brace) if brace > 0 else -1
        return (head.end(), close, brace, end) if 0 < end <= arg[1] else None

    def _lambda_edits(self, rel: str, e: _Edits, arg: tuple[int, int]) -> list[tuple[int, int, str]] | None:
        if not _LAMBDA.match(e.blanked, *arg):
            return []  # `[this] { ... }`: no parameters to change
        shape = self._lambda_shape(e, arg)
        if shape is None:
            return self._refuse_lambda(rel, e, arg[0], "could not read the lambda's parameters and body")
        first, close, brace, end = shape
        edits: list[tuple[int, int, str]] = []
        for a, b in _split_args(e.blanked, first, close):
            parameter = self._parameter_edits(rel, e, (a, b), (brace + 1, end))
            if parameter is None:
                return None
            edits += parameter
        if not all(e.single_line(s, t) for s, t, _ in edits):
            return self._refuse_lambda(rel, e, arg[0], "an edit would span lines")
        return edits

    def _refuse_lambda(self, rel: str, e: _Edits, pos: int, why: str) -> None:
        self.report.manual(rel, e.line_of(pos) + 1, "connect", f"lambda left as written: {why}")
        return None

    def _parameter_edits(self, rel: str, e: _Edits, param: tuple[int, int],
                         body: tuple[int, int]) -> list[tuple[int, int, str]] | None:
        a, b = param
        seg = e.blanked[a:b]
        m = _VARIANT_PARAM.match(seg)
        if not m:
            if "QVariant" in seg:
                return self._refuse_lambda(rel, e, a, "a parameter is a QVariant of another shape")
            return []
        start, end = a + len(seg) - len(seg.lstrip()), b - (len(seg) - len(seg.rstrip()))
        name = m.group(1)
        key = syntax.FOLD_KEY_ALIAS
        if not name:
            return self._typed(rel, e, start, end, key, "", [])
        return self._named(rel, e, (start, end), name, body)

    def _typed(self, rel: str, e: _Edits, start: int, end: int, key: str, name: str,
               extra: list[tuple[int, int, str]]) -> list[tuple[int, int, str]] | None:
        if key == syntax.FOLD_KEY_ALIAS and not self._encloses_key(e, start):
            return self._refuse_lambda(rel, e, start, f"the class around it has no `using {key} = ...` alias "
                                       "to type the parameter")
        return [(start, end, (syntax.key_parameter(key) + " " + name).rstrip()), *extra]

    @staticmethod
    def _encloses_key(e: _Edits, pos: int) -> bool:
        """Whether the innermost class around pos declares the Key alias."""
        inner = None
        for m in _CLASS_BODY.finditer(e.blanked, 0, pos):
            close = matching(e.blanked, m.end() - 1)
            if close > pos:
                inner = (m.end(), close)
        return bool(inner and _KEY_ALIAS.search(e.blanked, *inner))

    def _named(self, rel: str, e: _Edits, span: tuple[int, int], name: str,
               body: tuple[int, int]) -> list[tuple[int, int, str]] | None:
        scope = (e.blanked, *body)
        unbox = rf"{re.escape(name)}\s*\.\s*value\s*<\s*([\w:]+)\s*>\s*\(\s*\)"
        uses = list(re.compile(rf"{_LOOKBEHIND_USE}{re.escape(name)}\b").finditer(*scope))
        if not uses:
            return self._typed(rel, e, *span, syntax.FOLD_KEY_ALIAS, name, [])
        calls = list(re.compile(rf"{_LOOKBEHIND_USE}{unbox}").finditer(*scope))
        decl = re.compile(rf"(?<=[{{;}}])\s*(?P<stmt>const\s+([\w:]+)\s+(\w+)\s*=\s*{unbox}\s*;)")
        for case in (self._declared_once(decl, scope, uses), self._unboxed_everywhere(calls, uses, name)):
            if case:
                key, variable, extra = case
                return self._typed(rel, e, *span, key, variable, extra)
        return self._refuse_lambda(rel, e, span[0], f"`{name}` is used other than as `{name}.value<T>()`")

    @staticmethod
    def _declared_once(decl: re.Pattern, scope: tuple, uses: list) -> tuple[str, str, list] | None:
        """`const Key key = p.value<Key>();` is the parameter's only use: the parameter is `key` now."""
        found = list(decl.finditer(*scope))
        if len(found) != 1 or len(uses) != 1 or found[0].group(2) not in (found[0].group(4), "auto"):
            return None
        d = found[0]
        return d.group(4), d.group(3), [(*d.span("stmt"), "")]

    @staticmethod
    def _unboxed_everywhere(calls: list, uses: list, name: str) -> tuple[str, str, list] | None:
        """Every use is `p.value<T>()`: each becomes `p`."""
        types = {c.group(1) for c in calls}
        if len(calls) != len(uses) or len(types) != 1:
            return None
        return types.pop(), name, [(*c.span(), name) for c in calls]

    # --- includes, the rest ----------------------------------------------------------

    def _includes(self, rw: ClassRewriter) -> None:
        fold = self.fold
        stem = fold.base_rel.rsplit("/", 1)[-1]
        rx = re.compile(rf'^\s*#\s*include\s*[<"](?:[^">]*/)?{re.escape(stem)}[">]')
        wanted = [syntax.COMPAT_INCLUDE if self.compat else syntax.HEADER_INCLUDE]
        replaced = {rel: self._replace_includes(rel, e, rx, stem) for rel, e in self.edits.items()}
        for line_no in replaced[fold.template_rel]:
            self._add_once(self.sources[fold.template_rel], wanted, line_no)
        if not replaced[fold.template_rel]:
            present = self.edits[fold.template_rel].real
            add_includes(self.sources[fold.template_rel], rw.head_line,
                         [line for line in (syntax.QOBJECT_INCLUDE, *wanted) if line not in present])

    def _replace_includes(self, rel: str, e: _Edits, rx: re.Pattern, stem: str) -> list[int]:
        """The base's include becomes QObject's (or nothing, when QObject is included already); the 0-based
        line after each one is returned."""
        text = "" if syntax.QOBJECT_INCLUDE in e.real else syntax.QOBJECT_INCLUDE
        lines = [i for i, line in enumerate(e.src.lines) if rx.match(line)]
        for i in lines:
            e.add(e.starts[i], e.starts[i] + len(e.src.lines[i].rstrip("\r\n")), text)
            self.report.auto(rel, i + 1, "include", f"`{stem}` is gone; QObject is included instead")
        return [i + 1 for i in lines]

    @staticmethod
    def _add_once(src: Source, lines: list[str], at: int) -> None:
        present = "".join(src.lines)
        for line in lines:
            if line not in present:
                src.insert_before(at, line)

    def _residual(self) -> None:
        rx = re.compile(rf"\b{re.escape(self.fold.base)}\b")
        for rel, src in self.sources.items():
            if rel == self.fold.base_rel:
                continue
            for i, text in enumerate(src.blank_lines):
                if i not in src.deleted and rx.search(text):
                    self.report.manual(rel, i + 1, "signals-only base",
                                       f"still names `{self.fold.base}`, which is folded into `{self.fold.template}`")

    def run(self) -> None:
        fold = self.fold
        signals = self._base_signals()
        rw = self._template() if signals is not None else None
        if rw is None:
            return
        self.names = [s.name for s in signals]
        self.edits = {rel: _Edits(src) for rel, src in self.sources.items()
                      if rel != fold.base_rel or rel == fold.template_rel}
        e = self.edits[fold.template_rel]
        self.body = (e.starts[rw.head_line], e.starts[rw.close_line + 1])
        self._rewrite_template(rw, signals)
        for rel in self.edits:
            self._connect_sites(rel)
        self._includes(rw)
        for edits in self.edits.values():
            edits.apply()
        self._retire_base()
        self._residual()

    def _retire_base(self) -> None:
        """The base is gone from the template's file, or its file is left for a person to delete."""
        fold = self.fold
        if fold.template_rel != fold.base_rel:
            self.report.partial(fold.base_rel, 0, "signals-only base",
                                f"`{fold.base}` is folded into `{fold.template}` and nothing includes this file "
                                "now; delete it")
            return
        src = self.sources[fold.base_rel]
        last = self.base_rw.close_line + (1 if not src.blank_lines[self.base_rw.close_line + 1].strip() else 0)
        for i in range(self.base_rw.head_line, last + 1):
            src.delete(i)
        self.report.auto(fold.base_rel, self.base_rw.head_line + 1, "signals-only base",
                         f"`{fold.base}` removed: its signals moved into `{fold.template}`")
