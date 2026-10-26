F="-std=gnu++26 -freflection -fPIC -I/src/include -isystem /opt/qt/include/QtCore -isystem /opt/qt/include -I."
for n in 25 50 100 150 200; do
  python3 -c "
import json;print(json.dumps({'k%d'%i:[i,'v%d'%i,i*0.5] for i in range($n)}))" > big.json
  sed 's/#embed "meta.json"/#embed "big.json"/' rqt_plugin.cpp > rqt_big_tmp.cpp
  s=$(date +%s%N); r=$(g++ $F -c rqt_big_tmp.cpp -o /tmp/p.o 2>&1 | grep -c "operation count exceeds"); e=$(date +%s%N)
  echo "entries=$n json_bytes=$(wc -c < big.json) over_default_ops_limit=$r wall_ms=$(( (e - s) / 1000000 ))"
done
