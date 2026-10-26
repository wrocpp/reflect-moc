#pragma once
#include <QObject>
#include <QtPlugin>
class IDemo { public: virtual ~IDemo() = default; virtual int answer() const = 0; };
#define IDemo_iid "org.example.IDemo/1.0"
Q_DECLARE_INTERFACE(IDemo, IDemo_iid)
class EdgePlugin : public QObject, public IDemo {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID IDemo_iid FILE JSON_FILE)
  Q_INTERFACES(IDemo)
public:
  int answer() const override { return 42; }
};
