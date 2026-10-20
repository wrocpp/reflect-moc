#!/usr/bin/env python3
"""Median (min-max) of the csv files that run.sh collects, as the tables of docs/measurements.md."""
import collections
import statistics
import sys
from pathlib import Path

out = Path(sys.argv[1])


def rows(name):
    path = out / name
    return [line.strip().split(",") for line in path.read_text().splitlines() if line.strip()] if path.exists() else []


def cell(values):
    return f"{statistics.median(values):.1f} ({min(values):.1f}-{max(values):.1f})"


def table(title, header, keyed, columns, fmt=cell):
    print(f"\n{title}")
    print("| " + " | ".join(header) + " |")
    print("|" + "---|" * len(header))
    for key in sorted(keyed):
        print("| " + " | ".join(key) + " | " + " | ".join(fmt(keyed[key][c]) if keyed[key][c] else "-" for c in columns) + " |")


sizes = {}
for row in rows("signal.csv"):
    if row[0] == "size":
        sizes[row[1]] = row[2]
print("\nsizeof of a container-shaped class (ten signals, one QHash member):")
for design in ("qvariant", "rqt", "rqt-static"):
    print(f"  {design}: {sizes.get(design, '?')} bytes")

sig = collections.defaultdict(lambda: collections.defaultdict(list))
for row in rows("signal.csv"):
    if row[0] == "signal":
        sig[(row[2],)][row[1]].append(float(row[3]))
table("signal only, ns per emit with one slot, median (min-max)", ["key", "qvariant", "rqt", "rqt-static"], sig,
      ["qvariant", "rqt", "rqt-static"])

for name, title in (("e2e.csv", "end to end at 1000 entries, ns per change, median (min-max)"),
                    ("scale.csv", "end to end by size, ns per change, median (min-max)")):
    data = collections.defaultdict(lambda: collections.defaultdict(list))
    for row in rows(name):
        if row[0] == "e2e":
            data[(row[2], row[3], row[4])][row[1]].append(float(row[5]))
    if data:
        print(f"\n{title}")
        print("| container | entries | change | qvariant | rqt | rqt / qvariant |")
        print("|---|---|---|---|---|---|")
        for key in sorted(data, key=lambda k: (k[0], int(k[1]), k[2])):
            q, r = data[key]["qvariant"], data[key]["rqt"]
            print(f"| {key[0]} | {key[1]} | {key[2]} | {cell(q)} | {cell(r)} | {statistics.median(r) / statistics.median(q):.2f} |")

print("\ncompile and binary size:")
for kind, name, value in rows("compile.csv"):
    print(f"  {kind} {name}: {float(value):.2f}{' s' if kind == 'compile' else ' bytes'}")
