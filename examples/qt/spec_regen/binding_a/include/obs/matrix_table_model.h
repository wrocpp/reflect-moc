#pragma once

#include "obs/matrix.h"

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QVariant>

namespace obs {

template <class M>
class MatrixTableModel : public QAbstractTableModel {
public:
    explicit MatrixTableModel(M & source, QObject * parent = nullptr)
        : QAbstractTableModel(parent), m_source(source)
    {
        connectStructure();
        connectContent();
    }

    int rowCount(const QModelIndex & parent = {}) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_source.rowCount());
    }

    int columnCount(const QModelIndex & parent = {}) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(m_source.columnCount());
    }

    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid())
            return {};
        return cellData(m_source.valueAt(index.row(), index.column()), role);
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (role != Qt::DisplayRole || section < 0)
            return {};
        if (orientation == Qt::Vertical) {
            if (section >= m_source.rowCount())
                return {};
            return QVariant::fromValue(m_source.rowKeys()[section]);
        }
        if (section >= m_source.columnCount())
            return {};
        return QVariant::fromValue(m_source.columnKeys()[section]);
    }

protected:
    virtual QVariant cellData(const typename M::ValueType & value, int role) const
    {
        if (role == Qt::DisplayRole && !m_source.isUninitialized(value))
            return QVariant::fromValue(value);
        return {};
    }

private:
    using MS = MatrixSignals;

    static int asInt(qsizetype i) { return static_cast<int>(i); }

    void connectStructure()
    {
        M * s = &m_source;
        connect(s, &MS::rowAboutToBeAdded, this,
                [this](const QVariant &, qsizetype i) { beginInsertRows({}, asInt(i), asInt(i)); });
        connect(s, &MS::rowAdded, this, [this](const QVariant &, qsizetype) { endInsertRows(); });
        connect(s, &MS::rowsAboutToBeAdded, this,
                [this](qsizetype f, qsizetype l, const QList<QVariant> &) { beginInsertRows({}, asInt(f), asInt(l)); });
        connect(s, &MS::rowsAdded, this, [this](qsizetype, qsizetype, const QList<QVariant> &) { endInsertRows(); });
        connect(s, &MS::rowAboutToBeRemoved, this,
                [this](const QVariant &, qsizetype i) { beginRemoveRows({}, asInt(i), asInt(i)); });
        connect(s, &MS::rowRemoved, this, [this](const QVariant &, qsizetype) { endRemoveRows(); });
        connect(s, &MS::rowsAboutToBeRemoved, this,
                [this](qsizetype f, qsizetype l, const QList<QVariant> &) { beginRemoveRows({}, asInt(f), asInt(l)); });
        connect(s, &MS::rowsRemoved, this, [this](qsizetype, qsizetype, const QList<QVariant> &) { endRemoveRows(); });

        connect(s, &MS::columnAboutToBeAdded, this,
                [this](const QVariant &, qsizetype i) { beginInsertColumns({}, asInt(i), asInt(i)); });
        connect(s, &MS::columnAdded, this, [this](const QVariant &, qsizetype) { endInsertColumns(); });
        connect(s, &MS::columnsAboutToBeAdded, this, [this](qsizetype f, qsizetype l, const QList<QVariant> &) {
            beginInsertColumns({}, asInt(f), asInt(l));
        });
        connect(s, &MS::columnsAdded, this, [this](qsizetype, qsizetype, const QList<QVariant> &) { endInsertColumns(); });
        connect(s, &MS::columnAboutToBeRemoved, this,
                [this](const QVariant &, qsizetype i) { beginRemoveColumns({}, asInt(i), asInt(i)); });
        connect(s, &MS::columnRemoved, this, [this](const QVariant &, qsizetype) { endRemoveColumns(); });
        connect(s, &MS::columnsAboutToBeRemoved, this, [this](qsizetype f, qsizetype l, const QList<QVariant> &) {
            beginRemoveColumns({}, asInt(f), asInt(l));
        });
        connect(s, &MS::columnsRemoved, this, [this](qsizetype, qsizetype, const QList<QVariant> &) { endRemoveColumns(); });

        connect(s, &MS::aboutToBeCleared, this, [this] { beginResetModel(); });
        connect(s, &MS::cleared, this, [this] { endResetModel(); });
    }

    void connectContent()
    {
        M * s = &m_source;
        connect(s, &MS::rowRenamed, this, [this](const QVariant &, const QVariant &, qsizetype i) {
            emit headerDataChanged(Qt::Vertical, asInt(i), asInt(i));
        });
        connect(s, &MS::columnRenamed, this, [this](const QVariant &, const QVariant &, qsizetype j) {
            emit headerDataChanged(Qt::Horizontal, asInt(j), asInt(j));
        });
        connect(s, &MS::cellChanged, this,
                [this](const QVariant &, const QVariant &, qsizetype i, qsizetype j) {
                    emit dataChanged(index(asInt(i), asInt(j)), index(asInt(i), asInt(j)));
                });
        connect(s, &MS::cellsChanged, this, [this](qsizetype t, qsizetype l, qsizetype b, qsizetype r) {
            emit dataChanged(index(asInt(t), asInt(l)), index(asInt(b), asInt(r)));
        });
    }

    M & m_source;
};

}  // namespace obs
