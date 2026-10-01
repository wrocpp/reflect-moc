// Fixture: the QML_ELEMENT family.
#pragma once
#include <QObject>
#include <QtQml/qqmlregistration.h>

class Plain : public QObject
{
    Q_OBJECT
    QML_ELEMENT
};

class Renamed : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Fancy)
};

class Hidden : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
};

class Abstract : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Abstract \"base\"")
};

class Lonely : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
};
