export LD_LIBRARY_PATH=/opt/qt/lib QT_QPA_PLATFORM=offscreen
QT="-isystem /opt/qt/include/QtCore -isystem /opt/qt/include"
/opt/qt/libexec/moc moc_plugin_main.h -o /tmp/moc_dp.cpp 2>&1 | grep -v -i locale
g++ -std=gnu++26 -fPIC -shared $QT -I. -DMOC_OUTPUT='"/tmp/moc_dp.cpp"' moc_plugin.cpp -o /tmp/libmocdemo.so -L/opt/qt/lib -lQt6Core 2>&1 | grep -E "error" | head
g++ -std=gnu++26 -freflection -fPIC -shared -I/src/include $QT -I. rqt_plugin.cpp -o /tmp/librqtdemo.so -L/opt/qt/lib -lQt6Core 2>&1 | grep -E "error|what\(\)" | head
g++ -std=gnu++26 -fPIC $QT -I. host.cpp -o /tmp/host -L/opt/qt/lib -lQt6Core 2>&1 | grep error
echo "-- exported symbols"; for l in mocdemo rqtdemo; do echo $l; nm -D --defined-only /tmp/lib$l.so | grep -i "qt_plugin"; done
echo "-- moc plugin"; /tmp/host /tmp/libmocdemo.so 2>/dev/null | tee /tmp/a.out
echo "-- reflect-moc plugin"; /tmp/host /tmp/librqtdemo.so 2>/dev/null | tee /tmp/b.out
diff /tmp/a.out /tmp/b.out && echo SAME-HOST-VIEW
echo "-- section bytes"; for l in mocdemo rqtdemo; do readelf -n /tmp/lib$l.so 2>/dev/null | grep -i -A2 qt-project | head -4; done
echo "-- ELF note payload, moc vs reflect-moc"
for l in mocdemo rqtdemo; do readelf -n /tmp/lib$l.so 2>/dev/null | grep -A3 "qt-project" | grep -v "^--" | sed 's/0x000000[0-9a-f]*//' > /tmp/note_$l.txt; done
diff /tmp/note_mocdemo.txt /tmp/note_rqtdemo.txt && echo NOTES-IDENTICAL
echo "-- static plugin build (-DQT_STATICPLUGIN), compile only"
g++ -std=gnu++26 -fPIC $QT -I. -DQT_STATICPLUGIN -DMOC_OUTPUT='"/tmp/moc_dp.cpp"' -c moc_plugin.cpp -o /tmp/s1.o 2>&1 | grep -c error
g++ -std=gnu++26 -freflection -fPIC -I/src/include $QT -I. -DQT_STATICPLUGIN -c rqt_plugin.cpp -o /tmp/s2.o 2>&1 | grep -c error
nm /tmp/s1.o | grep -i "qt_static_plugin\|qt_plugin"; echo ..; nm /tmp/s2.o | grep -i "qt_static_plugin\|qt_plugin"
