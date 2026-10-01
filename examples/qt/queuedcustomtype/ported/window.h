// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef WINDOW_H
#define WINDOW_H

#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QWidget>
#include <reflect_moc/qt/qt.hpp>
#include <QtCore/qcoreapplication.h>

class Block;
class RenderThread;

//! [Window class definition]
class Window : public rqt::Object<QWidget>
{
    Q_DECLARE_TR_FUNCTIONS(Window)

public:
    static QMetaObject const &staticMetaObject;
    Window(QWidget *parent = nullptr);
    void loadImage(const QImage &image);

public:
    [[=rqt::slot]] void addBlock(const Block &block);

private:
    [[=rqt::slot]] void loadImage();
    [[=rqt::slot]] void resetUi();

private:
    QLabel *label;
    QPixmap pixmap;
    QPushButton *loadButton;
    QPushButton *resetButton;
    QString path;
    RenderThread *thread;
};
RQT_STATIC_META_OBJECT(Window);
//! [Window class definition]

#endif
