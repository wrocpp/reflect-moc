// Tier B in one line inside the class (RQT_META_OBJECT): qobject_cast, Qt's
// pointer-to-member and functor connect, qmlRegisterType through
// rqt::register_qml, and an inheritance chain where each class has its own line.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>

#include <memory>

namespace {
constexpr int kStart = 3;
constexpr int kBumped = 9;
constexpr int kInitial = 5;
constexpr int kMajor = 1;
constexpr int kMinor = 0;
}  // namespace

struct Base : rqt::Object<> {
  RQT_META_OBJECT(Base);
  Base() { bind(); }
  [[= rqt::property{.write = "setValue", .notify = "valueChanged"}]] int value() const { return value_; }
  [[= rqt::signal_function]] void valueChanged(int value) { rqt::emit{this}(value); }
  [[= rqt::slot]] void setValue(int v) {
    if (v == value_) return;
    value_ = v;
    valueChanged(v);
  }
  int value_ = 0;
};

struct Derived : Base {
  RQT_META_OBJECT(Derived);
  Derived() { bind(); }
  [[= rqt::slot]] void bump() { setValue(kBumped); }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Base base;
  Derived derived;

  // each class has its own meta-object, named for the class
  CHECK(&Base::staticMetaObject != &Derived::staticMetaObject);
  CHECK(QByteArray{Base::staticMetaObject.className()} == "Base");
  CHECK(QByteArray{Derived::staticMetaObject.className()} == "Derived");
  CHECK(Derived::staticMetaObject.superClass() == &Base::staticMetaObject);
  CHECK(derived.metaObject() == &Derived::staticMetaObject);

  // qobject_cast, down and sideways
  QObject* o = &derived;
  CHECK(qobject_cast<Derived*>(o) == &derived);
  CHECK(qobject_cast<Base*>(o) == &derived);
  CHECK(qobject_cast<Derived*>(static_cast<QObject*>(&base)) == nullptr);
  CHECK(qobject_cast<Derived*>(static_cast<Base*>(&derived)) == &derived);

  // Qt's own connect: pointer to member and functor
  int seen = 0;
  CHECK(QObject::connect(&base, &Base::valueChanged, &base, [&](int v) { seen = v; }));
  Base other;
  CHECK(QObject::connect(&base, &Base::valueChanged, &other, &Base::setValue));
  base.setValue(kStart);
  CHECK(seen == kStart && other.value_ == kStart);
  CHECK(QObject::connect(&derived, &Derived::valueChanged, &other, &Base::setValue));
  derived.bump();
  CHECK(other.value_ == kBumped);

  // QML creates the derived class and sets a property it inherits
  CHECK(rqt::register_qml<Derived>("rqt.oneline", kMajor, kMinor, "Derived") >= 0);
  QQmlEngine engine;
  QQmlComponent component(&engine);
  component.setData("import QtQml\nimport rqt.oneline\nDerived { value: 5 }\n", QUrl{});
  std::unique_ptr<QObject> created{component.create()};
  if (!created) std::fprintf(stderr, "QML errors: %s\n", component.errorString().toUtf8().constData());
  CHECK(created);
  if (created) CHECK(qobject_cast<Derived*>(created.get()) && qobject_cast<Derived*>(created.get())->value_ == kInitial);
  return rqt_test::finish("capability_static_metaobject_one_line");
}
