// the same plugin through reflect-moc: no moc run; the metadata comes from #embed + the constexpr encoder, the entry points from Qt's own macro
#define QT_NO_KEYWORDS
#include <reflect_moc/qt/qt.hpp>
#include "plugin_iface.h"
#include "json_to_cbor.hpp"

class DemoPlugin : public QObject, public IDemo {
  RQT_OBJECT
  Q_INTERFACES(IDemo)
 public:
  int answer() const override { return 42; }
};

static constexpr unsigned char meta_json[] = {
#embed "meta.json"
};
static constexpr auto rqt_plugin_blob = SPIKE_CBOR(meta_json, IDemo_iid, "DemoPlugin");
static constexpr auto& qt_pluginMetaDataV2_DemoPlugin = rqt_plugin_blob.bytes;
QT_MOC_EXPORT_PLUGIN_V2(DemoPlugin, DemoPlugin, qt_pluginMetaDataV2_DemoPlugin)
