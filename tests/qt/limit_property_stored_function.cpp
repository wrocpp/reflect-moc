// Limit: STORED with a function name stops the build instead of being read as true. This file must NOT compile.
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

class Gauge : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int level READ level STORED isDesignable)

 public:
  int level() const { return 0; }
  bool isDesignable() const { return true; }
 };

QMetaObject const* gauge_meta(Gauge& g) { return g.metaObject(); }
