#!/usr/bin/env python3
"""Checks docs/moc-parity.md, the conformance matrix of reflect-moc against moc.

usage: check_moc_parity.py <docs/moc-parity.md> <tests/qt dir>

A row is a markdown table row with eight cells:
  id | construct | moc spelling | reflect-moc spelling | macro-free spelling | verdict | test | note

  id        a category letter, a dash and two digits, unique
  verdict   same         the QMetaObject and the behaviour equal moc's; needs a test that builds a moc twin
            partial      some of it equals, the note says what does not
            unsupported  does not build or is ignored today; the note says why and what would carry it
            impossible   needs an external file or build step (the note names it) or is not expressible in C++
            not-needed   the construct exists to serve something reflect-moc does not have (a generated file)
  test      the test file without .cpp, found anywhere below tests/qt; mandatory for same, optional otherwise
  note      mandatory for every verdict but same

The macro-free column is a spelling, or '-' when the construct has no macro-free form yet.
"""
import re
import sys
from pathlib import Path

VERDICTS = {"same", "partial", "unsupported", "impossible", "not-needed"}
ID = re.compile(r"^[A-Z]-\d{2}$")


def main(argv):
    doc, tests = Path(argv[1]), Path(argv[2])
    names = {p.stem for p in tests.rglob("*.cpp")}
    problems, seen, rows, counts = [], set(), 0, {}
    for number, line in enumerate(doc.read_text().splitlines(), 1):
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if not line.startswith("|") or len(cells) != 8 or not ID.match(cells[0]):
            continue
        rows += 1
        ident, construct, moc, rqt, free, verdict, test, note = (c.strip("` ") if i != 7 else c for i, c in enumerate(cells))
        where = f"{doc.name}:{number} {ident}"
        if ident in seen:
            problems.append(f"{where}: duplicate id")
        seen.add(ident)
        counts[verdict] = counts.get(verdict, 0) + 1
        for label, value in (("construct", construct), ("moc spelling", moc), ("reflect-moc spelling", rqt), ("macro-free", free)):
            if not value:
                problems.append(f"{where}: {label} is blank (use - for none)")
        if verdict not in VERDICTS:
            problems.append(f"{where}: verdict '{verdict}' is not one of {sorted(VERDICTS)}")
        name = test.partition("::")[0]
        if verdict == "same" and name in {"", "-"}:
            problems.append(f"{where}: a 'same' row needs a test")
        if name not in {"", "-"} and name not in names:
            problems.append(f"{where}: no test file {name}.cpp below {tests}")
        if verdict in VERDICTS - {"same"} and not note.strip():
            problems.append(f"{where}: a '{verdict}' row needs a note")
    for problem in problems:
        print(problem, file=sys.stderr)
    summary = ", ".join(f"{k} {v}" for k, v in sorted(counts.items()))
    print(f"moc parity: {rows} rows ({summary}), {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
