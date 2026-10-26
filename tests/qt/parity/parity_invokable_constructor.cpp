// Q_INVOKABLE on a constructor, for a QObject class and for a gadget, against the same types processed by moc: the
// constructor rows (signature, parameter names and types, access, the cloned row of each default argument) must equal
// moc's, and every consumer of them must behave the same: QMetaObject::newInstance, indexOfConstructor, ConstructInPlace
// through the static meta-call, and a JS `new` through QJSEngine::newQMetaObject.
// docs/moc-parity.md: B-13.
#include "parity.hpp"
#include "parity_invokable_constructor_moc.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtQml/QJSEngine>

#include <memory>
#include <new>

namespace viarqt {

class Widget : public rqt::ObjectOf<> {
 public:
  [[= rqt::invokable]] explicit Widget(QObject* parent = nullptr) : ObjectOf(parent) {}
  [[= rqt::invokable]] Widget(int a, int b = 3, QObject* parent = nullptr) : ObjectOf(parent), sum(a + b) {}
  [[= rqt::invokable]] Widget(const QString& label, double scale) : label(label), sum(static_cast<int>(scale)) {}
  int sum = 0;
  QString label;
};

struct Point : rqt::Gadget<> {
  [[= rqt::invokable]] Point() = default;
  [[= rqt::invokable]] Point(int x, int y = 7) : x(x), y(y) {}
  int x = 0;
  int y = 0;
};

}  // namespace viarqt

namespace {
constexpr int kA = 5;
constexpr int kB = 11;
constexpr int kDefaultB = 3;
constexpr int kDefaultY = 7;
constexpr double kScale = 4.0;
}  // namespace

// The name of a constructor row: Qt names a constructor after the class, namespace included.
QByteArray constructor_signature(QMetaObject const& mo, char const* arguments) {
  return QByteArray{mo.className()} + '(' + arguments + ')';
}

// newInstance picks the constructor by its argument types, defaults included.
template <class W>
void widget_behaviour(char const* what) {
  QMetaObject const& mo = W::staticMetaObject;
  rqt_test::report(mo.constructorCount() == 6, what, __FILE__, __LINE__);
  std::unique_ptr<W> plain{static_cast<W*>(mo.newInstance())};
  std::unique_ptr<W> defaulted{static_cast<W*>(mo.newInstance(Q_ARG(int, kA)))};
  std::unique_ptr<W> both{static_cast<W*>(mo.newInstance(Q_ARG(int, kA), Q_ARG(int, kB)))};
  std::unique_ptr<W> labelled{static_cast<W*>(mo.newInstance(Q_ARG(QString, QString{"w"}), Q_ARG(double, kScale)))};
  rqt_test::report(plain && plain->sum == 0, what, __FILE__, __LINE__);
  rqt_test::report(defaulted && defaulted->sum == kA + kDefaultB, what, __FILE__, __LINE__);
  rqt_test::report(both && both->sum == kA + kB, what, __FILE__, __LINE__);
  rqt_test::report(labelled && labelled->label == "w" && labelled->sum == static_cast<int>(kScale), what, __FILE__, __LINE__);
  QObject parent;
  std::unique_ptr<W> owned{static_cast<W*>(mo.newInstance(Q_ARG(int, kA), Q_ARG(int, kB), Q_ARG(QObject*, &parent)))};
  rqt_test::report(owned && owned->parent() == &parent, what, __FILE__, __LINE__);
  rqt_test::report(mo.newInstance(Q_ARG(QString, QString{"w"})) == nullptr, what, __FILE__, __LINE__);  // no such constructor
  rqt_test::report(mo.indexOfConstructor(constructor_signature(mo, "int,int")) >= 0 &&
                       mo.indexOfConstructor(constructor_signature(mo, "QString,double")) >= 0 &&
                       mo.indexOfConstructor(constructor_signature(mo, "QString")) < 0,
                   what, __FILE__, __LINE__);
}

// A gadget is not a QObject: newInstance is not offered, so the static meta-call is called as Qt calls it.
template <class P>
void point_behaviour(char const* what) {
  QMetaObject const& mo = P::staticMetaObject;
  rqt_test::report(mo.constructorCount() == 3, what, __FILE__, __LINE__);
  int x = kA;
  int y = kB;
  void* created = nullptr;
  void* args[] = {&created, &x, &y};
  mo.d.static_metacall(nullptr, QMetaObject::CreateInstance, mo.indexOfConstructor(constructor_signature(mo, "int,int")), args);
  auto* two = static_cast<P*>(created);
  rqt_test::report(two != nullptr && two->x == kA && two->y == kB, what, __FILE__, __LINE__);
  delete two;
  void* one_args[] = {&created, &x};
  mo.d.static_metacall(nullptr, QMetaObject::CreateInstance, mo.indexOfConstructor(constructor_signature(mo, "int")), one_args);
  auto* one = static_cast<P*>(created);
  rqt_test::report(one != nullptr && one->x == kA && one->y == kDefaultY, what, __FILE__, __LINE__);
  delete one;

  alignas(P) unsigned char storage[sizeof(P)];
  void* place_args[] = {storage, &x, &y};
  mo.d.static_metacall(nullptr, QMetaObject::ConstructInPlace, mo.indexOfConstructor(constructor_signature(mo, "int,int")), place_args);
  auto* placed = std::launder(reinterpret_cast<P*>(storage));
  rqt_test::report(placed->x == kA && placed->y == kB, what, __FILE__, __LINE__);
  placed->~P();
  rqt_test::report(mo.indexOfConstructor(constructor_signature(mo, "")) >= 0 && mo.indexOfConstructor(constructor_signature(mo, "QString")) < 0, what, __FILE__, __LINE__);
}

// JS `new Widget(...)` goes through the constructor rows.
template <class W>
void script_behaviour(char const* what) {
  QJSEngine engine;
  engine.globalObject().setProperty("Widget", engine.newQMetaObject(&W::staticMetaObject));
  QJSValue const made = engine.evaluate("new Widget(5, 11)");
  rqt_test::report(!made.isError() && made.isQObject() && static_cast<W*>(made.toQObject())->sum == kA + kB, what, __FILE__, __LINE__);
  QJSValue const defaulted = engine.evaluate("new Widget(5)");
  rqt_test::report(!defaulted.isError() && defaulted.isQObject() && static_cast<W*>(defaulted.toQObject())->sum == kA + kDefaultB, what,
                   __FILE__, __LINE__);
  QJSValue const labelled = engine.evaluate("new Widget('w', 4.0)");
  rqt_test::report(!labelled.isError() && labelled.isQObject() && static_cast<W*>(labelled.toQObject())->label == "w", what, __FILE__,
                   __LINE__);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  PARITY_SAME("Widget", &viamoc::Widget::staticMetaObject, &viarqt::Widget::staticMetaObject);
  CHECK(parity::same_totals(&viamoc::Widget::staticMetaObject, &viarqt::Widget::staticMetaObject));
  PARITY_SAME("Point", &viamoc::Point::staticMetaObject, &viarqt::Point::staticMetaObject);
  CHECK(parity::same_totals(&viamoc::Point::staticMetaObject, &viarqt::Point::staticMetaObject));
  widget_behaviour<viamoc::Widget>("moc behaviour");
  widget_behaviour<viarqt::Widget>("rqt behaviour");
  point_behaviour<viamoc::Point>("moc behaviour");
  point_behaviour<viarqt::Point>("rqt behaviour");
  script_behaviour<viamoc::Widget>("moc behaviour");
  script_behaviour<viarqt::Widget>("rqt behaviour");
  return rqt_test::finish("parity_invokable_constructor");
}
