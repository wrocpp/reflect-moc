// The same plugin linked into the executable (QT_STATICPLUGIN, Q_IMPORT_PLUGIN): Qt lists it among the static plugins with
// the metadata of the file, and its instance answers. docs/moc-parity.md: E-01.
#include "check.hpp"
#include "plugin_iface.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonObject>
#include <QtCore/QPluginLoader>

Q_IMPORT_PLUGIN(DemoPlugin)

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  auto const plugins = QPluginLoader::staticPlugins();
  CHECK(plugins.size() == 1);
  if (plugins.size() == 1) {
    QJsonObject const md = plugins[0].metaData();
    CHECK(md["IID"].toString() == IDemo_iid);
    CHECK(md["className"].toString() == "DemoPlugin");
    CHECK(md["MetaData"].toObject()["Name"].toString() == "demo");
    CHECK(md["MetaData"].toObject()["Version"].toInt() == 3);
    auto* const demo = qobject_cast<IDemo*>(plugins[0].instance());
    CHECK(demo != nullptr);
    CHECK(demo && demo->answer() == 42);
  }
  return rqt_test::finish("capability_plugin_metadata_static");
}
