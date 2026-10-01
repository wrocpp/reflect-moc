// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef HAPPYBIRTHDAYSONG_H
#define HAPPYBIRTHDAYSONG_H

#include <QQmlProperty>
#include <QQmlPropertyValueSource>
#include <qqml.h>
#include <QStringList>
#include <reflect_moc/qt/qt.hpp>

class [[=rqt::classinfo{"QML.Element", "auto"}]] HappyBirthdaySong : public rqt::Object<QObject>, public QQmlPropertyValueSource
{
    QML_ELEMENT
public:
    static QMetaObject const &staticMetaObject;
    void *qt_metacast(const char *name) override
    {
        if (void *found = rqt::Object<QObject>::qt_metacast(name))
            return found;
        if (!qstrcmp(name, qobject_interface_iid<QQmlPropertyValueSource *>()))
            return static_cast<QQmlPropertyValueSource *>(this);
        return nullptr;
    }
    explicit HappyBirthdaySong(QObject *parent = nullptr);

    void setTarget(const QQmlProperty &) override;

    [[=rqt::property{.write = "setName", .notify = "nameChanged", .final = true}]] QString name() const;
    void setName(const QString &);

public:
    [[=rqt::signal]] void nameChanged() { rqt::emit{this}(); }

private:
    [[=rqt::slot]] void advance();

private:
    qsizetype m_line = -1;
    QStringList m_lyrics;
    QQmlProperty m_target;
    QString m_name;
};
RQT_STATIC_META_OBJECT(HappyBirthdaySong);

#endif // HAPPYBIRTHDAYSONG_H
