#include "container_model.h"

ContainerModel::ContainerModel(SampleList* source, QObject* parent) : QAbstractListModel(parent), m_source(source) {
  // The key arrives as a QVariant and is unboxed to the row.
  connect(source, &ContainerSignals::entryAboutToBeAdded, this, [this](const QVariant& key) {
    const int row = static_cast<int>(key.value<qsizetype>());
    beginInsertRows(QModelIndex(), row, row);
  });
  connect(source, &ContainerSignals::entryAdded, this, [this](const QVariant&) { endInsertRows(); });
}

int ContainerModel::rowCount(const QModelIndex& parent) const {
  return parent.isValid() ? 0 : static_cast<int>(m_source->size());
}

QVariant ContainerModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid() || role != Qt::DisplayRole) return {};
  return m_source->value(index.row()).value;
}
