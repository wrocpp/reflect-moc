// Limit: a WRITE in RQT_PROPERTY that names no member function is an error, not a read-only property. This file must NOT compile.
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

class Gauge : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int level READ level WRITE setLevl)

 public:
  int level() const { return 0; }
  bool isDesignable() const { return true; }
 };

QMetaObject const* gauge_meta(Gauge& g) { return g.metaObject(); }
