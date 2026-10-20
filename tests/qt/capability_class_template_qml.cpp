// A specialization of a class template with typed signals, registered as a QML element: QML creates it, handles
// its signal by name with the key as its own type, and the C++ side changes it. The registration is
// qmlRegisterType<T> on the specialization, the call QML code generated from QML_ELEMENT would make.
#include "check.hpp"

#include <container_traits.hpp>
#include <observable_container.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QHash>
#include <QtCore/QVariant>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlEngine>

#include <memory>

namespace {
constexpr int kMajor = 1;
constexpr int kMinor = 0;
constexpr unsigned kKey = 41;
constexpr unsigned kSecondKey = 42;

struct Sample {
  int value = 0;
};
using Hashed = observable::ObservableContainer<QHash<unsigned, Sample>>;
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  CHECK(qmlRegisterType<Hashed>("rqt.test", kMajor, kMinor, "HashedContainer") >= 0);

  QQmlEngine engine;
  QQmlComponent component(&engine);
  component.setData(R"(
    import QtQml
    import rqt.test
    QtObject {
      id: root
      property int added: 0
      property int lastKey: -1
      property int cleared: 0
      property var container: HashedContainer {
        onEntryAdded: function(key) { root.added++; root.lastKey = key }
        onCleared: root.cleared++
      }
    }
  )",
                    QUrl{});
  std::unique_ptr<QObject> root{component.create()};
  if (!root) std::fprintf(stderr, "QML errors: %s\n", component.errorString().toUtf8().constData());
  CHECK(root);
  if (!root) return rqt_test::finish("capability_class_template_qml");

  auto* container = qobject_cast<Hashed*>(root->property("container").value<QObject*>());
  CHECK(container != nullptr);
  if (!container) return rqt_test::finish("capability_class_template_qml");

  CHECK(container->insert(kKey, Sample{1}));
  CHECK(root->property("added").toInt() == 1);
  CHECK(root->property("lastKey").toUInt() == kKey);
  CHECK(container->insert(kSecondKey, Sample{2}));
  CHECK(root->property("added").toInt() == 2);
  CHECK(root->property("lastKey").toUInt() == kSecondKey);
  CHECK(container->clear());
  CHECK(root->property("cleared").toInt() == 1);
  return rqt_test::finish("capability_class_template_qml");
}
