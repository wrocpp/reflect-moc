// QMetaObject::invokeMethod by name on the real Qt 6.10, the method table that
// QMetaMethod reads back, and the cloned rows for default arguments.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QString>

namespace {
constexpr int kLevel = 3;
constexpr int kStepDefault = 1;
constexpr int kStepGiven = 5;
constexpr int kOperand = 21;
constexpr int kMethodCount = 5;
}  // namespace

struct Calc : rqt::Object<> {
  Calc() { bind(); }
  int level = 0;
  int step = 0;
  [[= rqt::signal_function]] void changed() { rqt::emit{this}(); }
  [[= rqt::slot]] void setLevel(int newLevel, int newStep = kStepDefault) {
    level = newLevel;
    step = newStep;
  }
  [[= rqt::invokable]] int twice(int n) const { return 2 * n; }
  [[= rqt::invokable]] QString greet(QString const& who) { return QString{"hi "} + who; }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Calc calc;
  QMetaObject const* mo = calc.metaObject();

  // methods: signals, then slots (the full row and its clone), then invokables
  int const first = mo->methodOffset();
  CHECK(mo->methodCount() - first == kMethodCount);
  CHECK(mo->method(first).methodType() == QMetaMethod::Signal);
  CHECK(mo->method(first + 1).methodSignature() == "setLevel(int,int)");
  CHECK(mo->method(first + 2).methodSignature() == "setLevel(int)");
  CHECK(mo->method(first + 2).attributes() & QMetaMethod::Cloned);
  CHECK(mo->method(first + 3).methodSignature() == "twice(int)");
  CHECK(mo->method(first + 4).methodSignature() == "greet(QString)");
  CHECK(mo->method(first + 1).parameterNames() == (QList<QByteArray>{"newLevel", "newStep"}));
  CHECK(mo->method(first + 4).parameterNames() == (QList<QByteArray>{"who"}));
  CHECK(mo->method(first + 4).returnMetaType() == QMetaType::fromType<QString>());

  // invoke by name: full row, cloned row (the default argument applies), const method with a result
  CHECK(QMetaObject::invokeMethod(&calc, "setLevel", Q_ARG(int, kLevel), Q_ARG(int, kStepGiven)));
  CHECK(calc.level == kLevel && calc.step == kStepGiven);
  CHECK(QMetaObject::invokeMethod(&calc, "setLevel", Q_ARG(int, kLevel + 1)));
  CHECK(calc.level == kLevel + 1 && calc.step == kStepDefault);

  int doubled = 0;
  CHECK(QMetaObject::invokeMethod(&calc, "twice", Q_RETURN_ARG(int, doubled), Q_ARG(int, kOperand)));
  CHECK(doubled == 2 * kOperand);

  QString greeting;
  CHECK(QMetaObject::invokeMethod(&calc, "greet", Q_RETURN_ARG(QString, greeting), Q_ARG(QString, "Qt")));
  CHECK(greeting == "hi Qt");

  CHECK(QMetaObject::invokeMethod(&calc, "changed"));
  CHECK(!QMetaObject::invokeMethod(&calc, "noSuchMethod"));
  return rqt_test::finish("capability_invoke_by_name_real_qt");
}
