#pragma once

#include <QList>
#include <QObject>
#include <QVariant>

namespace obs {

// Holds no data: only the signals of section 3.2.
class MatrixSignals : public QObject {
    Q_OBJECT
public:
    explicit MatrixSignals(QObject * parent = nullptr) : QObject(parent) {}

signals:
    void rowAboutToBeAdded(const QVariant & key, qsizetype index);
    void rowAdded(const QVariant & key, qsizetype index);
    void columnAboutToBeAdded(const QVariant & key, qsizetype index);
    void columnAdded(const QVariant & key, qsizetype index);
    void rowsAboutToBeAdded(qsizetype first, qsizetype last, const QList<QVariant> & keys);
    void rowsAdded(qsizetype first, qsizetype last, const QList<QVariant> & keys);
    void columnsAboutToBeAdded(qsizetype first, qsizetype last, const QList<QVariant> & keys);
    void columnsAdded(qsizetype first, qsizetype last, const QList<QVariant> & keys);
    void rowAboutToBeRemoved(const QVariant & key, qsizetype index);
    void rowRemoved(const QVariant & key, qsizetype index);
    void columnAboutToBeRemoved(const QVariant & key, qsizetype index);
    void columnRemoved(const QVariant & key, qsizetype index);
    void rowsAboutToBeRemoved(qsizetype first, qsizetype last, const QList<QVariant> & keys);
    void rowsRemoved(qsizetype first, qsizetype last, const QList<QVariant> & keys);
    void columnsAboutToBeRemoved(qsizetype first, qsizetype last, const QList<QVariant> & keys);
    void columnsRemoved(qsizetype first, qsizetype last, const QList<QVariant> & keys);
    void rowAboutToBeRenamed(const QVariant & oldKey, const QVariant & newKey, qsizetype index);
    void rowRenamed(const QVariant & oldKey, const QVariant & newKey, qsizetype index);
    void columnAboutToBeRenamed(const QVariant & oldKey, const QVariant & newKey, qsizetype index);
    void columnRenamed(const QVariant & oldKey, const QVariant & newKey, qsizetype index);
    void cellAboutToChange(const QVariant & rowKey, const QVariant & columnKey, qsizetype row, qsizetype column);
    void cellChanged(const QVariant & rowKey, const QVariant & columnKey, qsizetype row, qsizetype column);
    void cellsAboutToChange(qsizetype top, qsizetype left, qsizetype bottom, qsizetype right);
    void cellsChanged(qsizetype top, qsizetype left, qsizetype bottom, qsizetype right);
    void aboutToBeCleared();
    void cleared();
};

}  // namespace obs
