#pragma once

#include <QList>
#include <QObject>
#include <QVariant>

namespace obs {

// Holds no data: only the signals of section 3.1, so that the Container
// template can inherit them.
class ContainerSignals : public QObject {
    Q_OBJECT
public:
    explicit ContainerSignals(QObject * parent = nullptr) : QObject(parent) {}

signals:
    void entryAboutToBeAdded(const QVariant & key);
    void entryAdded(const QVariant & key);
    void entryAboutToBeRemoved(const QVariant & key);
    void entryRemoved(const QVariant & key);
    void entryDataAboutToChange(const QVariant & key, const QList<int> & roles);
    void entryDataChanged(const QVariant & key, const QList<int> & roles);
    void entriesAboutToBeAdded(const QList<QVariant> & keys);
    void entriesAdded(const QList<QVariant> & keys);
    void entriesAboutToBeRemoved(const QList<QVariant> & keys);
    void entriesRemoved(const QList<QVariant> & keys);
    void aboutToBeCleared();
    void cleared();
    void aboutToBeReset();
    void resetCompleted();
};

// f() after every structural change, never after a data change or an "about".
template <class F>
void onStructureChanged(const ContainerSignals * source, const QObject * context, F f)
{
    using CS = ContainerSignals;
    QObject::connect(source, &CS::entryAdded, context, [f](const QVariant &) mutable { f(); });
    QObject::connect(source, &CS::entryRemoved, context, [f](const QVariant &) mutable { f(); });
    QObject::connect(source, &CS::entriesAdded, context, [f](const QList<QVariant> &) mutable { f(); });
    QObject::connect(source, &CS::entriesRemoved, context, [f](const QList<QVariant> &) mutable { f(); });
    QObject::connect(source, &CS::cleared, context, [f]() mutable { f(); });
    QObject::connect(source, &CS::resetCompleted, context, [f]() mutable { f(); });
}

// f(keys) after entryAdded (one key) and entriesAdded (the batch).
template <class F>
void onKeysAdded(const ContainerSignals * source, const QObject * context, F f)
{
    using CS = ContainerSignals;
    QObject::connect(source, &CS::entryAdded, context,
                     [f](const QVariant & key) mutable { f(QList<QVariant>{key}); });
    QObject::connect(source, &CS::entriesAdded, context,
                     [f](const QList<QVariant> & keys) mutable { f(keys); });
}

}  // namespace obs
