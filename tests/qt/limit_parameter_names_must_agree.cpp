// Limit: a parameter name survives in the meta-object only while every
// declaration agrees. Declared `v` and defined `newValue`, GCC 16.2 reports
// has_identifier false, so QMetaMethod::parameterNames() is empty and QML
// cannot read the argument by name. This builds and passes: it pins the limit.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>

struct Agree : rqt::Object<> {
  Agree() { bind(); }
  [[= rqt::slot]] void set(int v);
  [[= rqt::slot]] void setSame(int v) { value = v; }
  int value = 0;
};

struct Disagree : rqt::Object<> {
  Disagree() { bind(); }
  [[= rqt::slot]] void set(int v);
  int value = 0;
};

void Disagree::set(int newValue) { value = newValue; }
void Agree::set(int v) { value = v; }

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Agree agree;
  Disagree disagree;

  QMetaObject const* agree_mo = agree.metaObject();
  QMetaObject const* disagree_mo = disagree.metaObject();
  CHECK(agree_mo->method(agree_mo->indexOfMethod("setSame(int)")).parameterNames() == (QList<QByteArray>{"v"}));
  CHECK(agree_mo->method(agree_mo->indexOfMethod("set(int)")).parameterNames() == (QList<QByteArray>{"v"}));
  QList<QByteArray> const names = disagree_mo->method(disagree_mo->indexOfMethod("set(int)")).parameterNames();
  CHECK(names == (QList<QByteArray>{QByteArray{}}));
  return rqt_test::finish("limit_parameter_names_must_agree");
}
