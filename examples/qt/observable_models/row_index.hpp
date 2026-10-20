// How a model finds the row of a key, and the key of a row, without walking the container.
//
//   IdentityIndex        a list: the key is the row, nothing to keep.
//   SortedIndex          a map: the model's own sorted copy of the keys, searched by binary search. A QMap
//                        counts a row by walking its tree (its iterators are not random access), which is
//                        linear; the copy turns that into a search, and the one linear step left is a memmove.
//   InsertionOrderIndex  a hash or set: keys in the order they were added. A Fenwick tree over the slots counts
//                        the live slots before a key (its row) and finds the k-th live slot (a row's key) in
//                        logarithmic time; a removal leaves a dead slot, and the slots are rebuilt when
//                        more than half of them are dead.
//
// All three answer the same calls, so the model has no branch on the container's order.
#pragma once

#include "container_traits.hpp"

#include <QtCore/QHash>
#include <QtCore/QList>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace observable {

template <class Key>
class IdentityIndex {
 public:
  void assign(const QList<Key>&) {}
  void clear() {}
  qsizetype rowToInsert(const Key& key) const { return static_cast<qsizetype>(key); }
  qsizetype rowOf(const Key& key) const { return static_cast<qsizetype>(key); }
  Key keyAt(qsizetype row) const { return static_cast<Key>(row); }
  void insert(const Key&, qsizetype) {}
  void erase(const Key&, qsizetype) {}
};

template <class Key>
class SortedIndex {
 public:
  // `keys` must be in ascending order, as QMap::keys() returns them.
  void assign(const QList<Key>& keys) { m_keys.assign(keys.begin(), keys.end()); }
  void clear() { m_keys.clear(); }
  qsizetype rowToInsert(const Key& key) const { return std::lower_bound(m_keys.begin(), m_keys.end(), key) - m_keys.begin(); }
  qsizetype rowOf(const Key& key) const { return rowToInsert(key); }
  Key keyAt(qsizetype row) const { return m_keys[static_cast<std::size_t>(row)]; }
  void insert(const Key& key, qsizetype row) { m_keys.insert(m_keys.begin() + row, key); }
  void erase(const Key&, qsizetype row) { m_keys.erase(m_keys.begin() + row); }

 private:
  std::vector<Key> m_keys;
};

template <class Key>
class InsertionOrderIndex {
 public:
  void assign(const QList<Key>& keys) {
    clear();
    for (const Key& key : keys) append(key);
  }

  void clear() {
    m_slot_of.clear();
    m_keys.clear();
    m_alive.clear();
    m_tree.assign(1, 0);
    m_live = 0;
  }

  qsizetype rowToInsert(const Key&) const { return m_live; }
  qsizetype rowOf(const Key& key) const { return prefix(m_slot_of.value(key) + 1) - 1; }
  Key keyAt(qsizetype row) const { return m_keys[static_cast<std::size_t>(kth_live_slot(row + 1))]; }
  void insert(const Key& key, qsizetype) { append(key); }

  void erase(const Key& key, qsizetype) {
    const qsizetype slot = m_slot_of.take(key);
    m_alive[static_cast<std::size_t>(slot)] = false;
    add(slot + 1, -1);
    --m_live;
    if (m_keys.size() > kCompactFloor && static_cast<std::size_t>(m_live) * 2 < m_keys.size()) compact();
  }

 private:
  // Below this many slots a rebuild would cost more than the dead slots do.
  static constexpr std::size_t kCompactFloor = 64;

  void append(const Key& key) {
    const qsizetype position = static_cast<qsizetype>(m_keys.size()) + 1;  // Fenwick positions start at 1
    m_slot_of.insert(key, position - 1);
    m_keys.push_back(key);
    m_alive.push_back(true);
    m_tree.push_back(0);
    // The node at `position` covers the slots (position - lowbit, position], this one included.
    m_tree[static_cast<std::size_t>(position)] = 1 + prefix(position - 1) - prefix(position - lowbit(position));
    ++m_live;
  }

  void compact() {
    QList<Key> live;
    live.reserve(m_live);
    for (std::size_t slot = 0; slot < m_keys.size(); ++slot)
      if (m_alive[slot]) live.append(m_keys[slot]);
    assign(live);
  }

  static qsizetype lowbit(qsizetype i) { return i & -i; }

  void add(qsizetype position, int delta) {
    for (; position < static_cast<qsizetype>(m_tree.size()); position += lowbit(position))
      m_tree[static_cast<std::size_t>(position)] += delta;
  }

  // Live slots among the first `position` slots.
  qsizetype prefix(qsizetype position) const {
    qsizetype sum = 0;
    for (; position > 0; position -= lowbit(position)) sum += m_tree[static_cast<std::size_t>(position)];
    return sum;
  }

  // The slot (counted from 0) of the k-th live slot, k counted from 1.
  qsizetype kth_live_slot(qsizetype k) const {
    qsizetype position = 0;
    qsizetype step = 1;
    while (step * 2 < static_cast<qsizetype>(m_tree.size())) step *= 2;
    for (; step > 0; step /= 2) {
      const qsizetype next = position + step;
      if (next < static_cast<qsizetype>(m_tree.size()) && m_tree[static_cast<std::size_t>(next)] < k) {
        position = next;
        k -= m_tree[static_cast<std::size_t>(position)];
      }
    }
    return position;
  }

  QHash<Key, qsizetype> m_slot_of;
  std::vector<Key> m_keys;        // by slot, dead slots included
  std::vector<bool> m_alive;      // by slot
  std::vector<int> m_tree{0};     // Fenwick tree over the slots, 1-based
  qsizetype m_live = 0;
};

template <Order O, class Key>
struct RowIndexFor;
template <class Key>
struct RowIndexFor<Order::indexed, Key> {
  using type = IdentityIndex<Key>;
};
template <class Key>
struct RowIndexFor<Order::sorted, Key> {
  using type = SortedIndex<Key>;
};
template <class Key>
struct RowIndexFor<Order::unordered, Key> {
  using type = InsertionOrderIndex<Key>;
};

}  // namespace observable
