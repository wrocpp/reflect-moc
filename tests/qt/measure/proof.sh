#!/bin/bash
# The proof for the owner-anchor signals, run in the image from the repo root:
#   docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 bash tests/qt/measure/proof.sh
# 1. a header class in two translation units, -Werror and -flto -Wodr
# 2. sizeof and an emit micro-benchmark against the previous (lambda tag) design
# 3. the sliders port compiles with its unchanged source
Q=/opt/qt
INC="-isystem $Q/include -isystem $Q/include/QtCore -isystem $Q/include/QtGui -isystem $Q/include/QtWidgets -isystem $Q/include/QtQml"
LIBS="-L$Q/lib -Wl,-rpath,$Q/lib -lQt6Qml -lQt6Widgets -lQt6Gui -lQt6Core"
BASE="-std=c++26 -freflection -fPIC -Wall -Wextra"
OUT=/tmp/proof
mkdir -p "$OUT"
export QT_QPA_PLATFORM=offscreen

echo "=== 1. two translation units, -Werror, no -Wno-subobject-linkage, -flto -Wodr"
g++ $BASE -Werror -Wodr -O2 -flto -Iinclude -Itests/qt $INC \
  tests/qt/capability_data_signals_two_tus_a.cpp tests/qt/capability_data_signals_two_tus_b.cpp $LIBS \
  -o "$OUT/two_tus_lto" 2> "$OUT/lto.err"
echo "compile_rc=$? warnings=$(grep -c 'warning' "$OUT/lto.err")"
head -5 "$OUT/lto.err" | cut -c1-250
"$OUT/two_tus_lto"; echo "run_rc=$?"

echo "=== 2. benchmark: owner anchor (now) vs lambda tag (the commit that added capability_data_signals_two_tus)"
rm -rf "$OUT/old" && mkdir -p "$OUT/old"
git archive "$(git log -n1 --format=%H --grep='data-member signals of a header class in two translation units')" include | tar -x -C "$OUT/old"
g++ $BASE -O2 -Iinclude $INC tests/qt/measure/signal_bench.cpp $LIBS -o "$OUT/bench_new" 2> "$OUT/bn.err"; echo "new compile_rc=$?"
g++ $BASE -O2 -I"$OUT/old/include" $INC tests/qt/measure/signal_bench.cpp $LIBS -o "$OUT/bench_old" 2> "$OUT/bo.err"; echo "old compile_rc=$?"
for i in 1 2 3; do echo -n "new: "; "$OUT/bench_new"; echo -n "old: "; "$OUT/bench_old"; done

echo "=== 3. sliders"
ls examples/qt 2>&1 | head
find examples/qt -ipath '*slider*' -name '*.cpp' | head -10
