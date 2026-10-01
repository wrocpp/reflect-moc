#!/bin/sh
# Build and run the reflect-moc demo in the Linux image (GCC 16.2 + Qt 6.10).
#   examples/demo/run.sh           interactive: type `help`
#   examples/demo/run.sh --tour    scripted tour
# The binary is cached in build/demo; delete it to rebuild.
set -eu
cd "$(dirname "$0")/../.."
[ -t 0 ] && TTY=-it || TTY=-i
exec docker run --rm $TTY -e LC_ALL=C.UTF-8 -v "$PWD":/src -w /src reflect-moc/gcc16-qt610 sh -c '
  QT=/opt/qt
  OUT=build/demo
  mkdir -p "$OUT"
  if [ ! -x "$OUT/rqt_demo" ] || [ examples/demo/demo.cpp -nt "$OUT/rqt_demo" ]; then
    echo "building the demo (about a minute)..." >&2
    g++ -std=c++26 -freflection -Wall -Wextra -fPIC -Iinclude -I$QT/include -I$QT/include/QtCore \
        examples/demo/demo.cpp -L$QT/lib -Wl,-rpath,$QT/lib -lQt6Core -o "$OUT/rqt_demo"
  fi
  exec "$OUT/rqt_demo" "$@"
' sh "$@"
