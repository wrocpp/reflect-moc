// Fixture: Q_PRIVATE_SLOT, Q_PLUGIN_METADATA, Q_REVISION and an unknown macro.
#pragma once
#include <QObject>

class PluginLike : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.example.Fixture")
    Q_PRIVATE_SLOT(d_func(), void _q_done())
    Q_FIXTURE_UNKNOWN
public:
    Q_REVISION(2) Q_INVOKABLE void later();
    void *d_func();
};
