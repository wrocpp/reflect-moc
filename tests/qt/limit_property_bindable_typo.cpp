// Limit: BINDABLE naming a function the class does not have stops the build, with the name and the class. This file must NOT compile.
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>

class Gauge : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int level READ level BINDABLE bindableLevl)

 public:
  int level() const { return 0; }
  QBindable<int> bindableLevel() { return QBindable<int>(&level_); }

 private:
  QProperty<int> level_;
};

QMetaObject const* gauge_meta(Gauge& g) { return g.metaObject(); }
