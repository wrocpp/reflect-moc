// A signal data member with several signatures, `rqt::signal<void(int), void(const QString&)> valueChanged;`, against moc's
// two overloaded signals: the same meta-object row for row, string connects by signature, QSignalSpy, and pointer connects
// through rqt::overload with both QObject::connect and rqt::connect. docs/moc-parity.md: B-03, B-15.
#include "parity.hpp"
#include "parity_overloads_multi_member_moc.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtTest/QSignalSpy>

namespace viarqt {

class MultiSignal : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)

 public:
  explicit MultiSignal(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }
  QString label() const { return label_; }

 public slots:
  [[= rqt::slot]] void setValue(int v) {
    value_ = v;
    valueChanged(v);
  }
  [[= rqt::slot]] void setLabel(const QString& text) {
    label_ = text;
    valueChanged(text);
  }

 signals:
  [[= rqt::names("value", "text")]] rqt::signal<void(int), void(const QString&)> valueChanged;

 private:
  int value_ = 0;
  QString label_;
};

}  // namespace viarqt

// The same twin, with the signals in the function form: both rqt syntaxes are measured against one moc class (same class name,
// in another namespace, as the meta-object names the class without its namespace).
namespace viarqt_function_form {

class MultiSignal : public QObject {
  RQT_OBJECT
  RQT_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)

 public:
  explicit MultiSignal(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }
  QString label() const { return label_; }

 public slots:
  [[= rqt::slot]] void setValue(int v) {
    value_ = v;
    emit valueChanged(v);
  }
  [[= rqt::slot]] void setLabel(const QString& text) {
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

}  // namespace viarqt_function_form

template <class T>
void behaviour(char const* what) {
  T object;
  T target;
  QMetaObject const* mo = object.metaObject();
  int const intRow = mo->indexOfSignal("valueChanged(int)");
  int const textRow = mo->indexOfSignal("valueChanged(QString)");
  rqt_test::report(intRow >= 0 && textRow == intRow + 1, what, __FILE__, __LINE__);
  rqt_test::report(mo->method(textRow).methodSignature() == QByteArray{"valueChanged(QString)"} &&
                       mo->method(textRow).parameterNames() == QList<QByteArray>{"text"},
                   what, __FILE__, __LINE__);

  QSignalSpy ints(&object, SIGNAL(valueChanged(int)));
  QSignalSpy texts(&object, SIGNAL(valueChanged(QString)));
  object.setValue(7);
  object.setLabel(QString{"hi"});
  rqt_test::report(ints.count() == 1 && texts.count() == 1 && ints.at(0).at(0).toInt() == 7 && texts.at(0).at(0).toString() == "hi", what,
                   __FILE__, __LINE__);

  // a string-based connect names the signature
  QObject::connect(&object, SIGNAL(valueChanged(QString)), &target, SLOT(setLabel(QString)));
  object.setLabel(QString{"seen"});
  rqt_test::report(target.label() == "seen" && target.value() == 0, what, __FILE__, __LINE__);

  // a queued connect carries each row's arguments across the event loop (the argument types are registered per row)
  T queued;
  QObject::connect(&object, SIGNAL(valueChanged(QString)), &queued, SLOT(setLabel(QString)), Qt::QueuedConnection);
  QObject::connect(&object, SIGNAL(valueChanged(int)), &queued, SLOT(setValue(int)), Qt::QueuedConnection);
  object.setLabel(QString{"later"});
  object.setValue(9);
  rqt_test::report(queued.label().isEmpty() && queued.value() == 0, what, __FILE__, __LINE__);
  QCoreApplication::processEvents();
  rqt_test::report(queued.label() == "later" && queued.value() == 9, what, __FILE__, __LINE__);

  // the property's NOTIFY is the first signal of the name
  QMetaProperty const p = mo->property(mo->indexOfProperty("value"));
  rqt_test::report(p.notifySignal().methodSignature() == QByteArray{"valueChanged(int)"}, what, __FILE__, __LINE__);

}

// Pointer-to-member connects need a selector: a pointer to the member alone names no single signal.
void selectors(char const* what) {
  viarqt::MultiSignal from;
  viarqt::MultiSignal to;
  int got = 0;
  QObject::connect(&from, rqt::overload<void(int)>(&viarqt::MultiSignal::valueChanged), [&](int v) { got += v; });
  rqt::connect(&from, rqt::overload<void(const QString&)>(&viarqt::MultiSignal::valueChanged), &to, &viarqt::MultiSignal::setLabel);
  from.setValue(5);
  from.setLabel(QString{"via selector"});
  rqt_test::report(got == 5 && to.label() == "via selector", what, __FILE__, __LINE__);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  viamoc::MultiSignal moc_object;
  viarqt::MultiSignal rqt_object;
  PARITY_SAME("MultiSignal", moc_object.metaObject(), rqt_object.metaObject());
  CHECK(parity::same_totals(moc_object.metaObject(), rqt_object.metaObject()));
  viarqt_function_form::MultiSignal function_form;
  PARITY_SAME("FunctionFormSignal", moc_object.metaObject(), function_form.metaObject());
  behaviour<viamoc::MultiSignal>("moc behaviour");
  behaviour<viarqt::MultiSignal>("rqt behaviour");
  behaviour<viarqt_function_form::MultiSignal>("rqt function-form behaviour");
  selectors("rqt selectors");
  return rqt_test::finish("parity_overloads_multi_member");
}
