#!/bin/bash
# Binding A (moc, QVariant keys) against binding B (typed signals) of the observable container spec, measured.
# Run in the image from the repository root:
#
#   docker run --rm -e RQT_RUNS=7 -v "$PWD":/src:ro -w /src reflect-moc/gcc16-qt610 bash tests/qt/measure/spec_bench/run.sh
#
# 1. gate: each binding passes its 17 acceptance entries, and a copy whose list model removes runs in
#    ascending order (instead of descending) fails tst_list_model
# 2. the benchmark, RQT_RUNS times (default 7), alternating the two bindings
# 3. compile time of the benchmark translation unit and binary size, each way
#
# Printed as median (min-max); the load average is printed before and after.
set -eu
export LC_ALL=C.UTF-8 QT_QPA_PLATFORM=offscreen
Q=${QTDIR:-/opt/qt}
RUNS=${RQT_RUNS:-7}
R=examples/qt/spec_regen
D=tests/qt/measure/spec_bench
O=${RQT_BENCH_OUT:-/tmp/rqt-spec-bench}
rm -rf "$O" && mkdir -p "$O"
INC="-isystem $Q/include -isystem $Q/include/QtCore -isystem $Q/include/QtTest"
LIBS="-L$Q/lib -Wl,-rpath,$Q/lib -lQt6Core"
load() { cut -d' ' -f1 /proc/loadavg; }

echo "# $(g++ -dumpfullversion), Qt $($Q/bin/qmake -query QT_VERSION 2>/dev/null || echo 6.10), $(uname -m), $(nproc) cores, runs $RUNS, load $(load)"

gate() {  # gate <binding dir> <label>: configure, build, run all tests
  cmake -S "$1" -B "$O/gate_$2" -G Ninja -DCMAKE_BUILD_TYPE=Debug > /dev/null
  cmake --build "$O/gate_$2" > "$O/gate_$2.log" 2>&1 || { tail -20 "$O/gate_$2.log"; exit 1; }
  ctest --test-dir "$O/gate_$2" 2>&1 | grep -E "tests passed|tests failed" | sed "s/^/$2: /"
}
echo "=== 1. gate"
gate $R/binding_a a
gate $R/binding_b b
for b in a b; do
  rm -rf "$O/flawed_$b" && cp -R $R/binding_$b "$O/flawed_$b"
  sed -i 's/for (auto run = runs.rbegin(); run != runs.rend(); ++run)/for (auto run = runs.begin(); run != runs.end(); ++run)/' "$O/flawed_$b/include/obs/container_list_model.h"
  cmp -s $R/binding_$b/include/obs/container_list_model.h "$O/flawed_$b/include/obs/container_list_model.h" && { echo "control not applied to $b"; exit 1; }
  cmake -S "$O/flawed_$b" -B "$O/flawed_${b}_build" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DREFLECT_MOC_INCLUDE_DIR="$PWD/include" > /dev/null 2>&1
  cmake --build "$O/flawed_${b}_build" --target tst_list_model > /dev/null 2>&1
  if "$O/flawed_${b}_build/tst_list_model" > /dev/null 2>&1; then echo "control FAILED: the flawed binding $b passed"; exit 1; fi
  echo "control: binding $b with runs removed in ascending order is caught by tst_list_model"
done

echo "=== build"
A_INC="-I$R/binding_a/include"
B_INC="-I$R/binding_b/include -Iinclude"
"$Q/libexec/moc" $A_INC $R/binding_a/include/obs/container_signals.h -o "$O/moc_cs.cpp"
"$Q/libexec/moc" $A_INC $R/binding_a/include/obs/matrix_signals.h -o "$O/moc_ms.cpp"
CXXA="g++ -std=c++20 -fPIC -O2 -Wall -Wextra -DBENCH_BINDING_A"
CXXB="g++ -std=c++26 -freflection -fPIC -O2 -Wall -Wextra"
$CXXA $INC $A_INC $D/bench.cpp "$O/moc_cs.cpp" "$O/moc_ms.cpp" $LIBS -o "$O/bench_a"
$CXXB $INC $B_INC $D/bench.cpp $LIBS -o "$O/bench_b"

echo "=== 2. benchmark"
for r in $(seq "$RUNS"); do "$O/bench_a" 2> /dev/null >> "$O/bench.csv"; "$O/bench_b" 2> /dev/null >> "$O/bench.csv"; done

echo "=== 3. compile time of the benchmark translation unit (-O2, -c, median of 3)"
ctime() { python3 -c 'import subprocess,sys,time,statistics
ts=[]
for _ in range(3):
    s=time.perf_counter(); subprocess.run(sys.argv[1:],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True); ts.append(time.perf_counter()-s)
print(statistics.median(ts))' "$@"; }
t_a=$(ctime $CXXA -c $INC $A_INC $D/bench.cpp -o "$O/c1.o")
t_moc=$(ctime "$Q/libexec/moc" $A_INC $R/binding_a/include/obs/container_signals.h -o "$O/c2.cpp")
t_mocm=$(ctime "$Q/libexec/moc" $A_INC $R/binding_a/include/obs/matrix_signals.h -o "$O/c2m.cpp")
t_mocc=$(ctime $CXXA -c $INC $A_INC "$O/moc_cs.cpp" -o "$O/c3.o")
t_moccm=$(ctime $CXXA -c $INC $A_INC "$O/moc_ms.cpp" -o "$O/c3m.o")
t_b=$(ctime $CXXB -c $INC $B_INC $D/bench.cpp -o "$O/c4.o")
printf 'compile,b,%s\ncompile,a,%s\ncompile,a-moc-and-its-output,%s\n' "$t_b" "$t_a" \
  "$(awk "BEGIN{print $t_moc + $t_mocm + $t_mocc + $t_moccm}")" >> "$O/compile.csv"
printf 'binary,b,%s\nbinary,a,%s\n' "$(stat -c %s "$O/bench_b")" "$(stat -c %s "$O/bench_a")" >> "$O/compile.csv"

echo "# load at end $(load)"
python3 $D/summarize.py "$O"
