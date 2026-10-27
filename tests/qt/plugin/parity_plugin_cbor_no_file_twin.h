// The oracle for a plugin with no FILE: moc writes IID and className only, with no MetaData key.
#pragma once

#include "plugin_iface.h"

class DemoPlugin : public QObject, public IDemo {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID IDemo_iid)
  Q_INTERFACES(IDemo)

 public:
  int answer() const override { return 42; }
};
