// Overloaded signals, slots and invokables, with signals in the function form (a data member cannot be overloaded: two
// members cannot share a name). The meta-object must equal moc's row for row, string-based connections must pick the
// right overload, and invokeMethod must dispatch on the argument type. docs/moc-parity.md: B-03, B-04, B-07.
#include "parity.hpp"
#include "parity_overloads_function_form_moc.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace viarqt {

class Overloaded : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)

 public:
  explicit Overloaded(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }
  QString label() const { return label_; }
  [[= rqt::invokable]] int twice(int n) const { return 2 * n; }
  [[= rqt::invokable]] QString twice(const QString& text) const { return text + text; }

 public slots:
  [[= rqt::slot]] void setValue(int v) {
    value_ = v;
    emit valueChanged(v);
  }
  [[= rqt::slot]] void setValue(const QString& text) {
    label_ = text;
    emit valueChanged(text);
  }

 signals:
  [[= rqt::signal_function]] void valueChanged(int value) { rqt::activate{this}(value); }
  [[= rqt::signal_function]] void valueChanged(const QString& text) { rqt::activate{this}(text); }

 private:
  int value_ = 0;
  QString label_;
};

}  // namespace viarqt

template <class T>
void behaviour(char const* what) {
  T object;
  int ints = 0;
  QString lastText;
  QObject::connect(&object, QOverload<int>::of(&T::valueChanged), [&](int) { ++ints; });
  QObject::connect(&object, QOverload<const QString&>::of(&T::valueChanged), [&](const QString& t) { lastText = t; });
  object.setValue(7);
  object.setValue(QString{"hi"});
  rqt_test::report(ints == 1 && object.value() == 7 && lastText == "hi" && object.label() == "hi", what, __FILE__, __LINE__);

  // string-based invoke picks the overload by the argument types
  int n = 0;
  QString s;
  rqt_test::report(QMetaObject::invokeMethod(&object, "twice", Q_RETURN_ARG(int, n), Q_ARG(int, 21)) && n == 42, what, __FILE__, __LINE__);
  rqt_test::report(QMetaObject::invokeMethod(&object, "twice", Q_RETURN_ARG(QString, s), Q_ARG(QString, QString{"ab"})) && s == "abab",
                   what, __FILE__, __LINE__);
  // a string-based connect names the overload in its signature
  T target;
  QObject::connect(&object, SIGNAL(valueChanged(QString)), &target, SLOT(setValue(QString)));
  object.setValue(QString{"seen"});
  rqt_test::report(target.label() == "seen", what, __FILE__, __LINE__);
  // the property's NOTIFY names the first overload (the int one) in both builds
  QMetaProperty const p = object.metaObject()->property(object.metaObject()->indexOfProperty("value"));
  rqt_test::report(p.notifySignal().methodSignature() == QByteArray{"valueChanged(int)"}, what, __FILE__, __LINE__);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  viamoc::Overloaded moc_object;
  viarqt::Overloaded rqt_object;
  PARITY_SAME("Overloaded", moc_object.metaObject(), rqt_object.metaObject());
  CHECK(parity::same_totals(moc_object.metaObject(), rqt_object.metaObject()));
  behaviour<viamoc::Overloaded>("moc behaviour");
  behaviour<viarqt::Overloaded>("rqt behaviour");
  return rqt_test::finish("parity_overloads_function_form");
}
