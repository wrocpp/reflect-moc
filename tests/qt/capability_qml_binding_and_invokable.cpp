// A reflected object as a QML context property, with no binding line in the
// class (E4: rqt::register_namespace): a binding to a property, a signal
// handler that reads its argument by NAME, a write from QML and a call of an
// invokable.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QVariant>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>

#include <memory>

namespace {
constexpr int kStart = 42;
constexpr int kFromQml = 99;
constexpr int kAdded = 1;
}  // namespace

namespace app {
struct Counter : rqt::Object<> {
  [[= rqt::property{.write = "setValue", .notify = "valueChanged"}]] int value() const { return value_; }
  [[= rqt::signal_function]] void valueChanged(int value) { rqt::emit{this}(value); }
  [[= rqt::slot]] void setValue(int v) {
    if (v == value_) return;
    value_ = v;
    valueChanged(v);
  }
  [[= rqt::invokable]] int add(int n) {
    setValue(value_ + n);
    return value_;
  }
  int value_ = 0;
};
}  // namespace app

static bool const registered = rqt::register_namespace<^^app>();

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  CHECK(registered);

  QQmlEngine engine;
  app::Counter counter;
  engine.rootContext()->setContextProperty("counter", &counter);
  QQmlComponent component(&engine);
  component.setData(R"(
    import QtQml
    QtObject {
      id: root
      property int seen: counter.value
      property int last: -1
      property int changes: 0
      property var watcher: Connections {
        target: counter
        function onValueChanged(value) { root.changes++; root.last = value }
      }
      function bump() { counter.value = 99 }
      function callAdd(n) { return counter.add(n) }
    }
  )",
                    QUrl{});
  std::unique_ptr<QObject> root{component.create()};
  if (!root) std::fprintf(stderr, "QML errors: %s\n", component.errorString().toUtf8().constData());
  CHECK(root);
  if (!root) return rqt_test::finish("capability_qml_binding_and_invokable");

  counter.setValue(kStart);
  CHECK(root->property("seen").toInt() == kStart);
  CHECK(root->property("last").toInt() == kStart);
  CHECK(root->property("changes").toInt() == 1);

  CHECK(QMetaObject::invokeMethod(root.get(), "bump"));
  CHECK(counter.value_ == kFromQml);
  CHECK(root->property("seen").toInt() == kFromQml);

  QVariant result;
  CHECK(QMetaObject::invokeMethod(root.get(), "callAdd", Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, kAdded)));
  CHECK(result.toInt() == kFromQml + kAdded && counter.value_ == kFromQml + kAdded);
  return rqt_test::finish("capability_qml_binding_and_invokable");
}
