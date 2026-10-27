#!/usr/bin/env python3
"""usage: gen_json.py <shape> <n>   prints a plugin JSON of that shape with n repeats (docs/notes/plugin-metadata.md)."""
import json
import sys

shape, n = sys.argv[1], int(sys.argv[2])
entry = lambda i: {"id": f"feature.{i}", "title": f"Feature number {i}", "enabled": i % 2 == 0, "weight": i + 0.5, "count": i,
                   "tags": ["a", "b", f"c{i}"], "extra": {"s": f"v{i}", "nested": [1, 2, 3]}}
compact = (",", ":")
if shape == "indented":      # features, one space per level, as a hand-written description looks
    print(json.dumps({"Name": "demo", "Keys": ["alpha", "beta"], "Features": [entry(i) for i in range(n)]}, indent=1))
elif shape == "compact":     # the same with no white space
    print(json.dumps({"Name": "demo", "Features": [entry(i) for i in range(n)]}, separators=compact))
elif shape == "numbers":     # fractions: three or four bytes of text, nine of CBOR
    print(json.dumps({"v": [i + 0.5 for i in range(n)]}, separators=compact))
elif shape == "strings":
    print(json.dumps({"v": [f"some text number {i} with words" for i in range(n)]}, separators=compact))
elif shape == "flatmap":     # one object, n keys, not in order: the sort
    print(json.dumps({f"key{(i * 7919) % n:06d}": i for i in range(n)}, separators=compact))
elif shape == "deep":        # 100 levels of one-member objects around n strings
    text = json.dumps([f"some text number {i} with words" for i in range(n)], separators=compact)
    for _ in range(100):
        text = '{"a":' + text + "}"
    print(text)
else:
    sys.exit("unknown shape")
