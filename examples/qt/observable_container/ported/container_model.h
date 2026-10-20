// A list model over an observable QList: the container's signals drive beginInsertRows and endInsertRows.
#pragma once

#include "observable_container.h"
#include "sample.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QAbstractListModel>

using SampleList = ObservableContainer<QList<Sample>>;

class ContainerModel : public QAbstractListModel {
  RQT_OBJECT

 public:
  explicit ContainerModel(SampleList* source, QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = QModelIndex()) const override;
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

 private:
  SampleList* m_source;
};
