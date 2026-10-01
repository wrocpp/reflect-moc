// rqt::register_qml<T> makes a reflected class a QML element: QML creates it,
// sets its property, handles its signal by name and calls its slot.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QVariant>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>

#include <memory>

namespace {
constexpr int kInitial = 3;
constexpr int kBumped = 9;
constexpr int kMajor = 1;
constexpr int kMinor = 0;
}  // namespace

struct Dial : rqt::Object<> {
  static QMetaObject const& staticMetaObject;
  Dial() { bind(); }
  [[= rqt::property{.write = "setValue", .notify = "valueChanged"}]] int value() const { return value_; }
  [[= rqt::signal_function]] void valueChanged(int value) { rqt::emit{this}(value); }
  [[= rqt::slot]] void setValue(int v) {
    if (v == value_) return;
    value_ = v;
    valueChanged(v);
  }
  int value_ = 0;
};
RQT_STATIC_META_OBJECT(Dial);

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  CHECK(rqt::register_qml<Dial>("rqt.test", kMajor, kMinor, "Dial") >= 0);

  QQmlEngine engine;
  QQmlComponent component(&engine);
  component.setData(R"(
    import QtQml
    import rqt.test
    QtObject {
      id: root
      property int heard: -1
      property var dial: Dial {
        value: 3
        onValueChanged: function(value) { root.heard = value }
      }
      function bump() { dial.setValue(9) }
    }
  )",
                    QUrl{});
  std::unique_ptr<QObject> root{component.create()};
  if (!root) std::fprintf(stderr, "QML errors: %s\n", component.errorString().toUtf8().constData());
  CHECK(root);
  if (!root) return rqt_test::finish("capability_register_qml");

  auto* dial = rqt::cast<Dial>(root->property("dial").value<QObject*>());
  CHECK(dial != nullptr);
  if (!dial) return rqt_test::finish("capability_register_qml");
  CHECK(dial->value_ == kInitial);
  CHECK(QByteArray{dial->metaObject()->className()} == "Dial");

  CHECK(QMetaObject::invokeMethod(root.get(), "bump"));
  CHECK(dial->value_ == kBumped);
  CHECK(root->property("heard").toInt() == kBumped);
  return rqt_test::finish("capability_register_qml");
}
