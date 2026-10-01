#!/bin/sh
# Run the Qt Quick demo and view it in your browser.
#   examples/demo/run-qml.sh                interactive; open http://localhost:6080/vnc.html?autoconnect=1&resize=scale
#   examples/demo/run-qml.sh --screenshot   write PNGs to build/demo/shots and exit
# First run builds a small GUI image and the app (a few minutes). Ctrl-C stops it.
set -eu
cd "$(dirname "$0")/../.."
IMG=reflect-moc/gcc16-qt610-gui
docker image inspect "$IMG" >/dev/null 2>&1 || docker build -t "$IMG" -f docker/Dockerfile.gui docker
MODE=${1:-}
PORT=${RQT_PORT:-6080}
# the browser port is only needed for the interactive window, not for screenshots
if [ "$MODE" = "--screenshot" ]; then PORTARG=""; else PORTARG="-p $PORT:6080"; fi
exec docker run --rm -e LC_ALL=C.UTF-8 $PORTARG -v "$PWD":/src -w /src "$IMG" sh -c '
  QT=/opt/qt
  OUT=build/demo
  mkdir -p "$OUT"
  if [ ! -x "$OUT/rqt_demo_qml" ] || [ examples/demo/demo_qml.cpp -nt "$OUT/rqt_demo_qml" ]; then
    echo "building the QML demo (a few minutes)..." >&2
    g++ -std=c++26 -freflection -fPIC -Iinclude -I$QT/include -I$QT/include/QtCore -I$QT/include/QtGui \
        -I$QT/include/QtQml -I$QT/include/QtQuick -I$QT/include/QtQmlIntegration \
        examples/demo/demo_qml.cpp -L$QT/lib -Wl,-rpath,$QT/lib -lQt6Quick -lQt6Qml -lQt6Gui -lQt6Core -o "$OUT/rqt_demo_qml"
  fi
  export QT_QUICK_BACKEND=software QT_QUICK_CONTROLS_STYLE=Basic QML_IMPORT_PATH=$QT/qml QML2_IMPORT_PATH=$QT/qml QT_PLUGIN_PATH=$QT/plugins
  if [ "$1" = "--screenshot" ]; then
    QT_QPA_PLATFORM=offscreen exec "$OUT/rqt_demo_qml" --screenshot "$OUT/shots"
  fi
  websockify --web /usr/share/novnc 6080 127.0.0.1:5900 >/dev/null 2>&1 &
  echo "open  http://localhost:'"$PORT"'/vnc.html?autoconnect=1&resize=scale   (Ctrl-C to stop)" >&2
  QT_QPA_PLATFORM="vnc:size=900x620:port=5900" exec "$OUT/rqt_demo_qml"
' sh "$MODE"
