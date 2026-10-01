#!/bin/sh
# Build and run the Qt spikes inside the reflect-moc/gcc16-qt610 image:
#   docker run --rm -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 spikes/06-qt/build.sh [probe-macro]
set -u
export LC_ALL=C.UTF-8
cd "$(dirname "$0")"
QT="${QTDIR:-/opt/qt}"
FLAGS="-std=c++26 -freflection -Wall -Wextra -fPIC -I$QT/include -I$QT/include/QtCore -I$QT/include/QtQml"
LIBS="-L$QT/lib -Wl,-rpath,$QT/lib"
OUT=/tmp/rqt
mkdir -p "$OUT"
status=0

echo "=== moc output shape (moc_probe.h)"
moc moc_probe.h -o "$OUT/moc_probe.cpp" && grep -c QtMocHelpers "$OUT/moc_probe.cpp" | sed 's/^/QtMocHelpers uses: /'

for spike in handwritten reflected; do
  echo "=== $spike.cpp"
  if g++ $FLAGS ${1:+-D$1} "$spike.cpp" $LIBS -lQt6Qml -lQt6Core -o "$OUT/$spike" 2> "$OUT/$spike.err"; then
    grep -c warning "$OUT/$spike.err" | sed 's/^/warnings: /'
    "$OUT/$spike"; rc=$?
    echo "exit: $rc"; [ $rc -eq 0 ] || status=1
  else
    echo "BUILD FAILED"; grep -E 'error' "$OUT/$spike.err" | head -30; status=1
  fi
done
exit $status
