// Fixture: a BINDABLE property, which the migrator carries as the library's BINDABLE / .bindable.
#ifndef METER_H
#define METER_H

#include <QObject>
#include <QProperty>

class Meter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int level READ level WRITE setLevel NOTIFY levelChanged BINDABLE bindableLevel)
public:
    using QObject::QObject;

    int level() const { return m_level.value(); }
    void setLevel(int v) { m_level = v; }
    QBindable<int> bindableLevel() { return QBindable<int>(&m_level); }

signals:
    void levelChanged(int level);

private:
    QProperty<int> m_level{0};
};

#endif
