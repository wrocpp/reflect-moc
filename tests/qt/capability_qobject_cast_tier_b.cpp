// qobject_cast<T*> needs the tier B opt-in; rqt::cast works without it and the
// two agree. HasQ_OBJECT_Macro is specialized only for classes that opt in.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>

struct Opted : rqt::Object<> {
  static QMetaObject const& staticMetaObject;
  Opted() { bind(); }
};
RQT_STATIC_META_OBJECT(Opted);

struct OptedChild : Opted {
  OptedChild() { bind(); }
};

struct Bare : rqt::Object<> {
  Bare() { bind(); }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Opted opted;
  OptedChild child;
  Bare bare;
  QObject plain;

  QObject* o = &opted;
  CHECK(qobject_cast<Opted*>(o) == &opted);
  CHECK(qobject_cast<Opted*>(static_cast<QObject*>(&child)) == &child);
  CHECK(qobject_cast<Opted*>(static_cast<QObject*>(&bare)) == nullptr);
  CHECK(qobject_cast<Opted*>(&plain) == nullptr);
  CHECK(qobject_cast<Opted*>(static_cast<QObject*>(nullptr)) == nullptr);

  CHECK(rqt::cast<Bare>(static_cast<QObject*>(&bare)) == &bare);
  CHECK(rqt::cast<Bare>(&plain) == nullptr);
  CHECK(rqt::cast<Opted>(static_cast<QObject*>(&child)) == &child);
  CHECK(rqt::cast<OptedChild>(o) == nullptr);
  return rqt_test::finish("capability_qobject_cast_tier_b");
}
