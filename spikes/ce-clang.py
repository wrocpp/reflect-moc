#!/usr/bin/env python3
"""Compile and run a spike on Compiler Explorer's clang-p2996 for comparison.

Usage: spikes/ce-clang.py spikes/01-define-aggregate.cpp [-DMACRO ...]

GCC ships the reflection header as <meta>; clang-p2996 as <experimental/meta>,
so the include is rewritten before the source is sent.
"""
import json
import sys
import urllib.request

COMPILER = "clang_bb_p2996"
OPTIONS = "-std=c++26 -freflection-latest -stdlib=libc++"


def main() -> int:
    path, defines = sys.argv[1], sys.argv[2:]
    with open(path, encoding="utf-8") as f:
        source = f.read().replace("#include <meta>", "#include <experimental/meta>", 1)
    body = {
        "source": source,
        "options": {
            "userArguments": " ".join([OPTIONS, *defines]),
            "filters": {"execute": True},
            "compilerOptions": {"executorRequest": True},
        },
    }
    req = urllib.request.Request(
        f"https://godbolt.org/api/compiler/{COMPILER}/compile",
        data=json.dumps(body).encode(),
        headers={"Content-Type": "application/json", "Accept": "application/json"},
    )
    with urllib.request.urlopen(req, timeout=120) as resp:
        result = json.load(resp)
    build = result.get("buildResult", {})
    errors = [line["text"] for line in build.get("stderr", []) if "error" in line["text"]]
    print(f"clang-p2996 {path} {' '.join(defines)}")
    print(f"build code: {build.get('code')}")
    for line in errors[:12]:
        print("  " + line)
    if build.get("code") == 0:
        print(f"exit: {result.get('code')}")
        for line in result.get("stdout", []):
            print("  " + line["text"])
        for line in result.get("stderr", [])[:6]:
            print("  stderr: " + line["text"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
