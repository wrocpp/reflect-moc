// Fixture: every construct rqt-migrate rewrites automatically.
#ifndef COUNTER_H
#define COUNTER_H

#include <QObject>
#include <QString>

class Counter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged RESET reset)
    Q_PROPERTY(bool enabled READ isEnabled CONSTANT)
    Q_PROPERTY(QString label MEMBER m_label NOTIFY labelChanged)
    Q_PROPERTY(double ratio
               READ ratio
               WRITE setRatio)
    Q_CLASSINFO("Author", "Fixture \"quoted\"")
public:
    enum class Mode { Up, Down };
    Q_ENUM(Mode)
    enum Option { None = 0, Wrap = 1, Clamp = 2 };
    Q_DECLARE_FLAGS(Options, Option)
    Q_FLAG(Options)

    using QObject::QObject;
    Counter(int start, QObject *parent);

    int value() const { return m_value; }
    bool isEnabled() const;
    double ratio() const;
    void setRatio(double);

    Q_INVOKABLE int add(int n);
    Q_INVOKABLE
    QString describe() const;
    Q_SLOT void clear();

signals:
    void valueChanged(int value);
    void labelChanged(const QString &);
    void moved(int from,
               int to);

public slots:
    void setValue(int v) { if (v != m_value) { m_value = v; emit valueChanged(v); } }
    void reset();

private Q_SLOTS:
    void tick();

private:
    int m_value = 0;
    QString m_label;
};

#endif
