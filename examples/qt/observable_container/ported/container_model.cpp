#include "container_model.h"

ContainerModel::ContainerModel(SampleList* source, QObject* parent) : QAbstractListModel(parent), m_source(source) {
  // The key arrives as the row, typed.
  connect(source, &SampleList::entryAboutToBeAdded, this, [this](const qsizetype& row) {
    beginInsertRows(QModelIndex(), static_cast<int>(row), static_cast<int>(row));
  });
  connect(source, &SampleList::entryAdded, this, [this](const qsizetype&) { endInsertRows(); });
}

int ContainerModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(m_source->size());
}

QVariant ContainerModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || role != Qt::DisplayRole) return {};
  return m_source->value(index.row()).value;
}
