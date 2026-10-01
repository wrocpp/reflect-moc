// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef PERSON_H
#define PERSON_H

#include <QtQml/qqml.h>
#include <QColor>
#include <QObject>
#include <reflect_moc/qt/qt.hpp>

class [[=rqt::classinfo{"QML.Element", "anonymous"}]] ShoeDescription : public QObject
{
    RQT_OBJECT
    RQT_PROPERTY(int size READ size WRITE setSize NOTIFY shoeChanged FINAL)
    RQT_PROPERTY(QColor color READ color WRITE setColor NOTIFY shoeChanged FINAL)
    RQT_PROPERTY(QString brand READ brand WRITE setBrand NOTIFY shoeChanged FINAL)
    RQT_PROPERTY(qreal price READ price WRITE setPrice NOTIFY shoeChanged FINAL)
    QML_ANONYMOUS
public:
    using QObject::QObject;

    int size() const;
    void setSize(int);

    QColor color() const;
    void setColor(const QColor &);

    QString brand() const;
    void setBrand(const QString &);

    qreal price() const;
    void setPrice(qreal);

    friend bool operator==(const ShoeDescription &lhs, const ShoeDescription &rhs)
    {
        return operatorEqualsImpl(lhs, rhs);
    }
    friend bool operator!=(const ShoeDescription &lhs, const ShoeDescription &rhs)
    {
        return !operatorEqualsImpl(lhs, rhs);
    }

signals:
    rqt::signal<void()> shoeChanged;

private:
    static bool operatorEqualsImpl(const ShoeDescription &, const ShoeDescription &);

    int m_size = 0;
    QColor m_color;
    QString m_brand;
    qreal m_price = 0;
};

class [[=rqt::classinfo{"QML.Element", "auto"}]] [[=rqt::classinfo{"QML.Creatable", "false"}]] [[=rqt::classinfo{"QML.UncreatableReason", "Person is an abstract base class."}]] Person : public QObject
{
    RQT_OBJECT
    RQT_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged FINAL)
    RQT_PROPERTY(ShoeDescription *shoe READ shoe WRITE setShoe NOTIFY shoeChanged FINAL)
    QML_ELEMENT
    QML_UNCREATABLE("Person is an abstract base class.")
public:
    using QObject::QObject;

    Person(QObject *parent = nullptr);

    QString name() const;
    void setName(const QString &);

    ShoeDescription *shoe() const;
    void setShoe(ShoeDescription *shoe);

signals:
    rqt::signal<void()> nameChanged;
    rqt::signal<void()> shoeChanged;

private:
    QString m_name;
    ShoeDescription *m_shoe = nullptr;
};

class [[=rqt::classinfo{"QML.Element", "auto"}]] Boy : public Person
{
    RQT_OBJECT
    QML_ELEMENT
public:
    using Person::Person;
};

class [[=rqt::classinfo{"QML.Element", "auto"}]] Girl : public Person
{
    RQT_OBJECT
    QML_ELEMENT
public:
    using Person::Person;
};

#endif // PERSON_H
