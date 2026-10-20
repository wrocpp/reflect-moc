// The model of the moc design. It is observable_models/container_model.hpp with one difference: the signals
// deliver the key in a QVariant, so each slot unboxes it with value<Key>(). The row logic is unchanged.
#pragma once

#include "observable_container.h"

#include <QtCore/QAbstractListModel>

#include <functional>

namespace observable_qv {

template <class C>
class ContainerModel : public QAbstractListModel {
 public:
  using Source = ObservableContainer<C>;
  using Traits = observable::ContainerTraits<C>;
  using Key = typename Traits::Key;
  using Value = typename Traits::Value;
  using Formatter = std::function<QVariant(const Key&, const Value&)>;

  static constexpr int KeyRole = Qt::UserRole;
  // A list entry has no identity but its row, which shifts under every insert and remove without a signal,
  // so a role that answered with it would go stale in a sorting proxy. Lists do not offer the role.
  static constexpr bool hasKeyRole = Traits::order != observable::Order::indexed;

  ContainerModel(Source* source, Formatter formatter, QObject* parent = nullptr)
      : QAbstractListModel(parent), m_source(source), m_formatter(std::move(formatter)) {
    if constexpr (Traits::order == observable::Order::unordered) m_order = Traits::keys(source->items());
    connectSource();
  }

  int rowCount(const QModelIndex& parent = QModelIndex()) const override {
    return parent.isValid() ? 0 : static_cast<int>(m_source->size());
  }

  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
    if (!index.isValid() || index.row() >= rowCount()) return {};
    const Key key = keyAt(index.row());
    if (role == KeyRole && hasKeyRole) return QVariant::fromValue(key);
    if (role == Qt::DisplayRole) return m_formatter(key, m_source->value(key));
    return {};
  }

  QHash<int, QByteArray> roleNames() const override {
    auto names = QAbstractListModel::roleNames();
    if (hasKeyRole) names.insert(KeyRole, "key");
    return names;
  }

 private:
  Key keyAt(qsizetype row) const {
    if constexpr (Traits::order == observable::Order::unordered)
      return m_order.at(row);
    else
      return Traits::key_at(m_source->items(), row);
  }

  // The row of a key that is in the container now.
  qsizetype rowOf(const Key& key) const {
    if constexpr (Traits::order == observable::Order::unordered)
      return m_order.indexOf(key);
    else
      return Traits::row_of(m_source->items(), key);
  }

  // The row a key that is not in the container yet will occupy.
  qsizetype rowToInsert(const Key& key) const {
    if constexpr (Traits::order == observable::Order::unordered)
      return m_order.size();
    else
      return Traits::insert_row(m_source->items(), key);
  }

  void connectSource() {
    connect(m_source, &ContainerSignals::entryAboutToBeAdded, this, [this](const QVariant& boxed) {
      const Key key = boxed.value<Key>();
      const int row = static_cast<int>(rowToInsert(key));
      beginInsertRows(QModelIndex(), row, row);
    });
    connect(m_source, &ContainerSignals::entryAdded, this, [this](const QVariant& boxed) {
      const Key key = boxed.value<Key>();
      if constexpr (Traits::order == observable::Order::unordered) m_order.append(key);
      endInsertRows();
    });

    connect(m_source, &ContainerSignals::entryAboutToBeRemoved, this, [this](const QVariant& boxed) {
      const Key key = boxed.value<Key>();
      m_pendingRow = static_cast<int>(rowOf(key));
      beginRemoveRows(QModelIndex(), m_pendingRow, m_pendingRow);
    });
    connect(m_source, &ContainerSignals::entryRemoved, this, [this](const QVariant&) {
      if constexpr (Traits::order == observable::Order::unordered) m_order.removeAt(m_pendingRow);
      endRemoveRows();
    });

    connect(m_source, &ContainerSignals::entryChanged, this, [this](const QVariant& boxed) {
      const Key key = boxed.value<Key>();
      const QModelIndex changed = index(static_cast<int>(rowOf(key)));
      emit dataChanged(changed, changed);
    });

    if constexpr (Traits::movable) {
      connect(m_source, &ContainerSignals::entryAboutToBeMoved, this, [this](const QVariant& boxedFrom, const QVariant& boxedTo) {
        const Key from = boxedFrom.value<Key>();
        const Key to = boxedTo.value<Key>();
        const int source = static_cast<int>(from);
        const int target = static_cast<int>(to);
        // beginMoveRows takes the row the block is put before, so a move down counts one further.
        beginMoveRows(QModelIndex(), source, source, QModelIndex(), target > source ? target + 1 : target);
      });
      connect(m_source, &ContainerSignals::entryMoved, this, [this](const QVariant&, const QVariant&) { endMoveRows(); });
    }

    connect(m_source, &ContainerSignals::aboutToBeCleared, this, [this] { beginResetModel(); });
    connect(m_source, &ContainerSignals::cleared, this, [this] {
      if constexpr (Traits::order == observable::Order::unordered) m_order.clear();
      endResetModel();
    });
  }

  Source* m_source;
  Formatter m_formatter;
  QList<Key> m_order;  // unordered containers only
  int m_pendingRow = -1;
};

}  // namespace observable_qv
