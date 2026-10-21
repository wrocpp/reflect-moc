#include "color_selection_models.h"

// ---- Single ----

ColorSingleSelectionModel::ColorSingleSelectionModel(QObject * parent) : ColorListModel(parent) {}

bool ColorSingleSelectionModel::hasSelection() const
{
    return tracker_.hasSelection();
}

QString ColorSingleSelectionModel::selectedName() const
{
    return sel::selectedValue<&ColorItem::name>(tracker_, items());
}

void ColorSingleSelectionModel::clearSelection()
{
    sel::clearSelectionAndNotify(this, tracker_);
}

QHash<int, QByteArray> ColorSingleSelectionModel::roleNames() const
{
    QHash<int, QByteArray> names = ColorListModel::roleNames();
    return Tracker::appendRoleNames(names);
}

QVariant ColorSingleSelectionModel::data(const QModelIndex & index, int role) const
{
    if (role == SelectedRole && index.isValid())
        return tracker_.isSelected(index.row());
    return ColorListModel::data(index, role);
}

bool ColorSingleSelectionModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    if (role == SelectedRole && index.isValid())
        return sel::applySelectionValue(this, tracker_, index, value);
    return false;
}

Qt::ItemFlags ColorSingleSelectionModel::flags(const QModelIndex & index) const
{
    const Qt::ItemFlags base = ColorListModel::flags(index);
    return index.isValid() ? base | Qt::ItemIsEditable : base;
}

// Structural changes are folded INSIDE the base's begin/end pair; the aggregate notifications follow the end* call.
void ColorSingleSelectionModel::onRowInserted(int row)
{
    const std::optional<int> before = tracker_.hasSelection() ? std::optional<int>{tracker_.selectedRow()} : std::nullopt;
    tracker_.rowInserted(row);
    const std::optional<int> after = tracker_.hasSelection() ? std::optional<int>{tracker_.selectedRow()} : std::nullopt;
    shifted_ = before != after;
}

void ColorSingleSelectionModel::onRowRemoved(int row)
{
    removal_ = tracker_.rowRemoved(row);
}

void ColorSingleSelectionModel::onReset()
{
    hadSelection_ = tracker_.hasSelection();
    tracker_.reset(rowCount());
}

void ColorSingleSelectionModel::afterRowInserted(int)
{
    if (shifted_)
        selectedRowChanged();
}

void ColorSingleSelectionModel::afterRowRemoved(int)
{
    if (removal_.selectionChanged)
        selectedRowChanged();
    if (removal_.presenceFlipped)
        hasSelectionChanged();
}

void ColorSingleSelectionModel::afterReset()
{
    if (hadSelection_) {
        selectedRowChanged();
        hasSelectionChanged();
    }
}

// ---- Multi ----

ColorMultiSelectionModel::ColorMultiSelectionModel(QObject * parent) : ColorListModel(parent) {}

bool ColorMultiSelectionModel::hasSelection() const
{
    return tracker_.hasSelection();
}

int ColorMultiSelectionModel::selectedCount() const
{
    return tracker_.count();
}

void ColorMultiSelectionModel::clearSelection()
{
    sel::clearSelectionAndNotify(this, tracker_);
}

QHash<int, QByteArray> ColorMultiSelectionModel::roleNames() const
{
    QHash<int, QByteArray> names = ColorListModel::roleNames();
    return Tracker::appendRoleNames(names);
}

QVariant ColorMultiSelectionModel::data(const QModelIndex & index, int role) const
{
    if (role == SelectedRole && index.isValid())
        return tracker_.isSelected(index.row());
    return ColorListModel::data(index, role);
}

bool ColorMultiSelectionModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    if (role == SelectedRole && index.isValid())
        return sel::applySelectionValue(this, tracker_, index, value);
    return false;
}

Qt::ItemFlags ColorMultiSelectionModel::flags(const QModelIndex & index) const
{
    const Qt::ItemFlags base = ColorListModel::flags(index);
    return index.isValid() ? base | Qt::ItemIsEditable : base;
}

void ColorMultiSelectionModel::onRowInserted(int row)
{
    tracker_.rowInserted(row);
}

void ColorMultiSelectionModel::onRowRemoved(int row)
{
    removal_ = tracker_.rowRemoved(row);
}

void ColorMultiSelectionModel::onReset()
{
    hadSelection_ = tracker_.hasSelection();
    tracker_.reset(rowCount());
}

void ColorMultiSelectionModel::afterRowInserted(int) {}

void ColorMultiSelectionModel::afterRowRemoved(int)
{
    if (removal_.countChanged)
        selectedCountChanged();
    if (removal_.presenceFlipped)
        hasSelectionChanged();
}

void ColorMultiSelectionModel::afterReset()
{
    if (hadSelection_) {
        selectedCountChanged();
        hasSelectionChanged();
    }
}
