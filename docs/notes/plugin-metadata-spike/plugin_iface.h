#pragma once
#include <QtCore/QObject>
#include <QtCore/QtPlugin>
class IDemo { public: virtual ~IDemo() = default; virtual int answer() const = 0; };
#define IDemo_iid "org.example.IDemo/1.0"
Q_DECLARE_INTERFACE(IDemo, IDemo_iid)
