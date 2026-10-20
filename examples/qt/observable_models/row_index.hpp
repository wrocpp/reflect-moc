// How a model finds the row of a key, and the key of a row, without walking the container.
//
//   IdentityIndex        a list: the key is the row, nothing to keep.
//   SortedIndex          a map: the model's own sorted copy of the keys, searched by binary search. A QMap
//                        counts a row by walking its tree (its iterators are not random access), which is
//                        linear; the copy turns that into a search, and the one linear step left is a memmove.
//   MultiSortedIndex / MultiInsertionIndex  the same for a QMultiMap and a QMultiHash, where the entries of one key
//                        are numbered from the most recent (ContainerTraits' MultiKey).
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

// Slots in the order entries were added, some of them dead. A Fenwick tree counts the live slots, so the row of a
// slot (the live slots before it) and the slot of a row (the k-th live slot) both cost log n.
class LiveSlots {
 public:
  void clear() {
    m_alive.clear();
    m_tree.assign(1, 0);
    m_live = 0;
  }

  qsizetype count() const { return static_cast<qsizetype>(m_alive.size()); }
  qsizetype live() const { return m_live; }

  // Adds a live slot after the last one and returns it.
  qsizetype append() {
    const qsizetype position = count() + 1;  // Fenwick positions start at 1
    m_alive.push_back(true);
    m_tree.push_back(0);
    // The node at `position` covers the slots (position - lowbit, position], this one included.
    m_tree[static_cast<std::size_t>(position)] = 1 + prefix(position - 1) - prefix(position - lowbit(position));
    ++m_live;
    return position - 1;
  }

  void kill(qsizetype slot) {
    m_alive[static_cast<std::size_t>(slot)] = false;
    add(slot + 1, -1);
    --m_live;
  }

  bool alive(qsizetype slot) const { return m_alive[static_cast<std::size_t>(slot)]; }

  // Below this many slots a rebuild would cost more than the dead slots do.
  static constexpr qsizetype kCompactFloor = 64;
  bool needs_compaction() const { return count() > kCompactFloor && m_live * 2 < count(); }

  qsizetype row_of(qsizetype slot) const { return prefix(slot + 1) - 1; }

  // The slot of the k-th live slot, k counted from 0.
  qsizetype slot_of_row(qsizetype row) const {
    qsizetype k = row + 1;
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

 private:
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

  std::vector<bool> m_alive;
  std::vector<int> m_tree{0};  // Fenwick tree over the slots, 1-based
  qsizetype m_live = 0;
};

template <class Key>
class InsertionOrderIndex {
 public:
  void assign(const QList<Key>& keys) {
    clear();
    for (const Key& key : keys) append(key);
  }

  void clear() {
    m_slots.clear();
    m_slot_of.clear();
    m_keys.clear();
  }

  qsizetype rowToInsert(const Key&) const { return m_slots.live(); }
  qsizetype rowOf(const Key& key) const { return m_slots.row_of(m_slot_of.value(key)); }
  Key keyAt(qsizetype row) const { return m_keys[static_cast<std::size_t>(m_slots.slot_of_row(row))]; }
  void insert(const Key& key, qsizetype) { append(key); }

  void erase(const Key& key, qsizetype) {
    m_slots.kill(m_slot_of.take(key));
    if (m_slots.needs_compaction()) compact();
  }

 private:
  void append(const Key& key) {
    m_slot_of.insert(key, m_slots.append());
    m_keys.push_back(key);
  }

  void compact() {
    QList<Key> live;
    live.reserve(m_slots.live());
    for (qsizetype slot = 0; slot < m_slots.count(); ++slot)
      if (m_slots.alive(slot)) live.append(m_keys[static_cast<std::size_t>(slot)]);
    assign(live);
  }

  LiveSlots m_slots;
  QHash<Key, qsizetype> m_slot_of;
  std::vector<Key> m_keys;  // by slot, dead slots included
};

// A multi container, sorted: the key of every entry, ascending, repeats included. The entries of a key are
// together and the most recent comes first, so the row of (key, n) is where the key starts, plus n.
template <class K>
class MultiSortedIndex {
 public:
  using Key = MultiKey<K>;

  void assign(const QList<Key>& entries) {
    m_keys.clear();
    for (const Key& entry : entries) m_keys.push_back(entry.key);
  }
  void clear() { m_keys.clear(); }
  qsizetype rowToInsert(const Key& entry) const { return first_row_of(entry.key); }
  qsizetype rowOf(const Key& entry) const { return first_row_of(entry.key) + entry.occurrence; }
  Key keyAt(qsizetype row) const {
    const K& key = m_keys[static_cast<std::size_t>(row)];
    return Key{key, row - first_row_of(key)};
  }
  void insert(const Key& entry, qsizetype row) { m_keys.insert(m_keys.begin() + row, entry.key); }
  void erase(const Key&, qsizetype row) { m_keys.erase(m_keys.begin() + row); }

 private:
  qsizetype first_row_of(const K& key) const { return std::lower_bound(m_keys.begin(), m_keys.end(), key) - m_keys.begin(); }
  std::vector<K> m_keys;
};

// A multi container, unordered: entries in the order they were added. Each key lists its slots, oldest first, so
// the entry with occurrence n is the n-th slot from the end.
template <class K>
class MultiInsertionIndex {
 public:
  using Key = MultiKey<K>;

  // `entries` lists the entries of a key oldest first (ContainerTraits<QMultiHash>::keys does).
  void assign(const QList<Key>& entries) {
    clear();
    for (const Key& entry : entries) append(entry.key);
  }

  void clear() {
    m_slots.clear();
    m_slots_of.clear();
    m_keys.clear();
  }

  qsizetype rowToInsert(const Key&) const { return m_slots.live(); }
  qsizetype rowOf(const Key& entry) const { return m_slots.row_of(slot_of(entry)); }

  Key keyAt(qsizetype row) const {
    const qsizetype slot = m_slots.slot_of_row(row);
    const K& key = m_keys[static_cast<std::size_t>(slot)];
    const std::vector<qsizetype>& list = m_slots_of.value(key);
    const auto at = std::lower_bound(list.begin(), list.end(), slot);
    return Key{key, static_cast<qsizetype>(list.end() - at) - 1};
  }

  void insert(const Key& entry, qsizetype) { append(entry.key); }

  void erase(const Key& entry, qsizetype) {
    std::vector<qsizetype>& list = m_slots_of[entry.key];
    const qsizetype slot = list[list.size() - 1 - static_cast<std::size_t>(entry.occurrence)];
    list.erase(list.end() - 1 - entry.occurrence);
    if (list.empty()) m_slots_of.remove(entry.key);
    m_slots.kill(slot);
    if (m_slots.needs_compaction()) compact();
  }

 private:
  qsizetype slot_of(const Key& entry) const {
    const std::vector<qsizetype>& list = m_slots_of.value(entry.key);
    return list[list.size() - 1 - static_cast<std::size_t>(entry.occurrence)];
  }

  void append(const K& key) {
    m_slots_of[key].push_back(m_slots.append());
    m_keys.push_back(key);
  }

  // Rebuilds with dense slots, keeping the order and each key's oldest-first list.
  void compact() {
    QList<Key> live;
    for (qsizetype slot = 0; slot < m_slots.count(); ++slot)
      if (m_slots.alive(slot)) live.append(Key{m_keys[static_cast<std::size_t>(slot)], 0});
    assign(live);
  }

  LiveSlots m_slots;
  QHash<K, std::vector<qsizetype>> m_slots_of;
  std::vector<K> m_keys;  // by slot, dead slots included
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
template <class Key>
struct RowIndexFor<Order::sorted_multi, Key> {
  using type = MultiSortedIndex<decltype(Key::key)>;
};
template <class Key>
struct RowIndexFor<Order::unordered_multi, Key> {
  using type = MultiInsertionIndex<decltype(Key::key)>;
};

}  // namespace observable
