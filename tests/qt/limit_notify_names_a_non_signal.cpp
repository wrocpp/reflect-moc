// Limit: a NOTIFY that names a function that is not an rqt::signal fails the
// build. This file must NOT compile with
//   uncaught exception of type 'std::meta::exception'; 'what()': 'rqt::property NOTIFY 'refresh': is not an rqt::signal with at most one parameter'
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

struct Gauge : rqt::Object<> {
  Gauge() { bind(); }
  [[= rqt::property{.notify = "refresh"}]] int level() const { return 0; }
  void refresh(int) {}
};

QMetaObject const* gauge_meta(Gauge& g) { return g.metaObject(); }
