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
#include <reflect_moc/qt/qt.hpp>

class [[=rqt::classinfo{"QML.Element", "anonymous"}]] BirthdayPartyAttached : public rqt::Object<QObject>
{
    QML_ANONYMOUS
public:
    static QMetaObject const &staticMetaObject;
    template <class... Args> explicit BirthdayPartyAttached(Args &&...args) : rqt::Object<QObject>(std::forward<Args>(args)...) { bind(); }

    [[=rqt::property{.write = "setRsvp", .notify = "rsvpChanged"}]] QDate rsvp() const;
    void setRsvp(QDate);

public:
    [[=rqt::signal]] void rsvpChanged() { rqt::emit{this}(); }

private:
    QDate m_rsvp;
};
inline RQT_STATIC_META_OBJECT(BirthdayPartyAttached);

class [[=rqt::classinfo{"DefaultProperty", "guests"}]] [[=rqt::classinfo{"QML.Element", "auto"}]] [[=rqt::classinfo{"QML.Attached", "BirthdayPartyAttached"}]] BirthdayParty : public rqt::Object<QObject>
{
    QML_ELEMENT
    QML_ATTACHED(BirthdayPartyAttached)
public:
    static QMetaObject const &staticMetaObject;
    template <class... Args> explicit BirthdayParty(Args &&...args) : rqt::Object<QObject>(std::forward<Args>(args)...) { bind(); }

    [[=rqt::property{.write = "setHost", .notify = "hostChanged"}]] Person *host() const;
    void setHost(Person *);

    [[=rqt::property{.write = "setAnnouncement", .notify = "announcementChanged"}]] QString announcement() const;
    void setAnnouncement(const QString &);

    [[=rqt::property{.notify = "guestsChanged"}]] QQmlListProperty<Person> guests();
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
inline RQT_STATIC_META_OBJECT(BirthdayParty);

#endif // BIRTHDAYPARTY_H
