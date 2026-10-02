// A QML Connections element receives a static signal, and the handler reads the argument by the NAME
// given with [[= rqt::names]] (a function type has no parameter names). A QML-side write to a property
// whose NOTIFY is a static signal reaches the handler too.
#include "check.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QVariant>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>

#include <reflect_moc/compat.hpp>

#include <memory>

namespace {
constexpr int kStart = 42;
constexpr int kFromQml = 99;
constexpr int kNone = -1;
}  // namespace

class Counter : public QObject {
  Q_OBJECT
  Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)

 public:
  explicit Counter(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }

 public slots:
  [[= rqt::slot]] void setValue(int v) {
    if (v == value_) return;
    value_ = v;
    emit valueChanged(v);
  }

 signals:
  [[= rqt::names("value")]] static inline rqt::static_signal<void(int)> valueChanged{};

 private:
  int value_ = 0;
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  QQmlEngine engine;
  Counter counter;
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
    }
  )",
                    QUrl{});
  std::unique_ptr<QObject> root{component.create()};
  if (!root) std::fprintf(stderr, "QML errors: %s\n", component.errorString().toUtf8().constData());
  CHECK(root);
  if (!root) return rqt_test::finish("capability_static_signal_qml");

  counter.setValue(kStart);
  CHECK(root->property("seen").toInt() == kStart);
  CHECK(root->property("last").toInt() == kStart);
  CHECK(root->property("changes").toInt() == 1);

  CHECK(QMetaObject::invokeMethod(root.get(), "bump"));
  CHECK(counter.value() == kFromQml);
  CHECK(root->property("seen").toInt() == kFromQml);
  CHECK(root->property("last").toInt() == kFromQml);
  CHECK(root->property("changes").toInt() == 2);
  CHECK(root->property("last").toInt() != kNone);
  return rqt_test::finish("capability_static_signal_qml");
}
