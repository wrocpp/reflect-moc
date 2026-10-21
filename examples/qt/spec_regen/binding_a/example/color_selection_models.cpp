#include "color_selection_models.h"

// ---- single selection -------------------------------------------------

ColorSingleSelectionModel::ColorSingleSelectionModel(QObject * parent) : ColorListModel(parent) {}

bool ColorSingleSelectionModel::hasSelection() const
{
    return m_tracker.hasSelection();
}

QString ColorSingleSelectionModel::selectedName() const
{
    return sel::selectedValue<&ColorItem::name>(m_tracker, items());
}

void ColorSingleSelectionModel::clearSelection()
{
    sel::clearSelectionAndNotify(this, m_tracker);
}

QHash<int, QByteArray> ColorSingleSelectionModel::roleNames() const
{
    QHash<int, QByteArray> names = ColorListModel::roleNames();
    return Tracker::appendRoleNames(names);
}

QVariant ColorSingleSelectionModel::data(const QModelIndex & index, int role) const
{
    if (role == SelectedRole && index.isValid())
        return m_tracker.isSelected(index.row());
    return ColorListModel::data(index, role);
}

bool ColorSingleSelectionModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    if (role != SelectedRole || !index.isValid())
        return false;
    return sel::applySelectionValue(this, m_tracker, index, value);
}

Qt::ItemFlags ColorSingleSelectionModel::flags(const QModelIndex & index) const
{
    Qt::ItemFlags f = ColorListModel::flags(index);
    if (index.isValid())
        f |= Qt::ItemIsEditable;
    return f;
}

void ColorSingleSelectionModel::onRowInserted(int row)
{
    m_rowBeforeInsert = m_tracker.hasSelection() ? std::optional<int>{m_tracker.selectedRow()} : std::nullopt;
    m_tracker.rowInserted(row);
}

void ColorSingleSelectionModel::onRowRemoved(int row)
{
    m_removalDelta = m_tracker.rowRemoved(row);
}

void ColorSingleSelectionModel::onReset()
{
    m_hadSelection = m_tracker.hasSelection();
    m_tracker.reset(rowCount());
}

void ColorSingleSelectionModel::afterRowInserted(int)
{
    const std::optional<int> now = m_tracker.hasSelection() ? std::optional<int>{m_tracker.selectedRow()} : std::nullopt;
    if (now != m_rowBeforeInsert)
        emit selectedRowChanged();
}

void ColorSingleSelectionModel::afterRowRemoved(int)
{
    if (m_removalDelta.selectionChanged)
        emit selectedRowChanged();
    if (m_removalDelta.presenceFlipped)
        emit hasSelectionChanged();
}

void ColorSingleSelectionModel::afterReset()
{
    if (!m_hadSelection)
        return;
    emit selectedRowChanged();
    emit hasSelectionChanged();
}

// ---- multiple selection -----------------------------------------------

ColorMultiSelectionModel::ColorMultiSelectionModel(QObject * parent) : ColorListModel(parent) {}

bool ColorMultiSelectionModel::hasSelection() const
{
    return m_tracker.hasSelection();
}

int ColorMultiSelectionModel::selectedCount() const
{
    return m_tracker.count();
}

void ColorMultiSelectionModel::clearSelection()
{
    sel::clearSelectionAndNotify(this, m_tracker);
}

QHash<int, QByteArray> ColorMultiSelectionModel::roleNames() const
{
    QHash<int, QByteArray> names = ColorListModel::roleNames();
    return Tracker::appendRoleNames(names);
}

QVariant ColorMultiSelectionModel::data(const QModelIndex & index, int role) const
{
    if (role == SelectedRole && index.isValid())
        return m_tracker.isSelected(index.row());
    return ColorListModel::data(index, role);
}

bool ColorMultiSelectionModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    if (role != SelectedRole || !index.isValid())
        return false;
    return sel::applySelectionValue(this, m_tracker, index, value);
}

Qt::ItemFlags ColorMultiSelectionModel::flags(const QModelIndex & index) const
{
    Qt::ItemFlags f = ColorListModel::flags(index);
    if (index.isValid())
        f |= Qt::ItemIsEditable;
    return f;
}

void ColorMultiSelectionModel::onRowInserted(int row)
{
    m_tracker.rowInserted(row);
}

void ColorMultiSelectionModel::onRowRemoved(int row)
{
    m_removalDelta = m_tracker.rowRemoved(row);
}

void ColorMultiSelectionModel::onReset()
{
    m_hadSelection = m_tracker.hasSelection();
    m_tracker.reset(rowCount());
}

void ColorMultiSelectionModel::afterRowRemoved(int)
{
    if (m_removalDelta.countChanged)
        emit selectedCountChanged();
    if (m_removalDelta.presenceFlipped)
        emit hasSelectionChanged();
}

void ColorMultiSelectionModel::afterReset()
{
    if (!m_hadSelection)
        return;
    emit selectedCountChanged();
    emit hasSelectionChanged();
}
