#include "counter.h"

#include <QStringList>

Counter::Counter(int start, QObject *parent)
    : QObject(parent), m_value(start)
{
}

int Counter::add(int n)
{
    setValue(m_value + n);
    Q_EMIT moved(m_value - n, m_value);
    return m_value;
}

QString Counter::describe() const
{
    // emit nothing here: comments are left alone
    QStringList parts{"emit ", tr("value")};
    foreach (const QString &p, parts)
        (void)p;
    return parts.join(QString());
}

void Counter::clear()
{
    forever {
        emit labelChanged(QString());
        break;
    }
}

void Counter::reset() { setValue(0); }
void Counter::tick() {}
bool Counter::isEnabled() const { return true; }
double Counter::ratio() const { return 0.5; }
void Counter::setRatio(double) {}

#include "moc_counter.cpp"
