#!/bin/sh
# Build and run spike 07 inside the reflect-moc/gcc16-qt610 image:
#   docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 sh spikes/07-deducing-this/build.sh
set -u
export LC_ALL=C.UTF-8
cd "$(dirname "$0")"
QT="${QTDIR:-/opt/qt}"
FLAGS="-std=c++26 -freflection -Wall -Wextra -fPIC -I$QT/include -I$QT/include/QtCore -I$QT/include/QtQml"
LIBS="-L$QT/lib -Wl,-rpath,$QT/lib -lQt6Qml -lQt6Core"
OUT=/tmp/rqt
mkdir -p "$OUT"
status=0

echo "=== binding.cpp (non-Qt, from team-lead research)"
if g++ -std=c++26 -freflection -Wall -Wextra binding.cpp -o "$OUT/binding" 2> "$OUT/binding.err"; then
  "$OUT/binding"; echo "exit: $?"
else
  echo "BUILD FAILED"; grep error "$OUT/binding.err" | head; status=1
fi

echo "=== qt_binding.cpp"
if g++ $FLAGS qt_binding.cpp $LIBS -o "$OUT/qt_binding" 2> "$OUT/qt_binding.err"; then
  grep -c warning "$OUT/qt_binding.err" | sed 's/^/warnings: /'
  grep warning "$OUT/qt_binding.err" | sort | uniq -c | head
  "$OUT/qt_binding"; rc=$?
  echo "exit: $rc"; [ $rc -eq 0 ] || status=1
else
  echo "BUILD FAILED"; grep -E 'error' "$OUT/qt_binding.err" | head -30; status=1
fi

for p in SPIKE_QOBJECT_CAST_WORKER SPIKE_PMF_CONNECT_WORKER SPIKE_INLINE_SMO SPIKE_INDEX_CONNECT_MISMATCH; do
  echo "--- probe -D$p"
  if g++ $FLAGS -D$p qt_binding.cpp $LIBS -o "$OUT/qt_binding-$p" 2> "$OUT/$p.err"; then
    echo "probe BUILT"; "$OUT/qt_binding-$p" > /dev/null; echo "probe exit: $?"
  else
    grep -E 'error' "$OUT/$p.err" | head -5
  fi
done
exit $status
