// A plugin built without moc, loaded by QPluginLoader as a host would: the metadata read from the file before it is loaded
// (the ELF note), the same after, the instance, qobject_cast to the interface, and the class in a namespace. Nothing here
// needs moc. docs/moc-parity.md: E-01.
#include "check.hpp"
#include "plugin_iface.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QPluginLoader>

static QJsonObject expected_meta() {
  return QJsonObject{{"Keys", QJsonArray{"alpha", "beta"}},
                     {"Version", 3},
                     {"Enabled", true},
                     {"Name", "demo"},
                     {"Ratio", 1.5},
                     {"Nested", QJsonObject{{"a", QJsonValue::Null}, {"b", -7}}}};
}

static void check_plugin(char const* path, char const* class_name) {
  QPluginLoader loader{QString::fromUtf8(path)};
  CHECK(!loader.isLoaded());
  QJsonObject const md = loader.metaData();  // from the file: nothing is loaded yet
  CHECK(!loader.isLoaded());
  CHECK(md["IID"].toString() == IDemo_iid);
  CHECK(md["className"].toString() == class_name);
  CHECK(md["MetaData"].toObject() == expected_meta());

  QObject* const instance = loader.instance();
  CHECK(instance != nullptr);
  if (!instance) {
    std::fprintf(stderr, "%s: %s\n", path, qPrintable(loader.errorString()));
    return;
  }
  CHECK(loader.metaData() == md);
  auto* const demo = qobject_cast<IDemo*>(instance);
  CHECK(demo != nullptr);
  CHECK(demo && demo->answer() == 42);
  CHECK(instance->qt_metacast(IDemo_iid) == static_cast<IDemo*>(demo));
  CHECK(loader.instance() == instance);
  CHECK(loader.unload());
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  check_plugin(RQT_PLUGIN_DEMO_PATH, "DemoPlugin");
  check_plugin(RQT_PLUGIN_DEMO_NAMESPACED_PATH, "DemoPlugin");
  return rqt_test::finish("capability_plugin_metadata");
}
