// Limit: a property with neither READ nor MEMBER is an error, as it is for moc. This file must NOT compile.
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

class Gauge : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int level NOTIFY changed)

 public:
  int level() const { return 0; }
  bool isDesignable() const { return true; }
 };

QMetaObject const* gauge_meta(Gauge& g) { return g.metaObject(); }
