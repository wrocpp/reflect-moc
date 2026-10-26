// The oracle: the plugin class as moc reads it. JSON_FILE is the file under test, set by the target (CMake). The tests
// include the moc output of this header, so that the CBOR moc wrote (qt_pluginMetaDataV2_DemoPlugin) can be compared byte
// for byte with the CBOR reflect-moc writes for the same file.
#pragma once

#include "plugin_iface.h"

namespace viamoc {
class DemoPlugin : public QObject, public IDemo {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID IDemo_iid FILE JSON_FILE)
  Q_INTERFACES(IDemo)

 public:
  int answer() const override { return 42; }
};
}  // namespace viamoc
