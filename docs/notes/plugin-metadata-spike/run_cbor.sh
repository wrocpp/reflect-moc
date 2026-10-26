# inside the image, from this directory
export LD_LIBRARY_PATH=/opt/qt/lib
QT="-isystem /opt/qt/include/QtCore -isystem /opt/qt/include"
for f in "$@"; do
  echo "== $f"
  sed "s/EdgePlugin/EdgePlugin/" plugin_edge.h > /tmp/plugin_edge.h
  /opt/qt/libexec/moc -DJSON_FILE="\"$f\"" plugin_edge.h -o /tmp/moc_edge.cpp 2>&1 | grep -v -i locale
  g++ -std=gnu++26 -fPIC $QT -I. -DJSON_FILE="\"$f\"" -DMOC_OUTPUT='"/tmp/moc_edge.cpp"' cbor_vs_moc.cpp -o /tmp/cbor -L/opt/qt/lib -lQt6Core 2>&1 | grep -E "error|what\(\)|note: .*(throw|json)" | head -5
  /tmp/cbor 2>/dev/null
done
