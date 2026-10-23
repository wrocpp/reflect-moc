// Limit: rqt::revision on a function that is no signal, slot or invokable is an error: nothing would read it. This file
// must NOT compile.
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

class Worker : public QObject {
  RQT_OBJECT

 public:
  [[= rqt::revision{1, 2}]] void run() {}
};

QMetaObject const* worker_meta(Worker& w) { return w.metaObject(); }
