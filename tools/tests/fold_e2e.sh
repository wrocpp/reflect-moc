#!/bin/sh
# The fold step, end to end: rqt-migrate ports a copy of the moc design (examples/qt/observable_models_qvariant),
# the result is built WITHOUT moc against reflect-moc and driven by the same seeded random driver, view mirror and
# QAbstractItemModelTester that examples/qt/observable_models passes (tests/qt/measure/observable_bench/gate.cpp).
# A control with one wrong row must be caught. Runs in the image, from the repository root:
#
#   docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 tools/tests/fold_e2e.sh [qtlike|annotations]
set -eu
export LC_ALL=C.UTF-8 QT_QPA_PLATFORM=offscreen
style="${1:-qtlike}"
Q="${QTDIR:-/opt/qt}"
out="${RQT_FOLD_OUT:-/tmp/rqt-fold-e2e}-$style"
twin=examples/qt/observable_models_qvariant
gate=tests/qt/measure/observable_bench/gate.cpp
rm -rf "$out" && mkdir -p "$out"

tools/rqt-migrate "$twin" --output "$out/ported" --reflect-moc-include include --style "$style" --macros rqt \
  --diff "$out/fold.diff" --report "$out/report.md"

inc="-isystem $Q/include -isystem $Q/include/QtCore -isystem $Q/include/QtGui -isystem $Q/include/QtWidgets -isystem $Q/include/QtQml -isystem $Q/include/QtTest"
libs="-L$Q/lib -Wl,-rpath,$Q/lib -lQt6Test -lQt6Qml -lQt6Gui -lQt6Core"
cxx="g++ -std=c++26 -freflection -fPIC -Wall -Wextra -O1 -Iinclude $inc -Iexamples/qt/observable_models -Itests/qt"

build() {  # build <tree> <binary> [flags]: no moc anywhere on this command line
  tree="$1"; bin="$2"; shift 2
  $cxx "$@" -I"$tree" "$gate" $libs -o "$bin"
}

build "$out/ported" "$out/gate" -Werror
"$out/gate" 2> /dev/null | tail -8
echo "gate: the migrated twin passes the random driver ($style)"

rm -rf "$out/flawed" && cp -r "$out/ported" "$out/flawed"
sed -i 's/m_pendingRow = static_cast<int>(rowOf(key));/m_pendingRow = 0;/' "$out/flawed/container_model.h"
cmp -s "$out/ported/container_model.h" "$out/flawed/container_model.h" && { echo "control not applied"; exit 1; }
build "$out/flawed" "$out/gate_flawed" 2> /dev/null
if "$out/gate_flawed" > /dev/null 2>&1; then echo "control FAILED: the flawed migrated twin passed"; exit 1; fi
echo "control: a flawed migrated twin (removal announced for row 0) is caught"
