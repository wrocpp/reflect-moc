// obs::MatrixTableModel<M>: a QAbstractTableModel wired to an obs::Matrix (SPEC 8.4).
// It adds no signals, so it has no RQT_OBJECT and no moc input.
#pragma once

#include "matrix.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QModelIndex>
#include <QtCore/QVariant>

namespace obs {

template <class M>
class MatrixTableModel : public QAbstractTableModel {
public:
    explicit MatrixTableModel(M & source, QObject * parent = nullptr) : QAbstractTableModel(parent), source_(&source)
    {
        using RowKey = typename M::RowKeyType;
        using ColKey = typename M::ColKeyType;

        // structure: begin on the "about", end on the twin
        QObject::connect(&source, &M::rowAboutToBeAdded, this, [this](const RowKey &, qsizetype i) { beginInsertRows({}, int(i), int(i)); });
        QObject::connect(&source, &M::rowAdded, this, [this] { endInsertRows(); });
        QObject::connect(&source, &M::rowsAboutToBeAdded, this, [this](qsizetype f, qsizetype l, const QList<RowKey> &) { beginInsertRows({}, int(f), int(l)); });
        QObject::connect(&source, &M::rowsAdded, this, [this] { endInsertRows(); });
        QObject::connect(&source, &M::rowAboutToBeRemoved, this, [this](const RowKey &, qsizetype i) { beginRemoveRows({}, int(i), int(i)); });
        QObject::connect(&source, &M::rowRemoved, this, [this] { endRemoveRows(); });
        QObject::connect(&source, &M::rowsAboutToBeRemoved, this, [this](qsizetype f, qsizetype l, const QList<RowKey> &) { beginRemoveRows({}, int(f), int(l)); });
        QObject::connect(&source, &M::rowsRemoved, this, [this] { endRemoveRows(); });

        QObject::connect(&source, &M::columnAboutToBeAdded, this, [this](const ColKey &, qsizetype j) { beginInsertColumns({}, int(j), int(j)); });
        QObject::connect(&source, &M::columnAdded, this, [this] { endInsertColumns(); });
        QObject::connect(&source, &M::columnsAboutToBeAdded, this, [this](qsizetype f, qsizetype l, const QList<ColKey> &) { beginInsertColumns({}, int(f), int(l)); });
        QObject::connect(&source, &M::columnsAdded, this, [this] { endInsertColumns(); });
        QObject::connect(&source, &M::columnAboutToBeRemoved, this, [this](const ColKey &, qsizetype j) { beginRemoveColumns({}, int(j), int(j)); });
        QObject::connect(&source, &M::columnRemoved, this, [this] { endRemoveColumns(); });
        QObject::connect(&source, &M::columnsAboutToBeRemoved, this, [this](qsizetype f, qsizetype l, const QList<ColKey> &) { beginRemoveColumns({}, int(f), int(l)); });
        QObject::connect(&source, &M::columnsRemoved, this, [this] { endRemoveColumns(); });

        // a rename is a header change, never a move, a remove plus insert, or a reset
        QObject::connect(&source, &M::rowRenamed, this, [this](const RowKey &, const RowKey &, qsizetype i) { headerDataChanged(Qt::Vertical, int(i), int(i)); });
        QObject::connect(&source, &M::columnRenamed, this, [this](const ColKey &, const ColKey &, qsizetype j) { headerDataChanged(Qt::Horizontal, int(j), int(j)); });

        QObject::connect(&source, &M::cellChanged, this, [this](const RowKey &, const ColKey &, qsizetype i, qsizetype j) {
            const QModelIndex at = index(int(i), int(j));
            dataChanged(at, at);
        });
        QObject::connect(&source, &M::cellsChanged, this, [this](qsizetype top, qsizetype left, qsizetype bottom, qsizetype right) {
            dataChanged(index(int(top), int(left)), index(int(bottom), int(right)));
        });

        QObject::connect(&source, &M::aboutToBeCleared, this, [this] { beginResetModel(); });
        QObject::connect(&source, &M::cleared, this, [this] { endResetModel(); });
    }

    int rowCount(const QModelIndex & parent = {}) const override { return parent.isValid() ? 0 : int(source_->rowCount()); }
    int columnCount(const QModelIndex & parent = {}) const override { return parent.isValid() ? 0 : int(source_->columnCount()); }

    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override
    {
        if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
            return {};
        return cellData(source_->valueAt(index.row(), index.column()), role);
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (role != Qt::DisplayRole || section < 0)
            return {};
        if (orientation == Qt::Vertical)
            return section < source_->rowCount() ? QVariant::fromValue(source_->rowKeys()[section]) : QVariant{};
        return section < source_->columnCount() ? QVariant::fromValue(source_->columnKeys()[section]) : QVariant{};
    }

protected:
    virtual QVariant cellData(const typename M::ValueType & value, int role) const
    {
        if (role != Qt::DisplayRole || source_->isUninitialized(value))
            return {};
        return QVariant::fromValue(value);
    }

private:
    M * source_;
};

}  // namespace obs
