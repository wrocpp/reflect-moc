"""rqt-migrate: rewrite a moc-based Qt source tree for reflect-moc."""

from __future__ import annotations

import argparse
import os
import sys

from . import moc, syntax
from .tree import Options, migrate


def parse(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        prog="rqt-migrate",
        description="Rewrite a moc-based Qt source tree to reflect-moc annotations, using moc's own "
        "--output-json as the source of truth. Prints a unified diff unless --output is given.",
    )
    p.add_argument("source", help="the source tree to migrate (never modified)")
    p.add_argument("-o", "--output", help="write the migrated tree here (replaced if it exists)")
    p.add_argument("--diff", help="write the unified diff to this file")
    p.add_argument("--report", help="write the unmigrated report (Markdown) to this file")
    p.add_argument("--title", default="", help="the report title (default: the source directory name)")
    p.add_argument("--json-dir", help="read moc JSON saved as <dir>/<relative path>.json instead of running moc")
    p.add_argument("--save-json", help="also save the moc JSON under this directory (for --json-dir later)")
    p.add_argument("--moc", help="the moc executable (default: $QTDIR/libexec/moc, then moc on PATH)")
    p.add_argument("-I", dest="includes", action="append", default=[],
                   help="an include dir for moc (Qt's own are added automatically)")
    p.add_argument("--reflect-moc-include", required=True,
                   help="reflect-moc's include dir; written relative to each CMakeLists.txt when possible")
    p.add_argument("--style", choices=syntax.STYLES, default=syntax.ANNOTATIONS,
                   help="annotations: the mixin base, annotations and bind() (default); qtlike: keep the Qt "
                   "macros and class heads, include reflect_moc/compat.hpp, annotate only signals and slots")
    p.add_argument("--signals", choices=syntax.SIGNAL_MODES, default=syntax.SIGNALS_MEMBERS,
                   help="qtlike only. members: each signal becomes `rqt::signal<void(int)> name;` (default); "
                   "bodies: a function annotated [[=rqt::signal]] with an rqt::activate body")
    p.add_argument("--macros", choices=syntax.MACRO_MODES, default=syntax.MACROS_RQT,
                   help="qtlike only. rqt: write the library's RQT_OBJECT / RQT_PROPERTY and annotations "
                   "(default, until compat.hpp exists); q: leave the Q_ macros and include compat.hpp")
    return p.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse(sys.argv[1:] if argv is None else argv)
    if args.json_dir:
        provider = moc.saved(args.json_dir)
    else:
        moc_path = moc.find_moc(args.moc)
        if not moc_path:
            print("rqt-migrate: no moc found; pass --moc or --json-dir", file=sys.stderr)
            return 2
        includes = args.includes + [os.path.abspath(args.source)] + moc.qt_include_dirs(moc_path)
        provider = moc.running(moc_path, includes, args.save_json)
    options = Options(reflect_moc_include=os.path.abspath(args.reflect_moc_include), title=args.title,
                      style=args.style, signals=args.signals, macros=args.macros)
    output = os.path.abspath(args.output) if args.output else None
    result = migrate(os.path.abspath(args.source), provider, options, output)
    if args.diff:
        _write(args.diff, result.diff)
    if args.report:
        _write(args.report, result.report_text)
    if not args.output and not args.diff:
        sys.stdout.write(result.diff)
    counts = result.report.counts()
    print(f"rqt-migrate: {len(result.files)} files changed; {counts['auto']} auto, {counts['partial']} partial, "
          f"{counts['manual']} manual", file=sys.stderr)
    return 0


def _write(path: str, text: str) -> None:
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)
