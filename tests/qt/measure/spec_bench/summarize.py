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


sizes = {}
for row in rows("bench.csv"):
    if row[0] == "size":
        sizes[(row[1], row[2])] = row[3]
print("\nsizeof, bytes:")
for what in ("Container<QHash>", "Matrix"):
    print(f"  {what}: binding A {sizes.get(('a', what), '?')}, binding B {sizes.get(('b', what), '?')}")

data = collections.defaultdict(lambda: collections.defaultdict(list))
for row in rows("bench.csv"):
    if row[0] == "spec":
        data[(row[2], row[3], row[4])][row[1]].append(float(row[5]))
print("\nns per operation (per change for fill, replace and drain; per call for the others), median (min-max)")
print("| container | entries | operation | binding A | binding B | B / A |")
print("|---|---|---|---|---|---|")
for key in sorted(data, key=lambda k: (k[0], int(k[1]), k[2])):
    a, b = data[key]["a"], data[key]["b"]
    print(f"| {key[0]} | {key[1]} | {key[2]} | {cell(a)} | {cell(b)} | {statistics.median(b) / statistics.median(a):.2f} |")

print("\ncompile and binary size:")
for kind, name, value in rows("compile.csv"):
    print(f"  {kind} {name}: {float(value):.2f}{' s' if kind == 'compile' else ' bytes'}")
