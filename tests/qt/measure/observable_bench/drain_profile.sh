#!/bin/bash
# Profile the hash drain with perf and strace. Needs a privileged container and network (it installs linux-perf
# and strace from Debian), so it runs from the bench workflow rather than by hand:
#   docker run --rm --privileged -v "$PWD":/src:ro -w /src reflect-moc/gcc16-qt610 bash tests/qt/measure/observable_bench/drain_profile.sh
set -u
export LC_ALL=C.UTF-8 QT_QPA_PLATFORM=offscreen DEBIAN_FRONTEND=noninteractive
Q=${QTDIR:-/opt/qt}
D=tests/qt/measure/observable_bench
QV=examples/qt/observable_models_qvariant
O=/tmp/rqt-drain-profile
rm -rf "$O" && mkdir -p "$O"
INC="-isystem $Q/include -isystem $Q/include/QtCore -isystem $Q/include/QtGui -isystem $Q/include/QtWidgets -isystem $Q/include/QtQml"
LIBS="-L$Q/lib -Wl,-rpath,$Q/lib -lQt6Qml -lQt6Gui -lQt6Core"
CXX="g++ -std=c++26 -fPIC -Wall -Wextra -O2 -g"
echo "# $(uname -m), $(nproc) cores, kernel $(uname -r)"
grep -m1 "model name" /proc/cpuinfo || true
grep -m1 "flags" /proc/cpuinfo | tr ' ' '\n' | grep -E "^(avx2|avx512f|erms|fsrm)$" | tr '\n' ' '; echo
(apt-get update -qq && apt-get install -y -qq linux-perf strace binutils) > "$O/apt.log" 2>&1 || { echo "apt failed:"; tail -5 "$O/apt.log"; }
which perf strace || true
echo -1 > /proc/sys/kernel/perf_event_paranoid 2>/dev/null || true
echo "perf_event_paranoid=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null)"

"$Q/libexec/moc" -I$QV $QV/container_signals.h -o "$O/moc_cs.cpp"
$CXX -freflection -Iinclude $INC -I$QV -Iexamples/qt/observable_models $D/e2e.cpp $LIBS -o "$O/rqt"
$CXX -DBENCH_QV $INC -I$QV -Iexamples/qt/observable_models $D/e2e.cpp "$O/moc_cs.cpp" $LIBS -o "$O/qv"

RUN="--size 100000 --only QHash"
echo "=== 1. timing, QHash 100000 (ns per change)"
for b in qv rqt qv rqt; do echo -n "$b "; "$O/$b" $RUN 2>/dev/null | grep -E "drain|replace" | cut -d, -f5,6 | tr '\n' ' '; echo; done

echo "=== 2. perf stat (whole run: fill, replace, drain)"
for b in qv rqt; do
  echo "--- $b"
  perf stat -e cycles,instructions,branches,branch-misses,L1-dcache-load-misses,cache-misses "$O/$b" $RUN 2>&1 >/dev/null | grep -E "cycles|instructions|branch|cache|elapsed|not supported|not counted|Permission|Error" | sed 's/^ *//'
done

echo "=== 3. where the cycles go (perf record + report, top symbols)"
for b in qv rqt; do
  echo "--- $b"
  perf record -q -F 4000 -o "$O/$b.data" "$O/$b" $RUN > /dev/null 2>&1 || echo "perf record failed"
  perf report -i "$O/$b.data" --stdio --no-children --percent-limit 2 2>/dev/null | grep -v "^#" | grep -v "^$" | head -8 | cut -c1-170
done

echo "=== 4. the hot loop, annotated (cycles per instruction of the scan in the removal slot)"
for b in qv rqt; do
  echo "--- $b"
  perf annotate -i "$O/$b.data" --stdio -l --symbol="$(perf report -i "$O/$b.data" --stdio --no-children 2>/dev/null | grep -v '^#' | grep -m1 'lambda' | sed 's/.*\] //' | cut -c1-200)" 2>/dev/null | grep -E "^ +[0-9]+\.[0-9]+" | sort -rn | head -10 | cut -c1-140
done

echo "=== 5. system calls (strace -c): is the time in the kernel?"
for b in qv rqt; do
  echo "--- $b"
  strace -c -f -o "$O/$b.strace" "$O/$b" $RUN > /dev/null 2>&1; head -8 "$O/$b.strace"
done
echo "=== 6. time split (user vs system), three runs each"
for b in qv rqt; do for i in 1 2 3; do echo -n "$b "; ( TIMEFORMAT='real %R user %U sys %S'; time "$O/$b" $RUN > /dev/null 2>&1 ) 2>&1; done; done
