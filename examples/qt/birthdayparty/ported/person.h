// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef PERSON_H
#define PERSON_H

#include <QtQml/qqml.h>
#include <QColor>
#include <QObject>
#include <reflect_moc/qt.hpp>

class [[=rqt::classinfo{"QML.Element", "anonymous"}]] ShoeDescription : public rqt::Object<QObject>
{
    QML_ANONYMOUS
public:
    using rqt::Object<QObject>::Object;

    [[=rqt::property{.write = "setSize", .notify = "shoeChanged", .final = true}]] int size() const;
    void setSize(int);

    [[=rqt::property{.write = "setColor", .notify = "shoeChanged", .final = true}]] QColor color() const;
    void setColor(const QColor &);

    [[=rqt::property{.write = "setBrand", .notify = "shoeChanged", .final = true}]] QString brand() const;
    void setBrand(const QString &);

    [[=rqt::property{.write = "setPrice", .notify = "shoeChanged", .final = true}]] qreal price() const;
    void setPrice(qreal);

    friend bool operator==(const ShoeDescription &lhs, const ShoeDescription &rhs)
    {
        return operatorEqualsImpl(lhs, rhs);
    }
    friend bool operator!=(const ShoeDescription &lhs, const ShoeDescription &rhs)
    {
        return !operatorEqualsImpl(lhs, rhs);
    }

public:
    [[=rqt::signal]] void shoeChanged() { rqt::emit{this}(); }

private:
    static bool operatorEqualsImpl(const ShoeDescription &, const ShoeDescription &);

    int m_size = 0;
    QColor m_color;
    QString m_brand;
    qreal m_price = 0;
};

class [[=rqt::classinfo{"QML.Element", "auto"}]] [[=rqt::classinfo{"QML.Creatable", "false"}]] [[=rqt::classinfo{"QML.UncreatableReason", "Person is an abstract base class."}]] Person : public rqt::Object<QObject>
{
    QML_ELEMENT
    QML_UNCREATABLE("Person is an abstract base class.")
public:
    using rqt::Object<QObject>::Object;

    Person(QObject *parent = nullptr);

    [[=rqt::property{.write = "setName", .notify = "nameChanged", .final = true}]] QString name() const;
    void setName(const QString &);

    [[=rqt::property{.write = "setShoe", .notify = "shoeChanged", .final = true}]] ShoeDescription *shoe() const;
    void setShoe(ShoeDescription *shoe);

public:
    [[=rqt::signal]] void nameChanged() { rqt::emit{this}(); }
    [[=rqt::signal]] void shoeChanged() { rqt::emit{this}(); }

private:
    QString m_name;
    ShoeDescription *m_shoe = nullptr;
};

class [[=rqt::classinfo{"QML.Element", "auto"}]] Boy : public rqt::Object<Person>
{
    QML_ELEMENT
public:
    using rqt::Object<Person>::Object;
};

class [[=rqt::classinfo{"QML.Element", "auto"}]] Girl : public rqt::Object<Person>
{
    QML_ELEMENT
public:
    using rqt::Object<Person>::Object;
};

#endif // PERSON_H
