// Must not compile: see the message the test expects (tests/qt/plugin/CMakeLists.txt). docs/moc-parity.md: E-01.
#include "plugin_iface.h"

#include <reflect_moc/qt/plugin.hpp>

class DemoPlugin : public QObject, public IDemo {
 public:
  int answer() const override { return 42; }
};

static constexpr unsigned char demo_json[] = {
#embed "limit_plugin_json_lone_high_surrogate.json"
};
RQT_PLUGIN_METADATA(DemoPlugin, IDemo_iid, demo_json)
