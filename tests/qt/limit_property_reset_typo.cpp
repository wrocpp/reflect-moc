// Limit: a RESET that names no member function is an error. This file must NOT compile.
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

class Gauge : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int level READ level RESET resetIt)

 public:
  int level() const { return 0; }
  bool isDesignable() const { return true; }
 };

QMetaObject const* gauge_meta(Gauge& g) { return g.metaObject(); }
