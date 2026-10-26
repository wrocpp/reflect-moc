// The twin of parity_plugin_twin_moc.cpp built by reflect-moc: the same class name in another namespace, the same file.
#include "plugin_iface.h"

#include <reflect_moc/qt/plugin.hpp>
#include <reflect_moc/qt/qt.hpp>

namespace viarqt {
class DemoPlugin : public QObject, public IDemo {
  RQT_OBJECT
  Q_INTERFACES(IDemo)

 public:
  int answer() const override { return 42; }
};
}  // namespace viarqt

static constexpr unsigned char demo_json[] = {
#embed "meta.json"
};
RQT_PLUGIN_METADATA_AS(viarqt::DemoPlugin, DemoPlugin, IDemo_iid, demo_json)
