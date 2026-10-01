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

class [[=rqt::classinfo{"QML.Element", "anonymous"}]] BirthdayPartyAttached : public QObject
{
    RQT_OBJECT
    RQT_PROPERTY(QDate rsvp READ rsvp WRITE setRsvp NOTIFY rsvpChanged FINAL)
    QML_ANONYMOUS
public:
    using QObject::QObject;

    QDate rsvp() const;
    void setRsvp(QDate);

signals:
    rqt::signal<void()> rsvpChanged;

private:
    QDate m_rsvp;
};

class [[=rqt::classinfo{"DefaultProperty", "guests"}]] [[=rqt::classinfo{"QML.Element", "auto"}]] [[=rqt::classinfo{"QML.Attached", "BirthdayPartyAttached"}]] BirthdayParty : public QObject
{
    RQT_OBJECT
    RQT_PROPERTY(Person *host READ host WRITE setHost NOTIFY hostChanged FINAL)
    RQT_PROPERTY(QQmlListProperty<Person> guests READ guests NOTIFY guestsChanged FINAL)
    RQT_PROPERTY(QString announcement READ announcement WRITE setAnnouncement NOTIFY announcementChanged FINAL)
    QML_ELEMENT
    QML_ATTACHED(BirthdayPartyAttached)
public:
    using QObject::QObject;

    Person *host() const;
    void setHost(Person *);

    QString announcement() const;
    void setAnnouncement(const QString &);

    QQmlListProperty<Person> guests();
    void appendGuest(Person *);
    qsizetype guestCount() const;
    Person *guest(qsizetype) const;
    void clearGuests();
    void replaceGuest(qsizetype, Person *);
    void removeLastGuest();

    static BirthdayPartyAttached *qmlAttachedProperties(QObject *);

    void startParty();

signals:
    rqt::signal<void()> hostChanged;
    rqt::signal<void()> guestsChanged;
    [[=rqt::names("time")]] rqt::signal<void(QDateTime)> partyStarted;
    rqt::signal<void()> announcementChanged;

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
