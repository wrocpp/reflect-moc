"""Migrating a whole source tree: copy, rewrite, diff, report."""

from __future__ import annotations

import difflib
import os
import re
import shutil
from dataclasses import dataclass

from . import cmake, ctors, optin, qml, syntax
from .classes import rewrite_classes
from .moc import JsonProvider
from .report import Report, render
from .sweep import add_includes, rewrite_base_initializers, sweep
from .text import Source, blank, matching

CXX_EXTENSIONS = (".h", ".hh", ".hpp", ".hxx", ".cpp", ".cc", ".cxx", ".c++")
_MOC_MARKER = re.compile(r"\b(Q_OBJECT|Q_GADGET|Q_GADGET_EXPORT|Q_NAMESPACE|Q_NAMESPACE_EXPORT)\b")
_TR_CALL = re.compile(r"(?<![\w.>:])tr\s*\(")


def _calls_tr(blanked: str, cls: str) -> bool:
    """Whether the class body or a `cls::member(...) {...}` definition calls tr()."""
    name = re.escape(cls)
    heads = [re.compile(rf"\b(?:class|struct)\s+[^;{{}}]*?\b{name}\b[^;{{}}]*\{{"),
             re.compile(rf"\b{name}\s*::\s*~?\w+\s*\(")]
    for head in heads:
        for m in head.finditer(blanked):
            open_pos = m.end() - 1
            if blanked[open_pos] == "(":
                close = matching(blanked, open_pos)
                if close < 0:
                    continue
                brace = blanked.find("{", close)
                semi = blanked.find(";", close)
                if brace < 0 or 0 <= semi < brace:
                    continue
                open_pos = brace
            end = matching(blanked, open_pos)
            if end > 0 and _TR_CALL.search(blanked, open_pos, end):
                return True
    return False


@dataclass
class Options:
    reflect_moc_include: str  # absolute, or relative to each CMakeLists.txt
    title: str = ""


@dataclass
class Result:
    files: dict[str, str]  # relative path -> new text (only files that changed or are new)
    diff: str
    report: Report
    report_text: str


def _walk(root: str) -> list[str]:
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if not d.startswith("."))
        for name in sorted(filenames):
            out.append(os.path.relpath(os.path.join(dirpath, name), root))
    return out


def _read(path: str) -> str:
    with open(path, encoding="utf-8", newline="") as f:
        return f.read()


def migrate(src_root: str, provider: JsonProvider, options: Options, dst_root: str | None = None) -> Result:
    """Migrate src_root. With dst_root, the migrated tree is written there (src_root is never changed)."""
    report = Report()
    rels = _walk(src_root)
    texts = {rel: _read(os.path.join(src_root, rel)) for rel in rels
             if rel.endswith(CXX_EXTENSIONS) or os.path.basename(rel) == "CMakeLists.txt"}
    cxx = {rel: t for rel, t in texts.items() if rel.endswith(CXX_EXTENSIONS)}
    cmakes = {rel: t for rel, t in texts.items() if os.path.basename(rel) == "CMakeLists.txt"}

    blanked = {rel: blank(t) for rel, t in cxx.items()}

    def uses_tr(class_name: str) -> bool:
        return any(_calls_tr(b, class_name) for b in blanked.values())

    sources = {rel: Source(rel, t) for rel, t in cxx.items()}
    qml_classes: list[tuple[str, dict]] = []
    per_file: list[tuple[str, list[dict]]] = []
    for rel in sorted(cxx):
        if not _MOC_MARKER.search(cxx[rel]):
            continue
        data, error = provider(os.path.join(src_root, rel), rel)
        if error:
            report.manual(rel, 0, "moc", f"moc could not process the file: {error.splitlines()[0]}")
            continue
        classes = [c for c in data.get("classes", []) if c.get("object") or c.get("gadget")]
        if any(c.get("namespace") for c in data.get("classes", [])):
            report.manual(rel, 0, "Q_NAMESPACE", "namespace meta-objects are not supported")
        if classes:
            per_file.append((rel, classes))
    all_classes = [c for _, classes in per_file for c in classes]
    static_meta = optin.required(list(blanked.values()), all_classes)

    bases: list[tuple[str, str]] = []
    migrated: list[str] = []
    declared_only: list[tuple[str, str]] = []
    for rel, classes in per_file:
        src = sources[rel]
        result = rewrite_classes(src, rel, classes, report, uses_tr, static_meta)
        if result.migrated_classes:
            includes = [syntax.HEADER_INCLUDE]
            if result.uses_tr_include:
                includes.append(syntax.TR_INCLUDE)
            add_includes(src, result.first_class_line, includes)
            report.auto(rel, 0, "reflect-moc include")
        qml_classes += [(rel, c) for c in classes]
        bases += result.bases
        migrated += result.migrated_classes
        declared_only += [(rel, name) for name in result.out_of_line]

    bound: dict[str, int] = {}
    for rel, src in sources.items():
        sweep(src, rel, report)
        rewrite_base_initializers(src, rel, bases, report)
        for cls, n in ctors.bind_out_of_line(src, rel, migrated, report).items():
            bound[cls] = bound.get(cls, 0) + n
    for rel, cls in declared_only:
        if not bound.get(cls):
            report.manual(rel, 0, "constructor", f"`{cls}` declares a constructor whose definition was not found; "
                          "add `bind();` to its body")

    new_files: dict[str, str] = {}
    module = qml.find_module(cmakes)
    qml_generated = False
    if module:
        includes, calls = qml.registrations(qml_classes, module, report)
        if calls:
            header_rel = os.path.join(module.cmake_dir, syntax.QML_HEADER)
            new_files[header_rel] = syntax.qml_header(includes, calls)
            qml_generated = True
            if not qml.insert_calls(sources, module, report):
                report.manual(header_rel, 0, "QML registration call",
                              f"call {syntax.QML_FUNCTION}() before the QML engine is created")
    elif any("QML.Element" in {ci["name"] for ci in c.get("classInfos", [])} for _, c in qml_classes):
        report.manual("CMakeLists.txt", 0, "QML_ELEMENT",
                      "QML types but no qt_add_qml_module(URI ...) to register them under; register by hand")

    cmake_sources = {rel: Source(rel, t) for rel, t in cmakes.items()}
    for rel, src in cmake_sources.items():
        cmake_dir_abs = os.path.join(dst_root or src_root, os.path.dirname(rel))
        include, absolute = cmake.relative_include(options.reflect_moc_include, cmake_dir_abs)
        cmake.rewrite(src, rel, include, absolute, qml_generated, report)

    changed: dict[str, str] = {}
    for rel, src in list(sources.items()) + list(cmake_sources.items()):
        text = src.text()
        if text != texts[rel]:
            changed[rel] = text
    changed.update(new_files)

    diff = unified_diff(texts, changed)
    report_text = render(options.title or os.path.basename(os.path.normpath(src_root)), report, diff)
    if dst_root:
        write_tree(src_root, dst_root, changed)
    return Result(changed, diff, report, report_text)


def unified_diff(before: dict[str, str], after: dict[str, str]) -> str:
    chunks = []
    for rel in sorted(after):
        old = before.get(rel)
        lines = difflib.unified_diff(
            [] if old is None else old.splitlines(keepends=True),
            after[rel].splitlines(keepends=True),
            fromfile="/dev/null" if old is None else f"a/{rel}",
            tofile=f"b/{rel}",
        )
        chunks.append("".join(lines))
    return "".join(chunks)


def write_tree(src_root: str, dst_root: str, changed: dict[str, str]) -> None:
    if os.path.exists(dst_root):
        shutil.rmtree(dst_root)
    shutil.copytree(src_root, dst_root)
    for rel, text in changed.items():
        path = os.path.join(dst_root, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.write(text)
