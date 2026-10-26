export LD_LIBRARY_PATH=/opt/qt/lib
QT="-isystem /opt/qt/include/QtCore -isystem /opt/qt/include"
F="-std=gnu++26 -freflection -fPIC -I/src/include $QT -I."
echo "-- dependency tracking"; g++ $F -MD -MF /tmp/d.d -c rqt_plugin.cpp -o /tmp/p.o && tr ' \\' '\n\n' < /tmp/d.d | grep -c "meta.json"
echo "-- compile time, plugin with encoder vs without (same class, exports from a fixed array)"
sed 's/#embed "meta.json"/#embed "bad.json"/' rqt_plugin.cpp > rqt_bad.cpp
echo "-- bad json"; g++ $F -c rqt_bad.cpp -o /tmp/p.o 2>&1 | grep -E "error|what" | head -4
sed 's/#embed "meta.json"/#embed "edge.json"/' rqt_plugin.cpp > rqt_edge_tmp.cpp
echo "-- edge.json (293 bytes of cbor) compile"; ( time g++ $F -c rqt_edge_tmp.cpp -o /tmp/p.o ) 2>&1 | grep -E "error|real"
echo "-- meta.json compile"; ( time g++ $F -c rqt_plugin.cpp -o /tmp/p.o ) 2>&1 | grep -E "error|real"
echo "-- baseline: same class without the metadata"; sed '/^static constexpr unsigned char meta_json/,$d' rqt_plugin.cpp | sed '/json_to_cbor/d' > rqt_base_tmp.cpp; ( time g++ $F -c rqt_base_tmp.cpp -o /tmp/p.o ) 2>&1 | grep -E "error|real"
echo "-- missing file"; sed 's/#embed "meta.json"/#embed "nosuch.json"/' rqt_plugin.cpp > rqt_miss_tmp.cpp; g++ $F -c rqt_miss_tmp.cpp -o /tmp/p.o 2>&1 | grep -E "error" | head -2
echo "-- constexpr steps limit: a ~64 KiB json"; python3 -c "
import json;print(json.dumps({'k%d'%i:[i,'v%d'%i,i*0.5] for i in range(2500)}))" > big.json 2>/dev/null || true
ls -l big.json | awk '{print $5}'
sed 's/#embed "meta.json"/#embed "big.json"/' rqt_plugin.cpp > rqt_big_tmp.cpp; ( time g++ $F -c rqt_big_tmp.cpp -o /tmp/p.o ) 2>&1 | grep -E "error|real|limit" | head -4
rm -f rqt_*_tmp.cpp rqt_bad.cpp
