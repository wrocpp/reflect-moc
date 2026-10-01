"""Rewrites the classes moc's JSON describes in one file.

moc's JSON is the source of truth for what each class declares: its bases,
signals, slots, invokables, properties, enums, class info, and the line each
one is on. The text is only searched to find the exact span on that line.
"""

from __future__ import annotations

import re
from dataclasses import dataclass, field

from . import ctors, syntax
from .report import Report
from .text import Source, has_top_level, indent_of, matching, split_top_level

# Macros that look like moc's but are plain C++: left alone.
NOT_MOC = {
    "Q_DISABLE_COPY",
    "Q_DISABLE_COPY_MOVE",
    "Q_DECLARE_PRIVATE",
    "Q_DECLARE_PRIVATE_D",
    "Q_DECLARE_PUBLIC",
    "Q_DECLARE_TR_FUNCTIONS",
    "Q_DECLARE_FLAGS",
    "Q_SIGNAL",
    "Q_SLOT",
    "Q_INVOKABLE",
    "Q_EMIT",
}

# QML registration macros: kept (they still declare the type aliases
# qmlRegisterType reads), and registered from moc's class info by qml.py.
QML_MACROS = {
    "QML_ELEMENT",
    "QML_NAMED_ELEMENT",
    "QML_ANONYMOUS",
    "QML_UNCREATABLE",
    "QML_ATTACHED",
    "QML_SINGLETON",
    "QML_FOREIGN",
    "QML_FOREIGN_NAMESPACE",
    "QML_EXTENDED",
    "QML_EXTENDED_NAMESPACE",
    "QML_VALUE_TYPE",
    "QML_INTERFACE",
    "QML_IMPLEMENTS_INTERFACES",
    "QML_ADDED_IN_VERSION",
    "QML_REMOVED_IN_VERSION",
    "QML_ADDED_IN_MINOR_VERSION",
    "QML_SEQUENTIAL_CONTAINER",
    "QML_STRUCTURED_VALUE",
    "QML_CONSTRUCTIBLE_VALUE",
}

# moc constructs with no reflect-moc form yet: left in place and reported.
UNSUPPORTED = {
    "Q_PRIVATE_SLOT": "a slot implemented in the d-pointer class; declare a real member function and annotate it",
    "Q_PRIVATE_PROPERTY": "a property on the d-pointer; add accessors to the class and annotate the READ accessor",
    "Q_PLUGIN_METADATA": "plugin metadata is emitted by moc into a .qtmetadata section; reflect-moc has no equivalent",
    "Q_REVISION": "method and property revisions are not carried by the annotations",
    "Q_SCRIPTABLE": "the scriptable flag is not carried by the annotations",
    "Q_MOC_INCLUDE": "a moc-only include; add the include to the source if the type is needed",
    "Q_ENUM_NS": "namespace meta-objects (Q_NAMESPACE) are not supported",
    "Q_FLAG_NS": "namespace meta-objects (Q_NAMESPACE) are not supported",
    "Q_NAMESPACE": "namespace meta-objects are not supported",
    "Q_NAMESPACE_EXPORT": "namespace meta-objects are not supported",
    "Q_OBJECT_FAKE": "Q_OBJECT_FAKE is not supported",
}

_MACRO_LINE = re.compile(r"^\s*((?:Q|QML)_[A-Z0-9_]+)\b")
_SIGNALS_SECTION = re.compile(r"^(\s*)(?:signals|Q_SIGNALS)(\s*):")
_SLOTS_SECTION = re.compile(r"^(\s*)(public|protected|private)\s+(?:slots|Q_SLOTS)(\s*):")

# Property attributes and the value that means "moc's default"; anything else
# has no annotation field yet.
_ACCESS_LABEL = re.compile(r"^\s*(public|protected|private)\b(?:\s+(?:slots|Q_SLOTS))?\s*:(?!:)")
_SIGNALS_LABEL = re.compile(r"^\s*(?:signals|Q_SIGNALS)\s*:")
_CLASS_KEY = re.compile(r"\b(class|struct)\b")


@dataclass
class FileResult:
    migrated_classes: list[str] = field(default_factory=list)
    first_class_line: int = -1
    uses_tr_include: bool = False
    bases: list[tuple[str, str]] = field(default_factory=list)  # (class, replaced base)
    out_of_line: list[str] = field(default_factory=list)  # classes whose constructors are defined elsewhere


class ClassRewriter:
    def __init__(self, src: Source, rel: str, cls: dict, report: Report, uses_tr: bool, static_meta: str | None = None,
                 object_names: set[str] | None = None, style: str = syntax.ANNOTATIONS):
        self.style = style
        self.src = src
        self.rel = rel
        self.cls = cls
        self.name = cls["className"]
        self.report = report
        self.uses_tr = uses_tr
        self.static_meta = static_meta  # why the class needs the tier B opt-in, or None
        self.class_annotations: list[str] = []
        self.base: str | None = None  # the Qt base an rqt::Object<> replaced; None when none was inserted
        self.direct_base: str | None = None  # how the class now spells its direct base
        self.object_names = object_names or set()
        self.property_index: dict[str, int] = {}  # set when Q_PROPERTY order differs from declaration order
        self.object_line: int | None = None  # where Q_OBJECT was: generated members go here
        self.interfaces: list[str] = []
        self.ctor_count = 0  # constructors declared in the class body
        self.ctor_declared_only = False  # at least one is defined outside the class
        self.using_line: int | None = None

    # --- locating ---------------------------------------------------------

    def locate(self) -> bool:
        line = self.cls["lineNumber"] - 1
        head = re.compile(rf"\b(class|struct)\b[^;{{]*?\b{re.escape(self.name)}\b")
        for i in [line, line + 1, line - 1, line + 2]:
            if 0 <= i < len(self.src.lines) and head.search(self.src.blank_lines[i]):
                self.head_line = i
                break
        else:
            self.report.manual(self.rel, line + 1, "class", f"could not find the head of class {self.name}")
            return False
        rest = "".join(self.src.blank_lines[self.head_line :])
        brace = rest.find("{")
        close = matching(rest, brace) if brace >= 0 else -1
        if close < 0:
            self.report.manual(self.rel, self.head_line + 1, "class", f"could not find the body of class {self.name}")
            return False
        self.open_line = self.head_line + rest[:brace].count("\n")
        self.close_line = self.head_line + rest[:close].count("\n")
        self._member_lines()
        return True

    def _member_lines(self) -> None:
        """Lines whose start is directly inside the class braces (depth 1)."""
        self.members: list[int] = []
        depth = 0
        for i in range(self.open_line, self.close_line + 1):
            line = self.src.blank_lines[i]
            start_depth = depth
            for ch in line:
                if ch == "{":
                    depth += 1
                elif ch == "}":
                    depth -= 1
            if i == self.open_line:
                continue
            if start_depth == 1 and line.strip():
                self.members.append(i)

    def _template_class(self) -> bool:
        for i in range(self.head_line - 1, max(-1, self.head_line - 4), -1):
            text = self.src.blank_lines[i].strip()
            if text:
                return text.startswith("template")
        return False

    # --- the rewrite ------------------------------------------------------

    def run(self) -> bool:
        if not self.locate():
            return False
        if self._template_class():
            self.report.manual(self.rel, self.head_line + 1, "template class",
                               "class templates cannot carry Q_OBJECT; port by hand")
            return False
        is_object = bool(self.cls.get("object"))
        if self.style == syntax.QTLIKE:
            return self._run_qtlike()
        if self.cls.get("gadget"):
            self.report.manual(self.rel, self.head_line + 1, "Q_GADGET",
                               f"`{self.name}`: reflect-moc has no gadget annotation; Q_GADGET needs moc")
            return False
        self._macros()
        self._sections()
        self._constructors()
        for sig in _dedupe(self.cls.get("signals", [])):
            self._signal(sig)
        overloaded = _overloads(self.cls.get("signals", []))
        for name, line in overloaded:
            self.report.partial(self.rel, line, "overloaded signal",
                                f"`{name}` is overloaded: pointer-to-member connects need QOverload, "
                                "and each overload must emit its own index")
        for slot in _dedupe(self.cls.get("slots", [])):
            self._annotate_method(slot, syntax.SLOT, "Q_SLOT", "slot")
        for method in _dedupe(self.cls.get("methods", [])):
            self._annotate_method(method, syntax.INVOKABLE, "Q_INVOKABLE", "Q_INVOKABLE")
        for ctor in _dedupe(self.cls.get("constructors", [])):
            self.report.manual(self.rel, ctor.get("lineNumber", 0), "Q_INVOKABLE constructor",
                               "invokable constructors (QMetaObject::newInstance) are not supported")
        self._property_order()
        for prop in self.cls.get("properties", []):
            self._property(prop)
        for enum in self.cls.get("enums", []):
            self._enum(enum)
        self._class_info()
        if is_object:
            self._base()
        self._generated_members()
        self._class_annotations()
        return True

    def _run_qtlike(self) -> bool:
        """qtlike: the class head, the macros, the sections and the constructors stay as Qt
        wrote them (compat.hpp gives the macros their reflect-moc meaning). Each signal gets
        its annotation and body; each slot gets its annotation."""
        for sig in _dedupe(self.cls.get("signals", [])):
            self._signal(sig)
        for name, line in _overloads(self.cls.get("signals", [])):
            self.report.partial(self.rel, line, "overloaded signal",
                                f"`{name}` is overloaded: pointer-to-member connects need QOverload, "
                                "and each overload must emit its own index")
        for slot in _dedupe(self.cls.get("slots", [])):
            self._annotate_method(slot, syntax.SLOT, "Q_SLOT", "slot")
        return True

    def _macros(self) -> None:
        for i in self.members:
            if i in self.src.deleted:
                continue
            m = _MACRO_LINE.match(self.src.blank_lines[i])
            if not m:
                continue
            macro = m.group(1)
            end = self._macro_end(i, m.end())
            if macro == "Q_OBJECT":
                self.object_line = i
                if self.uses_tr:
                    self.src.replace(i, indent_of(self.src.lines[i]) + syntax.TR_FUNCTIONS.format(cls=self.name))
                    self.report.auto(self.rel, i + 1, "Q_OBJECT", "replaced by Q_DECLARE_TR_FUNCTIONS: the class calls tr()")
                else:
                    self._delete_span(i, end)
                    self.report.auto(self.rel, i + 1, macro)
            elif macro in ("Q_GADGET", "Q_GADGET_EXPORT"):
                continue  # a gadget never gets here (run() reports it)
            elif macro == "Q_PROPERTY":
                self._delete_span(i, end)
            elif macro == "Q_CLASSINFO":
                # The annotation comes from moc's classInfos (see _class_info).
                self._delete_span(i, end)
                self.report.auto(self.rel, i + 1, "Q_CLASSINFO")
            elif macro == "Q_INTERFACES":
                self.interfaces += " ".join(self._macro_args(i, end)).split()
                self._delete_span(i, end)
            elif macro in ("Q_ENUM", "Q_FLAG", "Q_ENUMS", "Q_FLAGS"):
                self._delete_span(i, end)
            elif macro in QML_MACROS or macro in NOT_MOC or macro.startswith("Q_DECL_"):
                continue
            elif macro in UNSUPPORTED:
                self.report.manual(self.rel, i + 1, macro, UNSUPPORTED[macro])
            else:
                self.report.manual(self.rel, i + 1, macro, "unknown macro; check whether moc reads it")

    def _class_info(self) -> None:
        """Every class info moc put in the meta-object, in moc's order.

        That includes the ones QML_ELEMENT and its family expand to: the macros
        stay in the source for the type aliases they declare, but their
        Q_CLASSINFO part expands to nothing without moc.
        """
        # moc's JSON keeps each literal's contents as written, escapes included.
        infos = [syntax.classinfo(f'"{ci["name"]}"', f'"{ci["value"]}"') for ci in self.cls.get("classInfos", [])]
        self.class_annotations[:0] = infos

    def _macro_end(self, line: int, col: int) -> int:
        text = self.src.blank_lines[line]
        rest = text[col:]
        stripped = rest.lstrip()
        if not stripped.startswith("("):
            return line
        open_col = col + (len(rest) - len(stripped))
        end_line, _ = self.src.balanced_end_line(line, open_col)
        return end_line if end_line >= 0 else line

    def _macro_args(self, first: int, last: int) -> list[str]:
        real, blanked = self.src.joined(first, last)
        open_pos = blanked.find("(")
        close = matching(blanked, open_pos)
        return [real[a:b].strip() for a, b in split_top_level(blanked, open_pos + 1, close)]

    def _delete_span(self, first: int, last: int) -> None:
        for i in range(first, last + 1):
            self.src.delete(i)

    def _sections(self) -> None:
        for i in self.members:
            line = self.src.lines[i]
            m = _SIGNALS_SECTION.match(self.src.blank_lines[i])
            if m:
                self.src.replace(i, _SIGNALS_SECTION.sub(r"\1public\2:", line.rstrip("\r\n"), count=1))
                self.report.auto(self.rel, i + 1, "signals: section")
                continue
            m = _SLOTS_SECTION.match(self.src.blank_lines[i])
            if m:
                self.src.replace(i, _SLOTS_SECTION.sub(r"\1\2\3:", line.rstrip("\r\n"), count=1))
                self.report.auto(self.rel, i + 1, "slots: section")

    # --- signals ----------------------------------------------------------

    def _signal(self, sig: dict) -> None:
        name = sig["name"]
        line = sig["lineNumber"] - 1
        if sig.get("returnType", "void") != "void":
            self.report.manual(self.rel, line + 1, "signal", f"`{name}` returns {sig['returnType']}; "
                               "reflect-moc signals return void")
            return
        found = self._find_call(line, name)
        if not found:
            self.report.manual(self.rel, line + 1, "signal", f"could not find the declaration of `{name}`")
            return
        first, _ = found
        semi_line = self.src.find_line_end(first, ";")
        if semi_line < 0:
            self.report.manual(self.rel, line + 1, "signal", f"could not find the end of `{name}`")
            return
        real, blanked = self.src.joined(first, semi_line)
        m = re.compile(rf"\b{re.escape(name)}\s*\(").search(blanked)
        open_pos = m.end() - 1
        close = matching(blanked, open_pos)
        semi = blanked.find(";", close)
        between = blanked[close + 1 : semi].strip()
        if between:
            self.report.manual(self.rel, line + 1, "signal",
                               f"`{name}` is declared `{between}`; only plain signals are migrated")
            return
        params, names, has_default = self._signal_params(real, blanked, open_pos, close, sig.get("arguments", []))
        if has_default:
            self.report.partial(self.rel, line + 1, "signal with default arguments",
                                f"`{name}`: moc also registers the shorter overloads; string connects to them fail")
        indent = indent_of(real)
        prefix = re.sub(r"\bQ_SIGNAL\s+", "", real[len(indent) : m.start()])
        tail = real[semi + 1 :].rstrip("\r\n")
        new = (indent + syntax.SIGNAL + " " + prefix + real[m.start() : open_pos + 1] + params
               + real[close:semi].rstrip() + " " + syntax.signal_body(names, self.style) + tail)
        self.src.replace_span(first, semi_line, new)
        self.report.auto(self.rel, line + 1, "signal")

    def _signal_params(self, real: str, blanked: str, open_pos: int, close: int,
                       arguments: list[dict]) -> tuple[str, list[str], bool]:
        inner = blanked[open_pos + 1 : close]
        if not inner.strip() or inner.strip() == "void":
            return real[open_pos + 1 : close], [], False
        pieces = []
        names = []
        has_default = False
        for index, (a, b) in enumerate(split_top_level(blanked, open_pos + 1, close)):
            text = real[a:b]
            eq = has_top_level(blanked, a, b, "=")
            if eq >= 0:
                has_default = True
            arg_name = arguments[index].get("name", "") if index < len(arguments) else ""
            if not arg_name:
                arg_name = f"arg{index}"
                cut = (eq - a) if eq >= 0 else len(text.rstrip())
                text = text[:cut].rstrip() + " " + arg_name + (" " + text[cut:].lstrip() if eq >= 0 else text[cut:])
            names.append(arg_name)
            pieces.append(text)
        return ",".join(pieces), names, has_default

    def _find_call(self, line: int, name: str) -> tuple[int, int] | None:
        pattern = re.compile(rf"\b{re.escape(name)}\s*\(")
        for i in [line, line + 1]:
            if 0 <= i < len(self.src.lines) and i not in self.src.deleted:
                m = pattern.search(self.src.blank_lines[i])
                if m:
                    return i, m.start()
        return None

    # --- slots and invokables ---------------------------------------------

    def _annotate_method(self, method: dict, annotation: str, macro: str, construct: str) -> None:
        name = method["name"]
        line = method["lineNumber"] - 1
        found = self._find_call(line, name)
        if not found:
            self.report.manual(self.rel, line + 1, construct, f"could not find the declaration of `{name}`")
            return
        i, _ = found
        if re.match(r"\s*Q_PRIVATE_SLOT\b", self.src.blank_lines[i]):
            return  # reported with the macro
        if "revision" in method:
            self.report.partial(self.rel, line + 1, f"{construct} revision",
                                f"`{name}`: Q_REVISION is not carried by the annotation")
        # The macro may sit on the declaration line or on the line before it.
        for j in [i, i - 1]:
            if j >= 0 and j not in self.src.deleted and re.search(rf"\b{macro}\b", self.src.blank_lines[j]):
                text = self.src.lines[j].rstrip("\r\n")
                rest = re.sub(rf"\b{macro}\b\s*", "", text, count=1)
                if rest.strip():
                    self.src.replace(j, indent_of(rest) + annotation + " " + rest.lstrip())
                else:
                    self.src.replace(j, indent_of(text) + annotation)
                break
        else:
            text = self.src.lines[i].rstrip("\r\n")
            self.src.replace(i, indent_of(text) + annotation + " " + text.lstrip())
        if self._has_default_args(i):
            self.report.partial(self.rel, line + 1, f"{construct} with default arguments",
                                f"`{name}`: moc also registers the shorter overloads; invoking them by name fails")
        else:
            self.report.auto(self.rel, line + 1, construct)

    def _has_default_args(self, line: int) -> bool:
        blanked = self.src.blank_lines[line]
        open_pos = blanked.find("(")
        if open_pos < 0:
            return False
        close = matching(blanked, open_pos)
        if close < 0:
            return False
        return has_top_level(blanked, open_pos + 1, close, "=") >= 0

    # --- properties -------------------------------------------------------

    def _property(self, prop: dict) -> None:
        name = prop["name"]
        line = prop.get("lineNumber", 0)
        if prop.get("privateClass"):
            self.report.manual(self.rel, line, "Q_PRIVATE_PROPERTY", UNSUPPORTED["Q_PRIVATE_PROPERTY"])
            return
        unsupported = [k for k in ("bindable", "revision") if k in prop]
        fields: dict[str, str | bool | int] = {}
        read, member = prop.get("read"), prop.get("member")
        if not (read or member):
            self.report.manual(self.rel, line, "Q_PROPERTY", f"`{name}` has neither READ nor MEMBER")
            return
        target, what = self._property_target(prop)
        if target is None:
            self.report.manual(self.rel, line, "Q_PROPERTY",
                               f"`{name}`: the {what} is not declared in the class body")
            return
        if (read or member) != name:
            fields["name"] = name
        if name in self.property_index:
            fields["index"] = self.property_index[name]
        for key in ("write", "notify", "reset"):
            if prop.get(key):
                fields[key] = prop[key]
        for key, default in syntax.PROPERTY_FLAG_FIELDS.items():
            value = prop.get(key, default)
            if not isinstance(value, bool):
                unsupported.append(f"{key.upper()} (an expression)")
            elif value != default:
                fields[key] = value
        text = self.src.lines[target].rstrip("\r\n")
        self.src.replace(target, indent_of(text) + syntax.property(fields) + " " + text.lstrip())
        if unsupported:
            self.report.partial(self.rel, line, "Q_PROPERTY",
                                f"`{name}`: {', '.join(unsupported)} not carried by the annotation")
        else:
            self.report.auto(self.rel, line, "Q_PROPERTY")

    def _property_target(self, prop: dict) -> tuple[int | None, str]:
        """The line of the declaration the annotation goes on, and a name for it."""
        read, member = prop.get("read"), prop.get("member")
        if read:
            return self._find_member_declaration(rf"\b{re.escape(read)}\s*\("), f"READ accessor `{read}`"
        return (self._find_member_declaration(rf"\b{re.escape(member)}\b\s*(=|;|\{{|\[|,)"),
                f"MEMBER `{member}`")

    def _property_order(self) -> None:
        """moc orders properties by their Q_PROPERTY lines, reflect-moc by the position of the
        annotated declaration. When the two differ, every property of the class gets .index."""
        placed = []
        for prop in self.cls.get("properties", []):
            if prop.get("privateClass") or not (prop.get("read") or prop.get("member")):
                continue
            target, _ = self._property_target(prop)
            if target is not None:
                placed.append((prop["name"], target))
        if [t for _, t in placed] != sorted(t for _, t in placed):
            self.property_index = {name: i for i, (name, _) in enumerate(placed)}

    def _find_member_declaration(self, pattern: str) -> int | None:
        rx = re.compile(pattern)
        for i in self.members:
            if i in self.src.deleted:
                continue
            blanked = self.src.blank_lines[i]
            if _MACRO_LINE.match(blanked):
                continue
            if rx.search(blanked):
                return i
        return None

    # --- enums ------------------------------------------------------------

    def _enum(self, enum: dict) -> None:
        is_flag = bool(enum.get("isFlag"))
        target = enum.get("alias") or enum["name"]
        construct = "Q_FLAG" if is_flag else "Q_ENUM"
        rx = re.compile(rf"\benum\b(\s+(?:class|struct)\b)?(\s*)(?={re.escape(target)}\b)")
        for i in self.members:
            if i in self.src.deleted:
                continue
            m = rx.search(self.src.blank_lines[i])
            if m:
                text = self.src.lines[i].rstrip("\r\n")
                key_end = m.end(1) if m.group(1) else m.start() + len("enum")
                self.src.replace(i, text[:key_end] + " " + syntax.enum(is_flag) + " " + text[m.end():])
                self.report.partial(self.rel, i + 1, construct,
                                    f"`{target}` annotated; QMetaEnum::fromType and enum names in QDebug/QVariant "
                                    "need the qt_getEnumMetaObject friend that Q_ENUM declared")
                return
        self.report.manual(self.rel, self.head_line + 1, construct,
                           f"enum `{target}` is not declared in the class body")

    # --- the class head -------------------------------------------------------

    def _base(self) -> None:
        supers = self.cls.get("superClasses", [])
        if not supers:
            self.report.manual(self.rel, self.head_line + 1, "base class", f"{self.name} has no base class")
            return
        base = supers[0]["name"]
        if base.split("::")[-1] in self.object_names:
            # A class derived from another migrated class keeps that base: rqt::Object<Base>
            # would repeat rqt::object_tag, an ambiguous base that hides the class from Qt's templates.
            self.direct_base = base
            self.report.auto(self.rel, self.head_line + 1, "base class", f"`{base}` is itself migrated: kept")
            self._inherited_constructors(base)
            return
        rx = re.compile(rf"(:\s*(?:(?:public|protected|private|virtual)\s+)*){re.escape(base)}\b")
        for i in range(self.head_line, self.open_line + 1):
            m = rx.search(self.src.blank_lines[i])
            if m:
                text = self.src.lines[i].rstrip("\r\n")
                start = m.end(1)
                self.src.replace(i, text[:start] + syntax.object_base(base) + text[start + len(base):])
                self.report.auto(self.rel, i + 1, "base class")
                self.base = base
                self.direct_base = syntax.object_base(base)
                self._inherited_constructors(base)
                return
        self.report.manual(self.rel, self.head_line + 1, "base class", f"could not find base `{base}` in the head")

    def _inherited_constructors(self, base: str) -> None:
        """`using Base::Base;` names a direct base. A class with no constructor of its own
        gets a forwarding one instead: an inherited constructor has no body to call bind() in."""
        rx = re.compile(rf"^(\s*)using\s+{re.escape(base)}\s*::\s*{re.escape(base.split('::')[-1])}\s*;")
        for i in self.members:
            m = rx.match(self.src.blank_lines[i])
            if m:
                text = self.src.lines[i].rstrip("\r\n")
                self.using_line = i
                if self.ctor_count:
                    new = syntax.object_base_using(base) if self.base else text[:m.end()].strip()
                    if self.base:
                        self.report.auto(self.rel, i + 1, "inherited constructors")
                else:
                    new = syntax.forwarding_constructor(self.name, self.direct_base)
                    self.report.auto(self.rel, i + 1, "inherited constructors",
                                     "replaced by a forwarding constructor that calls bind()")
                self.src.replace(i, m.group(1) + new + text[m.end():])

    # --- constructors and generated members ---------------------------------

    def _constructors(self) -> None:
        """bind(); in each constructor defined in the class body, and the count of constructors."""
        ctor = re.compile(rf"(?<![\w~:.>]){re.escape(self.name)}\s*\(")
        edits = ctors.Edits(self.src)
        for i in self.members:
            if i in self.src.deleted:
                continue
            start = edits.starts[i - 1] if i else 0
            m = ctor.match(edits.blanked, start + len(indent_of(self.src.blank_lines[i]))) or self._after_specifiers(
                ctor, edits.blanked, start, i)
            if not m:
                continue
            close = matching(edits.blanked, m.end() - 1)
            if close < 0 or ctors.is_copy_or_move(edits.blanked[m.end() : close], self.name):
                continue
            kind, pos = ctors.tail(edits.blanked, close)
            if kind == ctors.DELETED:
                continue
            self.ctor_count += 1
            if kind == ctors.DECLARATION:
                self.ctor_declared_only = True
            elif kind == ctors.DEFAULTED:
                edits.replace_default(pos)
            elif not ctors.already_binds(edits.blanked, pos):
                edits.bind_after_brace(pos)
        for line in edits.apply():
            self.report.auto(self.rel, line, "constructor", "bind() added")

    @staticmethod
    def _after_specifiers(ctor: re.Pattern, blanked: str, start: int, i: int):
        """`explicit` / `constexpr` / `inline` before the constructor's name."""
        lead = re.compile(r"\s*(?:(?:explicit|constexpr|inline|Q_INVOKABLE)\b\s*)+")
        m = lead.match(blanked, start)
        return ctor.match(blanked, m.end()) if m else None

    def _access_at(self, line: int) -> str:
        """The access specifier in force at member line `line`."""
        head = _CLASS_KEY.search(self.src.blank_lines[self.head_line])
        access = "private" if head and head.group(1) == "class" else "public"
        for i in self.members:
            if i >= line:
                break
            if i in self.src.deleted:
                continue
            m = _ACCESS_LABEL.match(self.src.blank_lines[i])
            if m:
                access = m.group(1)
            elif _SIGNALS_LABEL.match(self.src.blank_lines[i]):
                access = "public"
        return access

    def _next_label(self, line: int) -> tuple[int, str] | None:
        """The access specifier that ends the run of members starting after `line`, if the run
        reaches one before any other member: (its line, its access)."""
        for i in self.members:
            if i <= line or i in self.src.deleted or not self.src.blank_lines[i].strip():
                continue
            m = _ACCESS_LABEL.match(self.src.blank_lines[i])
            if m:
                return i, m.group(1)
            if _SIGNALS_LABEL.match(self.src.blank_lines[i]):
                return i, "public"
            if not _MACRO_LINE.match(self.src.blank_lines[i]):
                return None
        return None

    def _generated_members(self) -> None:
        """What Q_OBJECT's expansion carried that has no annotation: the tier B opt-in, the
        interface metacast and, for a class with no constructor, one that binds."""
        if self.object_line is None:
            return
        lines: list[str] = []
        if self.static_meta:
            lines.append(syntax.STATIC_META_OBJECT_DECLARATION)
            self.src.insert_before(self.close_line + 1, syntax.STATIC_META_OBJECT_DEFINITION.format(cls=self.name))
            self.report.auto(self.rel, self.head_line + 1, "static metaobject opt-in", self.static_meta)
        if self.interfaces and self.direct_base:
            lines += syntax.interface_metacast(self.direct_base, self.interfaces)
            self.report.auto(self.rel, self.head_line + 1, "Q_INTERFACES", "qt_metacast override answers "
                             + ", ".join(self.interfaces))
        if not self.ctor_count and self.using_line is None:
            lines.append(syntax.default_constructor(self.name))
            self.report.auto(self.rel, self.head_line + 1, "constructor", "default constructor with bind() added")
        if not lines:
            return
        i = self.object_line
        access = self._access_at(i)
        pad = indent_of(self.src.lines[i])
        label_pad = indent_of(self.src.lines[self.head_line])
        following = self._next_label(i)
        if access == "public":
            at, before, after = i, [], []
        elif following and following[1] == "public":
            at, before, after = following[0] + 1, [], []  # the next label is public: join its run
        else:
            at, before = i, [label_pad + "public:"]
            after = [] if following else [label_pad + access + ":"]
        for line in before + [pad + line for line in lines] + after:
            self.src.insert_before(at, line)

    def _class_annotations(self) -> None:
        if not self.class_annotations:
            return
        text = self.src.lines[self.head_line].rstrip("\r\n")
        m = re.compile(rf"\b(class|struct)\b(?=[^;{{]*?\b{re.escape(self.name)}\b)").search(text)
        at = m.end()
        self.src.replace(self.head_line, text[:at] + " " + " ".join(self.class_annotations) + text[at:])


def _dedupe(methods: list[dict]) -> list[dict]:
    seen = set()
    out = []
    for m in methods:
        key = (m["name"], m.get("lineNumber"))
        if key in seen or m.get("isCloned"):
            continue
        seen.add(key)
        out.append(m)
    return out


def _overloads(signals: list[dict]) -> list[tuple[str, int]]:
    by_name: dict[str, list[int]] = {}
    for s in _dedupe(signals):
        by_name.setdefault(s["name"], []).append(s.get("lineNumber", 0))
    return [(name, lines[1]) for name, lines in by_name.items() if len(lines) > 1]


def rewrite_classes(src: Source, rel: str, classes: list[dict], report: Report, uses_tr,
                    static_meta: dict[str, str] | None = None, object_names: set[str] | None = None,
                    style: str = syntax.ANNOTATIONS) -> FileResult:
    """static_meta maps a class name to the reason it needs the tier B opt-in; object_names
    is every QObject class of the tree that is being migrated."""
    result = FileResult()
    static_meta = static_meta or {}
    for cls in classes:
        rewriter = ClassRewriter(src, rel, cls, report, uses_tr(cls["className"]), static_meta.get(cls["className"]),
                                 object_names, style)
        if rewriter.run():
            result.migrated_classes.append(cls["className"])
            if rewriter.ctor_declared_only:
                result.out_of_line.append(cls["className"])
            if result.first_class_line < 0 or rewriter.head_line < result.first_class_line:
                result.first_class_line = rewriter.head_line
            result.uses_tr_include |= rewriter.uses_tr and bool(cls.get("object"))
            if rewriter.base:
                result.bases.append((cls["className"], rewriter.base))
    return result
