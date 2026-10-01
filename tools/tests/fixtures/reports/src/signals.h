// Fixture: signals the tool migrates only partly, or not at all.
#pragma once
#include <QObject>

class Emitter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int hidden READ hidden DESIGNABLE false)
    Q_PROPERTY(int inherited READ baseValue)
public:
    int hidden() const;
public slots:
    void configure(int level, bool verbose = false);
signals:
    void progress(int percent, const QString &text = QString());
    void changed(int);
    void changed(const QString &);
};
