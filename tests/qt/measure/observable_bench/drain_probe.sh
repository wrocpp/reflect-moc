#!/bin/bash
# The hash-drain probe (drain_probe.cpp), and the end-to-end drain in isolation, in the image:
#   docker run --rm -v "$PWD":/src:ro -w /src reflect-moc/gcc16-qt610 bash tests/qt/measure/observable_bench/drain_probe.sh
set -eu
export LC_ALL=C.UTF-8 QT_QPA_PLATFORM=offscreen
Q=${QTDIR:-/opt/qt}
D=tests/qt/measure/observable_bench
QV=examples/qt/observable_models_qvariant
O=${RQT_BENCH_OUT:-/tmp/rqt-drain-probe}
rm -rf "$O" && mkdir -p "$O"
INC="-isystem $Q/include -isystem $Q/include/QtCore -isystem $Q/include/QtGui -isystem $Q/include/QtWidgets -isystem $Q/include/QtQml"
LIBS="-L$Q/lib -Wl,-rpath,$Q/lib -lQt6Qml -lQt6Gui -lQt6Core"
CXX="g++ -std=c++26 -fPIC -Wall -Wextra -O2"
echo "# $(uname -m), $(nproc) cores, load $(cut -d' ' -f1 /proc/loadavg)"
grep -m1 "model name" /proc/cpuinfo || true
"$Q/libexec/moc" -I$QV $QV/container_signals.h -o "$O/moc_cs.cpp"
$CXX $INC $D/drain_probe.cpp $LIBS -o "$O/probe"
$CXX -freflection -Iinclude $INC -I$QV -Iexamples/qt/observable_models $D/e2e.cpp $LIBS -o "$O/e2e_rqt"
$CXX -DBENCH_QV $INC -I$QV -Iexamples/qt/observable_models $D/e2e.cpp "$O/moc_cs.cpp" $LIBS -o "$O/e2e_qv"

echo "=== A. the two steps with no signals, 100000 keys, list allocated at a different offset (pad bytes first)"
echo "probe,pad,scan_ns,remove_ns,both_ns,vector_scan_ns,vector_remove_ns,vector_both_ns"
for pad in 0 4 8 12 16 24 32 48 64 96 128 4096; do "$O/probe" 100000 $pad; done
echo "=== B. the same with 10000 keys"
for pad in 0 4 8 16 32 64; do "$O/probe" 10000 $pad; done
echo "=== C. end to end, QHash only, 100000 entries, five alternating runs (drain and replace, ns per change)"
for i in 1 2 3 4 5; do for b in qv rqt; do echo -n "$b "; "$O/e2e_$b" --size 100000 --only QHash 2>/dev/null | grep -E "drain|replace" | cut -d, -f5,6 | tr '\n' ' '; echo; done; done
echo "=== D. the full run order (QList then QHash then QMap), QHash 100000, three runs"
for i in 1 2 3; do for b in qv rqt; do echo -n "$b "; "$O/e2e_$b" --scale 2>/dev/null | grep -E "QHash,100000,drain" | cut -d, -f2,6 | tr '\n' ' '; echo; done; done

echo "=== E. assembly of the model's removal and replace slots, both builds (saved as .s files)"
mkdir -p "$O/asm"
g++ -std=c++26 -fPIC -O2 -S -freflection -Iinclude $INC -I$QV -Iexamples/qt/observable_models $D/e2e.cpp -o "$O/asm/rqt.s"
g++ -std=c++26 -fPIC -O2 -S -DBENCH_QV $INC -I$QV -Iexamples/qt/observable_models $D/e2e.cpp -o "$O/asm/qv.s"
g++ -std=c++26 -fPIC -O2 -S $INC $D/drain_probe.cpp -o "$O/asm/probe.s"
objdump -d --no-show-raw-insn -C "$O/e2e_rqt" > "$O/asm/rqt.dump"
objdump -d --no-show-raw-insn -C "$O/e2e_qv" > "$O/asm/qv.dump"
if [ -n "${RQT_ASM_OUT:-}" ]; then cp -r "$O/asm/." "$RQT_ASM_OUT/"; fi

echo "=== F. the same loop, padded so that no jump crosses a 32-byte boundary (x86_64 only), and with loops aligned to 64"
if [ "$(uname -m)" = x86_64 ]; then
  for flags in "-Wa,-mbranches-within-32B-boundaries" "-falign-loops=64"; do
    echo "--- $flags"
    g++ -std=c++26 -fPIC -Wall -Wextra -O2 $flags -freflection -Iinclude $INC -I$QV -Iexamples/qt/observable_models $D/e2e.cpp $LIBS -o "$O/e2e_rqt_f"
    g++ -std=c++26 -fPIC -Wall -Wextra -O2 $flags -DBENCH_QV $INC -I$QV -Iexamples/qt/observable_models $D/e2e.cpp "$O/moc_cs.cpp" $LIBS -o "$O/e2e_qv_f"
    for i in 1 2 3; do for b in qv rqt; do echo -n "$b "; "$O/e2e_${b}_f" --size 100000 --only QHash 2>/dev/null | grep -E "drain|replace" | cut -d, -f5,6 | tr '\n' ' '; echo; done; done
  done
fi

echo "=== G. is the QList the cause? the model's key list as std::vector with std::find (both builds), QHash 100000"
for variant in rqt qv; do
  rm -rf "$O/sv_$variant" && mkdir -p "$O/sv_$variant"
done
python3 - "$O" <<'PY'
import sys, pathlib
o = pathlib.Path(sys.argv[1])
def patch(src, dst):
    t = pathlib.Path(src).read_text()
    t = t.replace("m_order = Traits::keys(source->items());", "{ auto k = Traits::keys(source->items()); m_order.assign(k.begin(), k.end()); }")
    t = t.replace("m_order.at(row)", "m_order[row]")
    t = t.replace("m_order.indexOf(key)", "(std::find(m_order.begin(), m_order.end(), key) - m_order.begin())")
    t = t.replace("m_order.append(key)", "m_order.push_back(key)")
    t = t.replace("m_order.removeAt(m_pendingRow)", "m_order.erase(m_order.begin() + m_pendingRow)")
    t = t.replace("QList<Key> m_order;", "std::vector<Key> m_order;")
    t = t.replace("#include <functional>", "#include <algorithm>\n#include <functional>\n#include <vector>")
    assert "std::vector<Key> m_order" in t and "std::find" in t
    pathlib.Path(dst).write_text(t)
patch("examples/qt/observable_models/container_model.hpp", o / "sv_rqt" / "container_model.hpp")
patch("examples/qt/observable_models_qvariant/container_model.h", o / "sv_qv" / "container_model.h")
PY
cp examples/qt/observable_models/observable_container.hpp "$O/sv_rqt/"
cp examples/qt/observable_models_qvariant/observable_container.h examples/qt/observable_models_qvariant/container_signals.h "$O/sv_qv/"
$CXX -freflection -Iinclude $INC -I"$O/sv_rqt" -Iexamples/qt/observable_models $D/e2e.cpp $LIBS -o "$O/e2e_rqt_vec"
$CXX -DBENCH_QV $INC -I"$O/sv_qv" -Iexamples/qt/observable_models $D/e2e.cpp "$O/moc_cs.cpp" $LIBS -o "$O/e2e_qv_vec"
for i in 1 2 3; do for b in qv rqt; do echo -n "$b+vector "; "$O/e2e_${b}_vec" --size 100000 --only QHash 2>/dev/null | grep -E "drain|replace" | cut -d, -f5,6 | tr '\n' ' '; echo; done; done
