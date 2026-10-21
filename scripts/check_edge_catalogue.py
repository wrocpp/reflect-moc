#!/usr/bin/env python3
"""Checks docs/qt-edge-cases/*.md: every row names a test and a verdict, and the test exists.

usage: check_edge_catalogue.py <catalogue dir> <tests dir>

A row is a markdown table row with five cells: id, case, source, test, verdict.
  id       a category letter, a dash and a number: P-01 protocol, R-01 re-entrancy and lifetime, X-01 proxies and
           selection, V-01 views and QML, D-01 data and types, T-01 threads. Unique across the files.
  case     what is done, in words
  source   where the case comes from: doc:<page>, tester:<check>, QTBUG-<n>, qttest:<file> or own
  test     <test file without .cpp>  or  <test file>::<function name>; the file is in the tests dir and, with a
           function name, mentions it
  verdict  works (the model keeps the contract), refused (it says no and changes nothing), pinned (a known gap, a
           limit_edge_ test shows it) or fixed (a gap that was pinned and is closed; the test is capability_edge_)
A row marked pinned needs a test whose name starts with limit_edge_; works, refused and fixed need capability_edge_.
"""
import re
import sys
from pathlib import Path

VERDICTS = {"works", "refused", "pinned", "fixed"}
ID = re.compile(r"^[PRXVDT]-\d{2,3}$")
SOURCE = re.compile(r"^(doc:.+|tester:.+|QTBUG-\d+|qttest:.+|own)$")


def main(argv):
    catalogue, tests = Path(argv[1]), Path(argv[2])
    problems, seen, rows = [], set(), 0
    for page in sorted(catalogue.glob("*.md")):
        for number, line in enumerate(page.read_text().splitlines(), 1):
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if not line.startswith("|") or len(cells) != 5 or not ID.match(cells[0]):
                continue
            rows += 1
            where = f"{page.name}:{number} {cells[0]}"
            ident, case, source, test, verdict = (c.strip("` ") for c in cells)
            if ident in seen:
                problems.append(f"{where}: duplicate id")
            seen.add(ident)
            if not case:
                problems.append(f"{where}: no case")
            if not SOURCE.match(source):
                problems.append(f"{where}: source '{source}' is not doc:, tester:, QTBUG-n, qttest: or own")
            if verdict not in VERDICTS:
                problems.append(f"{where}: verdict '{verdict}' is not one of {sorted(VERDICTS)}")
            name, _, function = test.partition("::")
            path = tests / f"{name}.cpp"
            if not name or not path.exists():
                problems.append(f"{where}: test '{test}' has no file {path}")
                continue
            if function and function not in path.read_text():
                problems.append(f"{where}: {path.name} does not mention {function}")
            if verdict == "pinned" and not name.startswith("limit_edge_"):
                problems.append(f"{where}: a pinned row needs a limit_edge_ test")
            if verdict in {"works", "refused", "fixed"} and not name.startswith("capability_edge_"):
                problems.append(f"{where}: a {verdict} row needs a capability_edge_ test")
    for problem in problems:
        print(problem, file=sys.stderr)
    print(f"catalogue: {rows} rows, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
