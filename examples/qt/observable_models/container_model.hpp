// One list model for every observable container. It is written once: the begin and end calls of
// QAbstractItemModel are paired here, driven by the container's typed signals, so a new data type needs a
// formatter and nothing else.
//
// Where the row comes from depends on how the container orders its entries:
//   indexed    the key is the row;
//   sorted     the row is counted from the key, at announcement time (a removed key is no longer there later);
//   unordered  the model keeps its own list of keys, in the order they were added.
#pragma once

#include "observable_container.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QAbstractListModel>

#include <algorithm>
#include <functional>
#include <vector>

namespace observable {

template <class C>
class ContainerModel : public QAbstractListModel {
  RQT_OBJECT

 public:
  using Source = ObservableContainer<C>;
  using Traits = ContainerTraits<C>;
  using Key = typename Traits::Key;
  using Value = typename Traits::Value;
  using Formatter = std::function<QVariant(const Key&, const Value&)>;

  static constexpr int KeyRole = Qt::UserRole;
  // A list entry has no identity but its row, which shifts under every insert and remove without a signal,
  // so a role that answered with it would go stale in a sorting proxy. Lists do not offer the role.
  static constexpr bool hasKeyRole = Traits::order != observable::Order::indexed;

  ContainerModel(Source* source, Formatter formatter, QObject* parent = nullptr)
      : QAbstractListModel(parent), m_source(source), m_formatter(std::move(formatter)) {
    if constexpr (Traits::order == Order::unordered) {
      const auto keys = Traits::keys(source->items());
      m_order.assign(keys.begin(), keys.end());
    }
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
    if constexpr (Traits::order == Order::unordered)
      return m_order[static_cast<std::size_t>(row)];
    else
      return Traits::key_at(m_source->items(), row);
  }

  // The row of a key that is in the container now.
  qsizetype rowOf(const Key& key) const {
    if constexpr (Traits::order == Order::unordered)
      return std::find(m_order.begin(), m_order.end(), key) - m_order.begin();
    else
      return Traits::row_of(m_source->items(), key);
  }

  // The row a key that is not in the container yet will occupy.
  qsizetype rowToInsert(const Key& key) const {
    if constexpr (Traits::order == Order::unordered)
      return static_cast<qsizetype>(m_order.size());
    else
      return Traits::insert_row(m_source->items(), key);
  }

  void connectSource() {
    connect(m_source, &Source::entryAboutToBeAdded, this, [this](const Key& key) {
      const int row = static_cast<int>(rowToInsert(key));
      beginInsertRows(QModelIndex(), row, row);
    });
    connect(m_source, &Source::entryAdded, this, [this](const Key& key) {
      if constexpr (Traits::order == Order::unordered) m_order.push_back(key);
      endInsertRows();
    });

    connect(m_source, &Source::entryAboutToBeRemoved, this, [this](const Key& key) {
      m_pendingRow = static_cast<int>(rowOf(key));
      beginRemoveRows(QModelIndex(), m_pendingRow, m_pendingRow);
    });
    connect(m_source, &Source::entryRemoved, this, [this](const Key&) {
      if constexpr (Traits::order == Order::unordered) m_order.erase(m_order.begin() + m_pendingRow);
      endRemoveRows();
    });

    connect(m_source, &Source::entryChanged, this, [this](const Key& key) {
      const QModelIndex changed = index(static_cast<int>(rowOf(key)));
      emit dataChanged(changed, changed);
    });

    if constexpr (Traits::movable) {
      connect(m_source, &Source::entryAboutToBeMoved, this, [this](const Key& from, const Key& to) {
        const int source = static_cast<int>(from);
        const int target = static_cast<int>(to);
        // beginMoveRows takes the row the block is put before, so a move down counts one further.
        beginMoveRows(QModelIndex(), source, source, QModelIndex(), target > source ? target + 1 : target);
      });
      connect(m_source, &Source::entryMoved, this, [this](const Key&, const Key&) { endMoveRows(); });
    }

    connect(m_source, &Source::aboutToBeCleared, this, [this] { beginResetModel(); });
    connect(m_source, &Source::cleared, this, [this] {
      if constexpr (Traits::order == Order::unordered) m_order.clear();
      endResetModel();
    });
  }

  Source* m_source;
  Formatter m_formatter;
  std::vector<Key> m_order;  // unordered containers only; a vector, because std::find scans it the same way wherever the code lands
  int m_pendingRow = -1;
};

}  // namespace observable
