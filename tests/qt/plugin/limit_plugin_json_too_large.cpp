// Must not compile: the file is larger than RQT_PLUGIN_JSON_MAX_BYTES. The file is made at configure time
// (tests/qt/plugin/CMakeLists.txt). docs/moc-parity.md: E-01.
#include "plugin_iface.h"

#include <reflect_moc/qt/plugin.hpp>

class DemoPlugin : public QObject, public IDemo {
 public:
  int answer() const override { return 42; }
};

static constexpr unsigned char demo_json[] = {
#embed "rqt_plugin_too_large.json"
};
RQT_PLUGIN_METADATA(DemoPlugin, IDemo_iid, demo_json)
