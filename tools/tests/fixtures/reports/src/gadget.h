// Fixture: Q_GADGET, Q_INTERFACES and an invokable constructor.
#pragma once
#include <QObject>

class Shape
{
public:
    virtual ~Shape() = default;
};
#define Shape_iid "org.example.Shape"
Q_DECLARE_INTERFACE(Shape, Shape_iid)

struct Point
{
    Q_GADGET
    Q_PROPERTY(int x MEMBER x)
public:
    int x = 0;
};

class Circle : public QObject, public Shape
{
    Q_OBJECT
    Q_INTERFACES(Shape)
public:
    Q_INVOKABLE explicit Circle(QObject *parent = nullptr);
};
