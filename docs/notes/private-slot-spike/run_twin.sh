# runs inside the reflect-moc/gcc16-qt610 image, from this directory, with the repo mounted at /src
export LD_LIBRARY_PATH=/opt/qt/lib
QT="-isystem /opt/qt/include/QtCore -isystem /opt/qt/include"
/opt/qt/libexec/moc twin.h -o /tmp/moc_twin.cpp &&
g++ -std=gnu++26 -fPIC $QT -I. -I/src/tests/qt -DMOC_OUTPUT='"/tmp/moc_twin.cpp"' oracle_main.cpp -o /tmp/oracle -L/opt/qt/lib -lQt6Core && /tmp/oracle 2>/dev/null > /tmp/oracle.out
g++ -std=gnu++26 -freflection -fPIC -I/src/include -I. -I/src/tests/qt $QT rqt_main.cpp -o /tmp/rqt -L/opt/qt/lib -lQt6Qml -lQt6Gui -lQt6Widgets -lQt6Core && /tmp/rqt 2>/dev/null > /tmp/rqt.out
diff /tmp/oracle.out /tmp/rqt.out && echo IDENTICAL
