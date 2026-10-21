#include "color_list_model.h"

ColorListModel::ColorListModel(QObject * parent) : QAbstractListModel(parent) {}

int ColorListModel::rowCount(const QModelIndex & parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QVariant ColorListModel::data(const QModelIndex & index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const ColorItem & item = m_items[index.row()];
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return item.name;
    case HexRole:
        return item.hex;
    default:
        return {};
    }
}

QHash<int, QByteArray> ColorListModel::roleNames() const
{
    QHash<int, QByteArray> names = QAbstractListModel::roleNames();
    names.insert(NameRole, "name");
    names.insert(HexRole, "hex");
    return names;
}

const QList<ColorItem> & ColorListModel::items() const
{
    return m_items;
}

void ColorListModel::insertItem(int row, const ColorItem & item)
{
    Q_ASSERT(row >= 0 && row <= rowCount());
    beginInsertRows({}, row, row);
    m_items.insert(row, item);
    onRowInserted(row);
    endInsertRows();
    afterRowInserted(row);
}

void ColorListModel::removeItem(int row)
{
    Q_ASSERT(row >= 0 && row < rowCount());
    beginRemoveRows({}, row, row);
    m_items.remove(row);
    onRowRemoved(row);
    endRemoveRows();
    afterRowRemoved(row);
}

void ColorListModel::resetWith(const QList<ColorItem> & items)
{
    beginResetModel();
    m_items = items;
    onReset();
    endResetModel();
    afterReset();
}

void ColorListModel::onRowInserted(int) {}
void ColorListModel::onRowRemoved(int) {}
void ColorListModel::onReset() {}
void ColorListModel::afterRowInserted(int) {}
void ColorListModel::afterRowRemoved(int) {}
void ColorListModel::afterReset() {}
