// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef BIRTHDAYPARTY_H
#define BIRTHDAYPARTY_H

#include "person.h"

#include <QDate>
#include <QDebug>
#include <QObject>
#include <QQmlListProperty>
#include <qqml.h>
#include <reflect_moc/qt.hpp>

class [[=rqt::classinfo{"QML.Element", "anonymous"}]] BirthdayPartyAttached : public rqt::Object<QObject>
{
    QML_ANONYMOUS
public:
    using rqt::Object<QObject>::Object;

    [[=rqt::property{.write = "setRsvp", .notify = "rsvpChanged", .final = true}]] QDate rsvp() const;
    void setRsvp(QDate);

public:
    [[=rqt::signal]] void rsvpChanged() { rqt::emit{this}(); }

private:
    QDate m_rsvp;
};

class [[=rqt::classinfo{"DefaultProperty", "guests"}]] [[=rqt::classinfo{"QML.Element", "auto"}]] [[=rqt::classinfo{"QML.Attached", "BirthdayPartyAttached"}]] BirthdayParty : public rqt::Object<QObject>
{
    QML_ELEMENT
    QML_ATTACHED(BirthdayPartyAttached)
public:
    using rqt::Object<QObject>::Object;

    [[=rqt::property{.write = "setHost", .notify = "hostChanged", .final = true}]] Person *host() const;
    void setHost(Person *);

    [[=rqt::property{.write = "setAnnouncement", .notify = "announcementChanged", .final = true}]] QString announcement() const;
    void setAnnouncement(const QString &);

    [[=rqt::property{.notify = "guestsChanged", .final = true}]] QQmlListProperty<Person> guests();
    void appendGuest(Person *);
    qsizetype guestCount() const;
    Person *guest(qsizetype) const;
    void clearGuests();
    void replaceGuest(qsizetype, Person *);
    void removeLastGuest();

    static BirthdayPartyAttached *qmlAttachedProperties(QObject *);

    void startParty();

public:
    [[=rqt::signal]] void hostChanged() { rqt::emit{this}(); }
    [[=rqt::signal]] void guestsChanged() { rqt::emit{this}(); }
    [[=rqt::signal]] void partyStarted(QDateTime time) { rqt::emit{this}(time); }
    [[=rqt::signal]] void announcementChanged() { rqt::emit{this}(); }

private:
    static void appendGuest(QQmlListProperty<Person> *, Person *);
    static qsizetype guestCount(QQmlListProperty<Person> *);
    static Person *guest(QQmlListProperty<Person> *, qsizetype);
    static void clearGuests(QQmlListProperty<Person> *);
    static void replaceGuest(QQmlListProperty<Person> *, qsizetype, Person *);
    static void removeLastGuest(QQmlListProperty<Person> *);

    Person *m_host = nullptr;
    QList<Person *> m_guests;
    QString m_announcement;
};

#endif // BIRTHDAYPARTY_H
