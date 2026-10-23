// A gadget with no macro, and a gadget derived from it, against the same types processed by moc: the meta-objects must
// equal moc's row for row, and the stock-Qt access to a gadget (QMetaProperty::readOnGadget and writeOnGadget,
// QMetaMethod::invokeOnGadget, QMetaType::metaObject, a QVariant that holds one) must behave the same.
// docs/moc-parity.md: A-03.
#include "parity.hpp"
#include "parity_gadget_moc.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>
#include <QtCore/QVariant>

namespace viarqt {

struct Point : rqt::Gadget<> {
  [[= rqt::property{}]] int x = 0;
  [[= rqt::property{.write = "setLabel"}]] QString label() const { return label_; }
  void setLabel(const QString& l) { label_ = l; }
  [[= rqt::invokable]] int manhattan(int y) const { return x + y; }

 private:
  QString label_;
};

struct Point3 : rqt::Gadget<Point> {
  [[= rqt::property{}]] int z = 0;
  [[= rqt::invokable]] int sum() const { return x + z; }
};

}  // namespace viarqt

template <class P, class P3>
void behaviour(char const* what) {
  QMetaObject const& mo = P::staticMetaObject;
  P p;
  rqt_test::report(mo.property(mo.indexOfProperty("x")).writeOnGadget(&p, 7) && p.x == 7, what, __FILE__, __LINE__);
  rqt_test::report(mo.property(mo.indexOfProperty("x")).readOnGadget(&p).toInt() == 7, what, __FILE__, __LINE__);
  rqt_test::report(mo.property(mo.indexOfProperty("label")).writeOnGadget(&p, QString{"a"}) && p.label() == "a", what, __FILE__, __LINE__);
  int out = 0;
  rqt_test::report(mo.method(mo.indexOfMethod("manhattan(int)")).invokeOnGadget(&p, Q_RETURN_ARG(int, out), Q_ARG(int, 3)) && out == 10, what, __FILE__, __LINE__);

  P3 q;
  QMetaObject const& mo3 = P3::staticMetaObject;
  rqt_test::report(mo3.superClass() == &mo, what, __FILE__, __LINE__);
  rqt_test::report(mo3.property(mo3.indexOfProperty("z")).writeOnGadget(&q, 5) && mo3.property(mo3.indexOfProperty("x")).writeOnGadget(&q, 2), what, __FILE__, __LINE__);
  rqt_test::report(mo3.method(mo3.indexOfMethod("sum()")).invokeOnGadget(&q, Q_RETURN_ARG(int, out)) && out == 7, what, __FILE__, __LINE__);

  QVariant v = QVariant::fromValue(p);
  rqt_test::report(v.metaType().metaObject() == &mo && v.value<P>().x == 7, what, __FILE__, __LINE__);
  rqt_test::report(QMetaType::fromType<P3>().metaObject() == &mo3, what, __FILE__, __LINE__);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  PARITY_SAME("Point", &viamoc::Point::staticMetaObject, &viarqt::Point::staticMetaObject);
  CHECK(parity::same_totals(&viamoc::Point::staticMetaObject, &viarqt::Point::staticMetaObject));
  PARITY_SAME("Point3", &viamoc::Point3::staticMetaObject, &viarqt::Point3::staticMetaObject);
  CHECK(parity::same_totals(&viamoc::Point3::staticMetaObject, &viarqt::Point3::staticMetaObject));
  behaviour<viamoc::Point, viamoc::Point3>("moc behaviour");
  behaviour<viarqt::Point, viarqt::Point3>("rqt behaviour");
  return rqt_test::finish("parity_gadget");
}
