#pragma once
#include "plugin_iface.h"
class DemoPlugin : public QObject, public IDemo {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID IDemo_iid FILE "meta.json")
  Q_INTERFACES(IDemo)
public:
  int answer() const override { return 42; }
};
