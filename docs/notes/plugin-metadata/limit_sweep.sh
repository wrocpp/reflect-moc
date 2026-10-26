#!/bin/sh
# For each shape of JSON, the largest file (bytes) the encoder builds under the default -fconstexpr-ops-limit, by bisection.
# Run inside the reflect-moc/gcc16-qt610 image with the repository at /src:
#   docker run --rm -v "$PWD":/src:ro -w /tmp reflect-moc/gcc16-qt610 sh /src/docs/notes/plugin-metadata/limit_sweep.sh
# Output: shape, the largest size that built, the smallest that did not, the repeats n of each.
QT="-isystem /opt/qt/include/QtCore -isystem /opt/qt/include"
here=/src/docs/notes/plugin-metadata
cat > probe.cpp <<'CPP'
#include <reflect_moc/qt/plugin.hpp>
class P : public QObject {};
static constexpr unsigned char json[] = {
#embed "probe.json"
};
RQT_PLUGIN_METADATA(P, "org.example.P/1.0", json)
CPP
builds() {  # shape n -> exit status 0 when it compiles
  python3 "$here/gen_json.py" "$1" "$2" > probe.json
  g++ -std=gnu++26 -freflection -fPIC -I/src/include $QT -DRQT_PLUGIN_JSON_MAX_BYTES=1000000000 -c probe.cpp -o probe.o 2> probe.err
}
for shape in indented compact numbers strings flatmap deep; do
  ok=1; bad=0; n=64
  while builds $shape $n; do ok=$n; n=$((n * 2)); [ $n -gt 1000000 ] && break; done
  bad=$n
  while [ $((bad - ok)) -gt $((ok / 16 + 1)) ]; do
    mid=$(((ok + bad) / 2))
    if builds $shape $mid; then ok=$mid; else bad=$mid; fi
  done
  python3 "$here/gen_json.py" $shape $ok > probe.json; ok_bytes=$(wc -c < probe.json)
  builds $shape $bad; python3 "$here/gen_json.py" $shape $bad > probe.json; bad_bytes=$(wc -c < probe.json)
  echo "$shape: builds $ok_bytes bytes (n=$ok), fails $bad_bytes bytes (n=$bad): $(grep -m1 -o 'operation count exceeds limit of [0-9]*' probe.err)"
done
