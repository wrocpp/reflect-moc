// moc orders properties by their Q_PROPERTY lines; reflect-moc orders them by
// declaration unless `.index` says otherwise. Indexed properties come first in
// index order, the rest follow in declaration order.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaProperty>

struct Ordered : rqt::Object<> {
  Ordered() { bind(); }
  [[= rqt::property{.index = 2}]] int third() const { return 3; }
  [[= rqt::property{}]] int unindexed() const { return 9; }
  [[= rqt::property{.index = 0}]] int first() const { return 1; }
  [[= rqt::property{.index = 1}]] int second() const { return 2; }
  [[= rqt::property{}]] int last() const { return 10; }
};

struct Declared : rqt::Object<> {
  Declared() { bind(); }
  [[= rqt::property{}]] int b() const { return 2; }
  [[= rqt::property{}]] int a() const { return 1; }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Ordered ordered;
  Declared declared;

  QMetaObject const* mo = ordered.metaObject();
  int const first = mo->propertyOffset();
  CHECK(mo->propertyCount() - first == 5);
  CHECK(QByteArray{mo->property(first + 0).name()} == "first");
  CHECK(QByteArray{mo->property(first + 1).name()} == "second");
  CHECK(QByteArray{mo->property(first + 2).name()} == "third");
  CHECK(QByteArray{mo->property(first + 3).name()} == "unindexed");
  CHECK(QByteArray{mo->property(first + 4).name()} == "last");
  CHECK(ordered.property("third").toInt() == 3 && ordered.property("first").toInt() == 1);

  // without .index, declaration order
  QMetaObject const* dmo = declared.metaObject();
  CHECK(QByteArray{dmo->property(dmo->propertyOffset()).name()} == "b");
  CHECK(QByteArray{dmo->property(dmo->propertyOffset() + 1).name()} == "a");
  return rqt_test::finish("capability_property_order_follows_explicit_index");
}
