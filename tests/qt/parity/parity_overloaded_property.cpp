// A property whose WRITE, RESET or NOTIFY names an overloaded function. moc resolves the setter by C++ overload resolution against
// the property's type, RESET by the empty argument list and NOTIFY to the first signal of the name; reflect-moc must bind the same
// functions, whatever the order of the overloads. docs/moc-parity.md: B-16 (and B-07).
#include "parity.hpp"
#include "parity_overloaded_property_moc.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace viarqt {

class Overloaded : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
  RQT_PROPERTY(int count READ count WRITE setCount)
  RQT_PROPERTY(QString label READ label WRITE setLabel RESET resetLabel)

 public:
  explicit Overloaded(QObject* parent = nullptr) : QObject(parent) {}
  double value() const { return value_; }
  int count() const { return count_; }
  QString label() const { return label_; }
  QByteArray used() const { return used_; }

  void setValue(int v) { store(v, "setValue(int)"); }
  void setValue(double v) { store(v, "setValue(double)"); }
  void setCount(double v) { count_ = static_cast<int>(v) + 1000, used_ = "setCount(double)"; }
  void setCount(int v) { count_ = v, used_ = "setCount(int)"; }
  void setLabel(const QString& text) {
    label_ = text;
    used_ = "setLabel";
  }
  void resetLabel(int) { used_ = "resetLabel(int)"; }
  void resetLabel() {
    label_ = QStringLiteral("reset");
    used_ = "resetLabel()";
  }

 signals:
  [[= rqt::signal_function]] void valueChanged(int value) { rqt::activate{this}(value); }
  [[= rqt::signal_function]] void valueChanged(double value) { rqt::activate{this}(value); }

 private:
  void store(double v, char const* by) {
    value_ = v;
    used_ = by;
    emit valueChanged(v);
  }
  double value_ = 0;
  int count_ = 0;
  QString label_;
  QByteArray used_;
};

}  // namespace viarqt

template <class T>
void behaviour(char const* what) {
  T object;
  rqt_test::report(object.setProperty("value", 2.5), what, __FILE__, __LINE__);
  rqt_test::report(object.value() == 2.5 && object.used() == "setValue(double)", what, __FILE__, __LINE__);
  rqt_test::report(object.setProperty("count", 7), what, __FILE__, __LINE__);
  rqt_test::report(object.count() == 7 && object.used() == "setCount(int)", what, __FILE__, __LINE__);
  rqt_test::report(object.setProperty("label", QString{"x"}) && object.label() == "x", what, __FILE__, __LINE__);
  QMetaProperty const label = object.metaObject()->property(object.metaObject()->indexOfProperty("label"));
  rqt_test::report(label.reset(&object) && object.label() == "reset" && object.used() == "resetLabel()", what, __FILE__, __LINE__);
  // the NOTIFY signal is the first overload of the name, in both builds, and it carries the write
  QMetaProperty const value = object.metaObject()->property(object.metaObject()->indexOfProperty("value"));
  rqt_test::report(value.notifySignal().methodSignature() == QByteArray{"valueChanged(int)"}, what, __FILE__, __LINE__);
  int doubles = 0;
  QObject::connect(&object, QOverload<double>::of(&T::valueChanged), [&](double) { ++doubles; });
  object.setProperty("value", 3.5);
  rqt_test::report(doubles == 1, what, __FILE__, __LINE__);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  viamoc::Overloaded moc_object;
  viarqt::Overloaded rqt_object;
  PARITY_SAME("Overloaded", moc_object.metaObject(), rqt_object.metaObject());
  CHECK(parity::same_totals(moc_object.metaObject(), rqt_object.metaObject()));
  behaviour<viamoc::Overloaded>("moc behaviour");
  behaviour<viarqt::Overloaded>("rqt behaviour");
  return rqt_test::finish("parity_overloaded_property");
}
