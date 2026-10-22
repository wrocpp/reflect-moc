// Limit: rqt::scriptable on a function that is no signal, slot or invokable is an error: nothing would read it. This file
// must NOT compile.
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

class Worker : public QObject {
  RQT_OBJECT

 public:
  [[= rqt::scriptable]] int value() const { return 0; }
};

QMetaObject const* worker_meta(Worker& w) { return w.metaObject(); }
