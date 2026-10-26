// A plugin built by moc and the same plugin built by reflect-moc (meta.json, one class name, two namespaces), both read by
// QPluginLoader: the same metadata object, the same bytes behind qt_plugin_query_metadata_v2 (the ELF note payload), the same
// answers through qobject_cast. docs/moc-parity.md: E-01.
#include "check.hpp"
#include "plugin_iface.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonObject>
#include <QtCore/QLibrary>
#include <QtCore/QPluginLoader>

#include <cstring>

namespace {
struct loaded {
  QPluginLoader loader;
  explicit loaded(char const* path) : loader{QString::fromUtf8(path)} {}
};

QPluginMetaData raw_metadata(char const* path) {
  QLibrary library{QString::fromUtf8(path)};
  auto const query = reinterpret_cast<QtPluginMetaDataFunction>(library.resolve("qt_plugin_query_metadata_v2"));
  CHECK(query != nullptr);
  return query ? query() : QPluginMetaData{nullptr, 0};
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  loaded moc{RQT_PLUGIN_TWIN_MOC_PATH};
  loaded rqt{RQT_PLUGIN_DEMO_NAMESPACED_PATH};

  QJsonObject const moc_md = moc.loader.metaData();
  QJsonObject const rqt_md = rqt.loader.metaData();
  CHECK(!moc_md.isEmpty());
  CHECK(moc_md == rqt_md);
  CHECK(moc_md["IID"].toString() == IDemo_iid);

  QPluginMetaData const moc_raw = raw_metadata(RQT_PLUGIN_TWIN_MOC_PATH);
  QPluginMetaData const rqt_raw = raw_metadata(RQT_PLUGIN_DEMO_NAMESPACED_PATH);
  CHECK(moc_raw.size == rqt_raw.size);
  CHECK(moc_raw.size == rqt_raw.size && std::memcmp(moc_raw.data, rqt_raw.data, moc_raw.size) == 0);

  QObject* const from_moc = moc.loader.instance();
  QObject* const from_rqt = rqt.loader.instance();
  CHECK(from_moc != nullptr);
  CHECK(from_rqt != nullptr);
  if (from_moc && from_rqt) {
    CHECK(qobject_cast<IDemo*>(from_moc) != nullptr);
    CHECK(qobject_cast<IDemo*>(from_rqt) != nullptr);
    CHECK(qobject_cast<IDemo*>(from_moc)->answer() == qobject_cast<IDemo*>(from_rqt)->answer());
    CHECK(QString::fromLatin1(from_moc->metaObject()->className()) == "viamoc::DemoPlugin");
    CHECK(QString::fromLatin1(from_rqt->metaObject()->className()) == "viarqt::DemoPlugin");
  }
  return rqt_test::finish("parity_plugin_loader");
}
