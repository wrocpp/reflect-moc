// What the observable container and its model need to know about each Qt container, in one place:
// how an entry is identified (its key), how a new key finds its position, and which changes the
// container supports. A QList entry is identified by its index, a QMap or QHash entry by its key and
// a QSet entry by itself. In a QMultiMap or QMultiHash two entries can share a key, so an entry is identified by
// a MultiKey: its key and its occurrence, counted from the most recently inserted (0) as values(key) lists them.
// A new entry takes occurrence 0 and the older ones of its key move up, so, like a list index, the identity of an
// entry changes without a signal, and a model offers no key role for them.
#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QMultiHash>
#include <QtCore/QMultiMap>
#include <QtCore/QQueue>
#include <QtCore/QSet>
#include <QtCore/QStack>
#include <QtCore/QStringList>

#include <concepts>
#include <iterator>
#include <limits>
#include <utility>

namespace observable {

// How the model orders the rows of a container.
enum class Order {
  indexed,         // the key is the row (QList and its subclasses)
  sorted,          // the row follows the key order (QMap)
  unordered,       // no order of its own: the model lists keys in the order they were added (QHash, QSet)
  sorted_multi,    // sorted, and several entries per key, the most recent first (QMultiMap)
  unordered_multi  // in the order added, and several entries per key (QMultiHash)
};

// A model counts rows in an int, a Qt container in a qsizetype. A container that would grow past this many entries
// refuses the change (insert, insertRange and insertMany return false or 0, resetWith returns false), so a model's row
// count, row numbers and run bounds always fit an int.
inline constexpr qsizetype kMaxRows = std::numeric_limits<int>::max();

// Whether `added` more entries still fit under kMaxRows on top of `current`.
constexpr bool fits_rows(qsizetype current, qsizetype added) { return added >= 0 && added <= kMaxRows - current; }

// A row or a size as an int, for the model's begin and end calls. The container keeps every row under kMaxRows
// (an oversized content handed to a constructor is the one case it cannot refuse, and asserts in a debug build), and
// a release build saturates instead of wrapping.
inline int to_row(qsizetype row) {
  Q_ASSERT_X(row >= 0 && row <= kMaxRows, "observable::to_row", "a row must fit an int (observable::kMaxRows)");
  return static_cast<int>(row < 0 ? 0 : (row > kMaxRows ? kMaxRows : row));
}

// A floating point key that is not equal to itself (a NaN) cannot be found again, by a hash (it equals nothing) or
// by a map (it is equivalent to every key). A map, hash or set refuses it in insert and insertMany, never finds it,
// and drops it from the content a constructor or resetWith is given.
template <class K>
constexpr bool usable_key(const K& key) {
  if constexpr (std::floating_point<K>)
    return key == key;
  else
    return true;
}

// An entry of a multi container: its key, and which of the entries of that key it is, counted from the most
// recently inserted.
template <class K>
struct MultiKey {
  K key{};
  qsizetype occurrence = 0;
  bool operator==(const MultiKey&) const = default;
};

template <class C>
struct ContainerTraits;

template <class V>
struct ContainerTraits<QList<V>> {
  using Key = qsizetype;
  using Value = V;
  static constexpr Order order = Order::indexed;
  static constexpr bool movable = true;
  static constexpr bool assignable = true;

  static qsizetype size(const QList<V>& c) { return c.size(); }
  static bool contains(const QList<V>& c, Key k) { return k >= 0 && k < c.size(); }
  static bool can_insert(const QList<V>& c, Key k) { return k >= 0 && k <= c.size(); }
  static bool can_move(const QList<V>& c, Key from, Key to) { return contains(c, from) && contains(c, to) && from != to; }
  static void insert(QList<V>& c, Key k, V v) { c.insert(k, std::move(v)); }
  static void erase(QList<V>& c, Key k) { c.removeAt(k); }
  static void assign(QList<V>& c, Key k, V v) { c[k] = std::move(v); }
  static void move(QList<V>& c, Key from, Key to) { c.move(from, to); }
  static const V& value(const QList<V>& c, Key k) { return c.at(k); }
  static QList<Key> keys(const QList<V>& c) {
    QList<Key> out;
    for (qsizetype i = 0; i < c.size(); ++i) out.append(i);
    return out;
  }
};

// QStringList is an alias of QList<QString> in Qt 6, so the QList entry above covers it.
template <class V>
struct ContainerTraits<QQueue<V>> : ContainerTraits<QList<V>> {};
template <class V>
struct ContainerTraits<QStack<V>> : ContainerTraits<QList<V>> {};

template <class K, class V>
struct ContainerTraits<QMap<K, V>> {
  using Key = K;
  using Value = V;
  static constexpr Order order = Order::sorted;
  static constexpr bool movable = false;
  static constexpr bool assignable = true;

  static qsizetype size(const QMap<K, V>& c) { return c.size(); }
  static bool contains(const QMap<K, V>& c, const K& k) { return usable_key(k) && c.contains(k); }
  static bool can_insert(const QMap<K, V>& c, const K& k) { return usable_key(k) && !c.contains(k); }
  static bool can_move(const QMap<K, V>&, const K&, const K&) { return false; }
  static void insert(QMap<K, V>& c, const K& k, V v) { c.insert(k, std::move(v)); }
  static void erase(QMap<K, V>& c, const K& k) { c.remove(k); }
  static void assign(QMap<K, V>& c, const K& k, V v) { c[k] = std::move(v); }
  static void move(QMap<K, V>&, const K&, const K&) {}
  static const V& value(const QMap<K, V>& c, const K& k) { return *c.constFind(k); }
  // Where a key that is not in the map yet will land: before the first key that is not smaller.
  static QList<K> keys(const QMap<K, V>& c) { return c.keys(); }
};

template <class K, class V>
struct ContainerTraits<QHash<K, V>> {
  using Key = K;
  using Value = V;
  static constexpr Order order = Order::unordered;
  static constexpr bool movable = false;
  static constexpr bool assignable = true;

  static qsizetype size(const QHash<K, V>& c) { return c.size(); }
  static bool contains(const QHash<K, V>& c, const K& k) { return usable_key(k) && c.contains(k); }
  static bool can_insert(const QHash<K, V>& c, const K& k) { return usable_key(k) && !c.contains(k); }
  static bool can_move(const QHash<K, V>&, const K&, const K&) { return false; }
  static void insert(QHash<K, V>& c, const K& k, V v) { c.insert(k, std::move(v)); }
  static void erase(QHash<K, V>& c, const K& k) { c.remove(k); }
  static void assign(QHash<K, V>& c, const K& k, V v) { c[k] = std::move(v); }
  static void move(QHash<K, V>&, const K&, const K&) {}
  static const V& value(const QHash<K, V>& c, const K& k) { return *c.constFind(k); }
  static QList<K> keys(const QHash<K, V>& c) { return c.keys(); }
};

template <class K>
struct ContainerTraits<QSet<K>> {
  using Key = K;
  using Value = K;
  static constexpr Order order = Order::unordered;
  static constexpr bool movable = false;
  static constexpr bool assignable = false;  // an entry is its own key: there is nothing to replace

  static qsizetype size(const QSet<K>& c) { return c.size(); }
  static bool contains(const QSet<K>& c, const K& k) { return usable_key(k) && c.contains(k); }
  static bool can_insert(const QSet<K>& c, const K& k) { return usable_key(k) && !c.contains(k); }
  static bool can_move(const QSet<K>&, const K&, const K&) { return false; }
  static void insert(QSet<K>& c, const K& k, const K&) { c.insert(k); }
  static void erase(QSet<K>& c, const K& k) { c.remove(k); }
  static void assign(QSet<K>&, const K&, const K&) {}
  static void move(QSet<K>&, const K&, const K&) {}
  static const K& value(const QSet<K>&, const K& k) { return k; }
  static QList<K> keys(const QSet<K>& c) { return c.values(); }
};

// Both multi containers keep the entries of one key together, the most recently inserted first, so the entry
// with occurrence n is the n-th from find(key).
template <class K, class V>
struct ContainerTraits<QMultiMap<K, V>> {
  using Key = MultiKey<K>;
  using Value = V;
  static constexpr Order order = Order::sorted_multi;
  static constexpr bool movable = false;
  static constexpr bool assignable = true;

  static qsizetype size(const QMultiMap<K, V>& c) { return c.size(); }
  static bool contains(const QMultiMap<K, V>& c, const Key& k) { return k.occurrence >= 0 && k.occurrence < c.count(k.key); }
  static bool can_insert(const QMultiMap<K, V>&, const Key& k) { return usable_key(k.key) && k.occurrence == 0; }  // a new entry is the newest
  static bool can_move(const QMultiMap<K, V>&, const Key&, const Key&) { return false; }
  static void insert(QMultiMap<K, V>& c, const Key& k, V v) { c.insert(k.key, std::move(v)); }
  static void erase(QMultiMap<K, V>& c, const Key& k) { c.erase(std::next(c.find(k.key), k.occurrence)); }
  static void assign(QMultiMap<K, V>& c, const Key& k, V v) { std::next(c.find(k.key), k.occurrence).value() = std::move(v); }
  static void move(QMultiMap<K, V>&, const Key&, const Key&) {}
  static const V& value(const QMultiMap<K, V>& c, const Key& k) { return *std::next(c.constFind(k.key), k.occurrence); }
  // Every entry once, ordered by key, the most recent of a key first.
  static QList<Key> keys(const QMultiMap<K, V>& c) {
    QList<Key> out;
    for (const K& key : c.uniqueKeys())
      for (qsizetype n = 0; n < c.count(key); ++n) out.append(Key{key, n});
    return out;
  }
};

template <class K, class V>
struct ContainerTraits<QMultiHash<K, V>> {
  using Key = MultiKey<K>;
  using Value = V;
  static constexpr Order order = Order::unordered_multi;
  static constexpr bool movable = false;
  static constexpr bool assignable = true;

  static qsizetype size(const QMultiHash<K, V>& c) { return c.size(); }
  static bool contains(const QMultiHash<K, V>& c, const Key& k) { return k.occurrence >= 0 && k.occurrence < c.count(k.key); }
  static bool can_insert(const QMultiHash<K, V>&, const Key& k) { return usable_key(k.key) && k.occurrence == 0; }
  static bool can_move(const QMultiHash<K, V>&, const Key&, const Key&) { return false; }
  static void insert(QMultiHash<K, V>& c, const Key& k, V v) { c.insert(k.key, std::move(v)); }
  static void erase(QMultiHash<K, V>& c, const Key& k) { c.erase(std::next(c.find(k.key), k.occurrence)); }
  static void assign(QMultiHash<K, V>& c, const Key& k, V v) { std::next(c.find(k.key), k.occurrence).value() = std::move(v); }
  static void move(QMultiHash<K, V>&, const Key&, const Key&) {}
  static const V& value(const QMultiHash<K, V>& c, const Key& k) { return *std::next(c.constFind(k.key), k.occurrence); }
  // Every entry once, the oldest of a key first, so that the model numbers the slots in the order they were added.
  static QList<Key> keys(const QMultiHash<K, V>& c) {
    QList<Key> out;
    for (const K& key : c.uniqueKeys())
      for (qsizetype n = c.count(key); n-- > 0;) out.append(Key{key, n});
    return out;
  }
};

// Removes the entries whose key is unusable (see usable_key) from a map, hash or set given whole. Other containers, and
// keys that are not floating point, are left alone at no cost.
template <class C>
void drop_unusable_keys(C& c) {
  using Traits = ContainerTraits<C>;
  if constexpr ((Traits::order == Order::sorted || Traits::order == Order::unordered) && std::floating_point<typename Traits::Key>) {
    for (auto it = c.begin(); it != c.end();) {
      bool usable = false;
      if constexpr (requires { it.key(); })
        usable = usable_key(it.key());
      else
        usable = usable_key(*it);
      it = usable ? std::next(it) : c.erase(it);
    }
  }
}

}  // namespace observable
