"""QML registration from moc's class info.

In a moc build qmltyperegistrar reads moc's metatypes JSON and generates the
registration for every QML_ELEMENT-family class of a qt_add_qml_module
target. With moc gone it has nothing to read, so rqt-migrate generates the
equivalent qmlRegister* calls into one header and calls them before the QML
engine is created.
"""

from __future__ import annotations

import os
import re
from dataclasses import dataclass

from . import syntax
from .report import Report
from .sweep import add_includes_at_end_of_includes
from .text import Source

_MODULE = re.compile(r"qt_add_qml_module\s*\(([^)]*)\)", re.S)
_ENGINE = re.compile(r"^(\s*)(?:QQmlEngine|QQmlApplicationEngine|QQuickView|QQuickWidget)\s+\w+\s*[;({]")

# qmlRegisterType covers attached properties (QML_ATTACHED's alias) by itself.
_HANDLED_INFO = {"QML.Element", "QML.Creatable", "QML.UncreatableReason", "QML.Attached"}


@dataclass
class QmlModule:
    uri: str
    major: int
    minor: int
    cmake_dir: str  # relative directory of the CMakeLists.txt that declares it


def find_module(cmake_files: dict[str, str]) -> QmlModule | None:
    for rel, text in sorted(cmake_files.items()):
        m = _MODULE.search(text)
        if not m:
            continue
        words = m.group(1).split()
        uri = words[words.index("URI") + 1] if "URI" in words else None
        version = words[words.index("VERSION") + 1] if "VERSION" in words else "1.0"
        major, _, minor = version.partition(".")
        if uri:
            return QmlModule(uri, int(major), int(minor or 0), os.path.dirname(rel))
    return None


def registrations(classes: list[tuple[str, dict]], module: QmlModule, report: Report) -> tuple[list[str], list[str]]:
    """(header paths relative to the module dir, registration statements)."""
    includes: list[str] = []
    calls: list[str] = []
    for rel, cls in classes:
        info = {ci["name"]: ci["value"] for ci in cls.get("classInfos", [])}
        element = info.get("QML.Element")
        if element is None:
            continue
        name = cls["className"]
        line = cls.get("lineNumber", 0)
        others = sorted(k for k in info if k.startswith("QML.") and k not in _HANDLED_INFO)
        if others:
            report.manual(rel, line, "QML registration",
                          f"`{name}` uses {', '.join(others)}; register it by hand")
            continue
        if element == "anonymous":
            kind, qml_name, reason = "anonymous", name, None
        elif info.get("QML.Creatable") == "false":
            kind, qml_name = "uncreatable", (name if element == "auto" else element)
            reason = f'"{info.get("QML.UncreatableReason", "")}"'  # moc keeps the literal's escapes
        else:
            kind, qml_name, reason = "type", (name if element == "auto" else element), None
        calls.append(syntax.qml_register(kind, cls.get("qualifiedClassName", name), module.uri,
                                         module.major, module.minor, qml_name, reason))
        include = os.path.relpath(rel, module.cmake_dir or ".")
        if include not in includes:
            includes.append(include)
        report.auto(rel, line, "QML registration", f"{kind} `{qml_name}` in {module.uri}")
    return includes, calls


def insert_calls(sources: dict[str, Source], module: QmlModule, report: Report) -> bool:
    """Call the registration before each QML engine is created; False if none was found."""
    header = os.path.join(module.cmake_dir, syntax.QML_HEADER)
    inserted = False
    for rel, src in sorted(sources.items()):
        for i, line in enumerate(src.lines):
            m = _ENGINE.match(src.blank_lines[i])
            if m and i not in src.deleted:
                src.insert_before(i, m.group(1) + syntax.QML_CALL)
                include = os.path.relpath(header, os.path.dirname(rel) or ".")
                add_includes_at_end_of_includes(src, [f'#include "{include}"'])
                report.auto(rel, i + 1, "QML registration call")
                inserted = True
                break
    return inserted
