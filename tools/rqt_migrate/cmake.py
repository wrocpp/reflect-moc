"""CMakeLists.txt edits: moc off, reflection on."""

from __future__ import annotations

import os
import re

from . import syntax
from .report import Report
from .text import Source

_SETUP = re.compile(r"^\s*qt_standard_project_setup\s*\(", re.I)
_FIND_QT = re.compile(r"^\s*find_package\s*\(\s*Qt6\b", re.I)
_AUTOMOC_SET = re.compile(r"^(\s*set\s*\(\s*CMAKE_AUTOMOC\s+)(ON|TRUE|YES|1)(\s*\))", re.I)
_AUTOMOC_PROP = re.compile(r"\bAUTOMOC(\s+)(ON|TRUE|YES|1)\b", re.I)
_WRAP_CPP = re.compile(r"^\s*(qt[56]?_wrap_cpp)\s*\(", re.I)
_QML_MODULE = re.compile(r"^\s*qt_add_qml_module\s*\(", re.I)
_URI = re.compile(r"\bURI\s+\S+")


def _statement_end(src: Source, first: int) -> int:
    """The line closing the parenthesis opened on line `first`."""
    depth = 0
    for i in range(first, len(src.lines)):
        for ch in src.lines[i].split("#", 1)[0]:
            if ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    return i
    return first


def rewrite(src: Source, rel: str, include_dir: str, include_is_absolute: bool, qml_types_generated: bool,
            report: Report, style: str = syntax.ANNOTATIONS) -> None:
    anchor = -1
    for i, line in enumerate(src.lines):
        if _SETUP.match(line):
            anchor = _statement_end(src, i)
            break
    if anchor < 0:
        for i, line in enumerate(src.lines):
            if _FIND_QT.match(line):
                anchor = _statement_end(src, i)
                break
    if anchor < 0:
        report.manual(rel, 0, "CMake", "no find_package(Qt6) or qt_standard_project_setup(); "
                      "turn AUTOMOC off and add reflect-moc by hand")
        return
    block = syntax.cmake_block(style).format(include_path=syntax.cmake_include_path(include_dir, include_is_absolute))
    src.insert_before(anchor + 1, "\n" + block.rstrip("\n"))
    report.auto(rel, anchor + 1, "CMake: AUTOMOC off, reflection flags, include dir")

    for i, line in enumerate(src.lines):
        text = line.rstrip("\r\n")
        if _AUTOMOC_SET.match(text):
            src.replace(i, _AUTOMOC_SET.sub(r"\1OFF\3", text))
            report.auto(rel, i + 1, "CMake: AUTOMOC ON")
        elif _AUTOMOC_PROP.search(text.split("#", 1)[0]):
            src.replace(i, _AUTOMOC_PROP.sub(r"AUTOMOC\1OFF", text))
            report.auto(rel, i + 1, "CMake: AUTOMOC ON")
        m = _WRAP_CPP.match(text)
        if m:
            report.manual(rel, i + 1, f"CMake: {m.group(1)}", "an explicit moc step; remove it once its headers are migrated")
        if _QML_MODULE.match(text) and qml_types_generated:
            end = _statement_end(src, i)
            for j in range(i, end + 1):
                uri = _URI.search(src.lines[j])
                if uri:
                    body = src.lines[j].rstrip("\r\n")
                    src.replace(j, body[: uri.end()] + " " + syntax.CMAKE_QML_NO_TYPES + body[uri.end():])
                    report.auto(rel, j + 1, "CMake: qt_add_qml_module without qmltypes")
                    break


def relative_include(include_dir: str, cmake_dir_abs: str) -> tuple[str, bool]:
    """include_dir as CMake should spell it from cmake_dir_abs: relative when it can be."""
    if not os.path.isabs(include_dir):
        return include_dir, False
    try:
        return os.path.relpath(include_dir, cmake_dir_abs), False
    except ValueError:
        return include_dir, True
