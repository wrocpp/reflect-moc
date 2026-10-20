#!/bin/bash
# The QVariant split against the typed template, measured. Run in the image from the repository root:
#
#   docker run --rm -e RQT_RUNS=7 -v "$PWD":/src:ro -w /src reflect-moc/gcc16-qt610 bash tests/qt/measure/observable_bench/run.sh
#
# 1. gate: the QVariant twin passes the random driver, and a copy with one wrong row is caught
# 2. sizeof, and the signal-only cost for three key types
# 3. end to end: a model connected to a container, per change, at one size and then 100 to 100 000 entries
# 4. compile time of the benchmark translation unit, and binary size, each way
#
# Every timing is repeated RQT_RUNS times (default 7), alternating the two designs, and printed as
# median (min-max). The load average is printed before and after: read nothing under about 2 ns.
set -eu
export LC_ALL=C.UTF-8 QT_QPA_PLATFORM=offscreen
Q=${QTDIR:-/opt/qt}
RUNS=${RQT_RUNS:-7}
D=tests/qt/measure/observable_bench
QV=examples/qt/observable_models_qvariant
O=${RQT_BENCH_OUT:-/tmp/rqt-observable-bench}
rm -rf "$O" && mkdir -p "$O"
INC="-isystem $Q/include -isystem $Q/include/QtCore -isystem $Q/include/QtGui -isystem $Q/include/QtWidgets -isystem $Q/include/QtQml -isystem $Q/include/QtTest"
LIBS="-L$Q/lib -Wl,-rpath,$Q/lib -lQt6Test -lQt6Qml -lQt6Gui -lQt6Core"
PROJ="-Iexamples/qt/observable_models -Itests/qt"
CXX="g++ -std=c++26 -fPIC -Wall -Wextra"
load() { cut -d' ' -f1 /proc/loadavg; }

echo "# $(g++ -dumpfullversion), Qt $($Q/bin/qmake -query QT_VERSION 2>/dev/null || echo 6.10), $(uname -m), $(nproc) cores, runs $RUNS, load $(load)"

"$Q/libexec/moc" -I$QV $QV/container_signals.h -o "$O/moc_cs.cpp"
"$Q/libexec/moc" -I$D $D/signal_qv.h -o "$O/moc_sq.cpp"

echo "=== 1. gate"
$CXX -O1 -Werror $INC -I$QV $PROJ -I"$O" $D/gate.cpp "$O/moc_cs.cpp" $LIBS -o "$O/gate" 2> "$O/gate.err" || { cat "$O/gate.err"; exit 1; }
"$O/gate" 2> /dev/null | tail -8
# Control: a model that announces every removal for row 0 must be caught by the same driver.
rm -rf "$O/flawed" && mkdir -p "$O/flawed" && cp $QV/*.h "$O/flawed/"
sed -i 's/m_pendingRow = static_cast<int>(rowOf(key));/m_pendingRow = 0;/' "$O/flawed/container_model.h"
cmp -s $QV/container_model.h "$O/flawed/container_model.h" && { echo "control not applied"; exit 1; }
"$Q/libexec/moc" -I"$O/flawed" "$O/flawed/container_signals.h" -o "$O/flawed/moc_cs.cpp"
$CXX -O1 $INC -I"$O/flawed" $PROJ -I"$O" $D/gate.cpp "$O/flawed/moc_cs.cpp" $LIBS -o "$O/gate_flawed" 2> /dev/null
if "$O/gate_flawed" > /dev/null 2>&1; then echo "control FAILED: the flawed twin passed the gate"; exit 1; else echo "control: a flawed twin (removal announced for row 0) is caught"; fi

echo "=== build"
$CXX -freflection -O2 -Iinclude $INC -I$QV $PROJ $D/e2e.cpp $LIBS -o "$O/e2e_rqt"
$CXX -O2 -DBENCH_QV $INC -I$QV $PROJ $D/e2e.cpp "$O/moc_cs.cpp" $LIBS -o "$O/e2e_qv"
$CXX -freflection -O2 -Iinclude -I$D $INC $D/signal.cpp "$O/moc_sq.cpp" $LIBS -o "$O/signal"

collect() {  # collect <file> <binary> [args]
  local out=$1; shift
  "$@" 2> /dev/null >> "$out"
}

echo "=== 2. signal only and sizeof"
for r in $(seq "$RUNS"); do collect "$O/signal.csv" "$O/signal"; done
echo "=== 3. end to end"
for r in $(seq "$RUNS"); do collect "$O/e2e.csv" "$O/e2e_qv"; collect "$O/e2e.csv" "$O/e2e_rqt"; done
for r in 1 2 3; do collect "$O/scale.csv" "$O/e2e_qv" --scale; collect "$O/scale.csv" "$O/e2e_rqt" --scale; done

echo "=== 4. compile time of the benchmark translation unit (-O2, -c, median of 3)"
ctime() { python3 -c 'import subprocess,sys,time,statistics
ts=[]
for _ in range(3):
    s=time.perf_counter(); subprocess.run(sys.argv[1:],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True); ts.append(time.perf_counter()-s)
print(statistics.median(ts))' "$@"; }
t_rqt=$(ctime $CXX -freflection -O2 -c -Iinclude $INC -I$QV $PROJ $D/e2e.cpp -o "$O/c1.o")
t_moc=$(ctime "$Q/libexec/moc" -I$QV $QV/container_signals.h -o "$O/c2.cpp")
t_mocc=$(ctime $CXX -O2 -c $INC -I$QV "$O/moc_cs.cpp" -o "$O/c3.o")
t_qv=$(ctime $CXX -O2 -DBENCH_QV -c $INC -I$QV $PROJ $D/e2e.cpp -o "$O/c4.o")
printf 'compile,rqt,%s\ncompile,qvariant,%s\ncompile,qvariant-moc-and-its-output,%s\n' "$t_rqt" "$t_qv" "$(awk "BEGIN{print $t_moc + $t_mocc}")" >> "$O/compile.csv"
printf 'binary,rqt,%s\nbinary,qvariant,%s\n' "$(stat -c %s "$O/e2e_rqt")" "$(stat -c %s "$O/e2e_qv")" >> "$O/compile.csv"

echo "# load at end $(load)"
python3 $D/summarize.py "$O"
