// Limit: a NOTIFY that names no member at all is an error (moc: NOTIFY signal does not exist in class). This file must NOT compile.
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

class Gauge : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int level READ level NOTIFY levelChanged)

 public:
  int level() const { return 0; }
  bool isDesignable() const { return true; }
 };

QMetaObject const* gauge_meta(Gauge& g) { return g.metaObject(); }
