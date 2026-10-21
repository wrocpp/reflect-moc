#!/bin/bash
# The cost of one change of a keyed matrix: observable::ObservableMatrix against the matrix of the spec. Run in the image
# from the repository root:
#
#   docker run --rm -e RQT_RUNS=5 -v "$PWD":/src:ro -w /src reflect-moc/gcc16-qt610 bash tests/qt/measure/matrix_bench/run.sh
#
# The benchmark runs RQT_RUNS times (default 5) at each size, alternating nothing (each run measures both implementations
# in turn); the table is the median, with the minimum and maximum, in nanoseconds per change. The load average is printed
# before and after.
set -eu
export LC_ALL=C.UTF-8 QT_QPA_PLATFORM=offscreen
Q=${QTDIR:-/opt/qt}
RUNS=${RQT_RUNS:-5}
R=examples/qt/spec_regen/binding_b
O=${RQT_BENCH_OUT:-/tmp/rqt-matrix-bench}
rm -rf "$O" && mkdir -p "$O"
INC="-isystem $Q/include -isystem $Q/include/QtCore"
LIBS="-L$Q/lib -Wl,-rpath,$Q/lib -lQt6Core"
load() { cut -d' ' -f1 /proc/loadavg; }

echo "# $(g++ -dumpfullversion), Qt 6.10, $(uname -m), $(nproc) cores, runs $RUNS, load $(load)"
g++ -std=c++26 -freflection -O2 -DNDEBUG -DQT_NO_DEBUG -Iinclude -Iexamples/qt/observable_models -I$R/include $INC \
  tests/qt/measure/matrix_bench/bench.cpp $LIBS -o "$O/bench"
for rows in 10000 100000; do
  for run in $(seq 1 "$RUNS"); do "$O/bench" $rows; done
done > "$O/raw.txt"
python3 - "$O/raw.txt" <<'PY'
import statistics, sys
rows = {}
for line in open(sys.argv[1]):
    impl, kind, op, n, ns = line.split()
    rows.setdefault((int(n), kind, op), {}).setdefault(impl, []).append(float(ns))
print(f"{'rows':>7} {'':6} {'operation':10} {'spec':>26} {'observable':>26} {'ratio':>7}")
for (n, kind, op), by in sorted(rows.items()):
    cell = lambda v: f"{statistics.median(v):9.0f} ({min(v):.0f}-{max(v):.0f})"
    ratio = statistics.median(by['spec']) / statistics.median(by['observable'])
    print(f"{n:>7} {kind:6} {op:10} {cell(by['spec']):>26} {cell(by['observable']):>26} {ratio:6.1f}x")
PY
echo "# load after $(load)"
