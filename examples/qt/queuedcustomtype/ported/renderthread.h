// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef RENDERTHREAD_H
#define RENDERTHREAD_H

#include <QImage>
#include <QThread>
#include <reflect_moc/qt.hpp>

class Block;

//! [RenderThread class definition]
class RenderThread : public rqt::Object<QThread>
{

public:
    RenderThread(QObject *parent = nullptr);
    ~RenderThread();

    void processImage(const QImage &image);

public:
    [[=rqt::signal]] void sendBlock(const Block &block) { rqt::emit{this}(block); }

protected:
    void run();

private:
    QImage m_image;
};
//! [RenderThread class definition]

#endif
