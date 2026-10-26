// A plugin as reflect-moc builds it: no moc run. Shared by the shared-library plugin and the static one
// (capability_plugin_metadata, capability_plugin_metadata_static). The class is in the global namespace.
#include "plugin_iface.h"

#include <reflect_moc/qt/plugin.hpp>
#include <reflect_moc/qt/qt.hpp>

class DemoPlugin : public QObject, public IDemo {
  RQT_OBJECT
  Q_INTERFACES(IDemo)

 public:
  int answer() const override { return 42; }
};

static constexpr unsigned char demo_json[] = {
#embed "meta.json"
};
RQT_PLUGIN_METADATA(DemoPlugin, IDemo_iid, demo_json)
