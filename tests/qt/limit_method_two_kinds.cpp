// Limit: a function annotated as a slot and an invokable (or a signal) is an error: the table would list it as only the
// first kind and silently drop the other. This file must NOT compile.
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

class Worker : public QObject {
  RQT_OBJECT

 public:
  [[= rqt::slot, = rqt::invokable]] void run() {}
};

QMetaObject const* worker_meta(Worker& w) { return w.metaObject(); }
