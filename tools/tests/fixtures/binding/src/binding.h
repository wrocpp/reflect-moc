// Fixture: every constructor shape that needs bind(), and the tier B opt-in.
#pragma once
#include <QObject>

class Shape
{
public:
    virtual ~Shape() = default;
};
#define Shape_iid "org.example.Shape"
Q_DECLARE_INTERFACE(Shape, Shape_iid)

class Plain : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
};

class Child : public Plain
{
    Q_OBJECT
public:
    using Plain::Plain;
};

class Owner : public QObject
{
    Q_OBJECT
public:
    explicit Owner(QObject *parent = nullptr);
    Owner(int first, int second);
};

class Inline : public QObject
{
    Q_OBJECT
public:
    explicit Inline(QObject *parent = nullptr) : QObject(parent) {}
    Inline(int v) : QObject(nullptr), m_v{v}
    {
        m_v += 1;
    }
    Inline(const Inline &) = delete;

private:
    int m_v = 0;
};

class Defaulted : public QObject
{
    Q_OBJECT
public:
    Defaulted() = default;
};

// A class keyword with no constructor: the generated one must be public.
class Bare : public QObject
{
    Q_OBJECT
    int m_hidden = 0;
};

class Casted : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
};

class Linked : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
signals:
    void changed(int value);
};

class Meta : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
};

class Holder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Plain *plain READ plain CONSTANT)
public:
    using QObject::QObject;
    Plain *plain() const;
};

class Circle : public QObject, public Shape
{
    Q_OBJECT
    Q_INTERFACES(Shape)
public:
    using QObject::QObject;
};

class Quiet : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
};
