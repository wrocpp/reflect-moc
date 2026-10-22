#!/bin/bash
# CRAP of examples/qt/observable_models (CRAP = CC^2 * (1 - coverage)^3 + CC, limit 8), from lizard complexity and the coverage of the
# tests of this repository, with the crap.py of the cpp-quality-gates plugin (wrocpp/claude-marketplace, cloned read-only into /tmp, not
# modified). Run it in the image from the repository root; the repository is only read (the build is in /tmp) unless the result is
# written into it:
#
#   docker run --rm -v "$PWD":/src -w /src -e RQT_CRAP_OUT=docs/crap-observable-models.md reflect-moc/gcc16-qt610 \
#     bash scripts/crap_observable_models.sh
#
# 1. a venv with lizard and gcovr, and a clone of the plugin
# 2. the whole Qt test suite built with --coverage (GCC, -O0) in /tmp, and run
# 3. gcovr over examples/qt/observable_models, then crap.py over every function there
# 4. the table, written as markdown to $RQT_CRAP_OUT (default /tmp/crap-observable-models.md), with the proposed splits of
#    scripts/crap_splits_observable_models.txt next to the functions above the limit
set -eu
export LC_ALL=C.UTF-8 QT_QPA_PLATFORM=offscreen
ROOT=examples/qt/observable_models
LIMIT=8
W=${RQT_CRAP_WORK:-/tmp/rqt-crap}
OUT=${RQT_CRAP_OUT:-/tmp/crap-observable-models.md}
SPLITS=scripts/crap_splits_observable_models.txt
PLUGIN_URL=https://github.com/wrocpp/claude-marketplace
SOURCE=$(pwd -P)
mkdir -p "$W"

if [ ! -x "$W/venv/bin/lizard" ]; then
  python3 -m venv "$W/venv"
  "$W/venv/bin/pip" install -q lizard gcovr
fi
if [ ! -d "$W/market/.git" ]; then
  git clone -q --depth 1 "$PLUGIN_URL" "$W/market"
fi
PLUGIN=$W/market/plugins/cpp-quality-gates/scripts

if [ -z "${RQT_CRAP_SKIP_BUILD:-}" ]; then
  rm -rf "$W/build"
  cmake -S . -B "$W/build" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ -DRQT_BUILD_QT=ON \
    -DCMAKE_CXX_FLAGS="--coverage -O0" -DCMAKE_EXE_LINKER_FLAGS="--coverage" > "$W/configure.log" 2>&1
  cmake --build "$W/build" -j"$(nproc)" > "$W/build.log" 2>&1 || { tail -30 "$W/build.log"; exit 1; }
  find "$W/build" -name '*.gcda' -type f -delete
  # The compile-fail tests (limit_*, labelled qt-limit) rebuild the tree; they cover no run-time code.
  ctest --test-dir "$W/build" -j2 -LE qt-limit --output-on-failure > "$W/ctest.log" 2>&1 || { tail -40 "$W/ctest.log"; exit 1; }
  tail -3 "$W/ctest.log"
fi

"$W/venv/bin/gcovr" --gcov-executable gcov -r "$SOURCE" --filter "$ROOT/.*" --gcov-ignore-errors=no_working_dir_found \
  --json "$W/cov.json" "$W/build" > "$W/gcovr.log" 2>&1
"$W/venv/bin/python" "$PLUGIN/crap.py" "$W/cov.json" "$ROOT" --max "$LIMIT" --top 100000 > "$W/crap.txt" || true
tail -1 "$W/crap.txt"  # the plugin's own verdict; the table below is made from the same functions with unrounded scores

TOOLS="g++ $(g++ -dumpfullversion), gcov $(gcov --version | head -1 | sed 's/.* //'), lizard $("$W/venv/bin/lizard" --version), gcovr $("$W/venv/bin/gcovr" --version | head -1 | sed 's/gcovr //'), crap.py of $PLUGIN_URL at $(git -C "$W/market" rev-parse --short HEAD)" \
DATE=$(date -u +%F) LIMIT=$LIMIT CTEST=$(grep -E "tests passed|tests failed" "$W/ctest.log" 2>/dev/null | tail -1 || true) \
"$W/venv/bin/python" - "$PLUGIN" "$W/cov.json" "$ROOT" "$SPLITS" "$OUT" <<'PY'
import json, os, pathlib, sys
plugin, cov_json, root, splits_file, out = sys.argv[1:6]
sys.path.insert(0, plugin)
import crap, lizard  # the plugin's own scoring, not a copy of it
report = crap.coverage_by_line(json.loads(pathlib.Path(cov_json).read_text()))
rows = []
for path in crap.source_files([root], crap.DEFAULT_SUFFIXES):
    lines = report.get(os.path.normpath(path.as_posix()), {})
    for name, start, complexity, fraction, score in crap.score_file(lizard, path, lines):
        rows.append((score, complexity, round(fraction * 100), path.as_posix(), start, name))
limit = float(os.environ["LIMIT"])
proposals = {}
if os.path.exists(splits_file):
    for line in open(splits_file):
        line = line.strip()
        if line and not line.startswith("#"):
            key, _, text = line.partition("|")
            proposals[key.strip()] = text.strip()
rows.sort(key=lambda r: (-r[0], r[3], r[4]))
over = [r for r in rows if r[0] > limit]
short = lambda p: p.split("/")[-1]
with open(out, "w") as f:
    f.write("# CRAP of examples/qt/observable_models\n\n")
    f.write(f"CRAP = CC^2 * (1 - coverage)^3 + CC, limit {limit:g}; at full coverage the score is the cyclomatic complexity itself, so a function above the limit\n")
    f.write("at full coverage has to be split, and a function below it that is not covered is a missing test.\n\n")
    f.write(f"- Date: {os.environ['DATE']}\n- Tools: {os.environ['TOOLS']}\n")
    f.write(f"- Tests: the whole Qt suite of this repository built with `--coverage -O0` and run ({os.environ.get('CTEST') or 'see the log'}), compile-fail tests excluded\n")
    f.write("- Reproduce: `docker run --rm -v \"$PWD\":/src -w /src -e RQT_CRAP_OUT=docs/crap-observable-models.md reflect-moc/gcc16-qt610 bash scripts/crap_observable_models.sh`\n")
    f.write("- Coverage is line coverage of the function's own lines (gcovr), merged over every test executable; complexity is lizard's.\n\n")
    f.write(f"**{len(rows)} functions, {len(over)} above {limit:g}.**\n\n")
    f.write("## Above the limit\n\n")
    if over:
        f.write("| header | function | line | CC | coverage | CRAP | status | proposed split |\n|---|---|---|---|---|---|---|---|\n")
        for score, cc, cov, path, line, name in over:
            proposal = proposals.get(f"{short(path)}:{name}", "(none yet)")
            f.write(f"| {short(path)} | `{name}` | {line} | {cc} | {cov}% | {score:.1f} | open | {proposal} |\n")
    else:
        f.write("None.\n")
    near = [r for r in rows if limit - 1 < r[0] <= limit]
    f.write(f"\n## At the limit (above {limit - 1:g} and not above {limit:g}: one more branch puts them over)\n\n")
    for score, cc, cov, path, line, name in near:
        f.write(f"- {short(path)}:{line} `{name}`: CC {cc}, coverage {cov}%, CRAP {score:.1f}\n")
    f.write("\n## Every function\n\n| header | function | line | CC | coverage | CRAP |\n|---|---|---|---|---|---|\n")
    for score, cc, cov, path, line, name in rows:
        f.write(f"| {short(path)} | `{name}` | {line} | {cc} | {cov}% | {score:.1f} |\n")
print(f"wrote {out}: {len(rows)} functions, {len(over)} above {limit:g}")
PY
