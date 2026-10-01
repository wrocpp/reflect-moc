// Limit: a WRITE name that matches no member is a build error, not a silent
// read-only property. This file must NOT compile with
//   uncaught exception of type 'std::meta::exception'; 'what()': 'rqt::property WRITE 'setLvel': no member function of that name in Gauge'
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

struct Gauge : rqt::Object<> {
  Gauge() { bind(); }
  [[= rqt::property{.write = "setLvel"}]] int level() const { return level_; }
  [[= rqt::slot]] void setLevel(int l) { level_ = l; }
  int level_ = 0;
};

QMetaObject const* gauge_meta(Gauge& g) { return g.metaObject(); }
