// Fixture: moc rejects a class template with Q_OBJECT.
#pragma once
#include <QObject>

template <class T>
class Holder : public QObject
{
    Q_OBJECT
public:
    T value;
};
