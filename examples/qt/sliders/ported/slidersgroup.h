// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef SLIDERSGROUP_H
#define SLIDERSGROUP_H

#include <QGroupBox>
#include <reflect_moc/qt/qt.hpp>

QT_BEGIN_NAMESPACE
class QDial;
class QScrollBar;
class QSlider;
class QBoxLayout;
QT_END_NAMESPACE

//! [0]
class SlidersGroup : public rqt::Object<QGroupBox>
{

public:
    static QMetaObject const &staticMetaObject;
    SlidersGroup(const QString &title, QWidget *parent = nullptr);

public:
    [[=rqt::signal]] void valueChanged(int value) { rqt::emit{this}(value); }

public:
    [[=rqt::slot]] void setValue(int value);
    [[=rqt::slot]] void setMinimum(int value);
    [[=rqt::slot]] void setMaximum(int value);
    [[=rqt::slot]] void invertAppearance(bool invert);
    [[=rqt::slot]] void invertKeyBindings(bool invert);
    [[=rqt::slot]] void setOrientation(Qt::Orientation orientation);

private:
    QSlider *slider;
    QScrollBar *scrollBar;
    QDial *dial;
    QBoxLayout *slidersLayout;
};
RQT_STATIC_META_OBJECT(SlidersGroup);
//! [0]

#endif
