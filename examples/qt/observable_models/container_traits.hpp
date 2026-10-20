// What the observable container and its model need to know about each Qt container, in one place:
// how an entry is identified (its key), how a new key finds its position, and which changes the
// container supports. A QList entry is identified by its index, a QMap or QHash entry by its key and
// a QSet entry by itself. QMultiMap and QMultiHash are not described: two entries can share a key,
// so a key does not identify one.
#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QQueue>
#include <QtCore/QSet>
#include <QtCore/QStack>
#include <QtCore/QStringList>

#include <iterator>
#include <utility>

namespace observable {

// How the model orders the rows of a container.
enum class Order {
  indexed,    // the key is the row (QList and its subclasses)
  sorted,     // the row follows the key order (QMap)
  unordered,  // no order of its own: the model lists keys in the order they were added (QHash, QSet)
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
  static bool contains(const QMap<K, V>& c, const K& k) { return c.contains(k); }
  static bool can_insert(const QMap<K, V>& c, const K& k) { return !c.contains(k); }
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
  static bool contains(const QHash<K, V>& c, const K& k) { return c.contains(k); }
  static bool can_insert(const QHash<K, V>& c, const K& k) { return !c.contains(k); }
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
  static bool contains(const QSet<K>& c, const K& k) { return c.contains(k); }
  static bool can_insert(const QSet<K>& c, const K& k) { return !c.contains(k); }
  static bool can_move(const QSet<K>&, const K&, const K&) { return false; }
  static void insert(QSet<K>& c, const K& k, const K&) { c.insert(k); }
  static void erase(QSet<K>& c, const K& k) { c.remove(k); }
  static void assign(QSet<K>&, const K&, const K&) {}
  static void move(QSet<K>&, const K&, const K&) {}
  static const K& value(const QSet<K>&, const K& k) { return k; }
  static QList<K> keys(const QSet<K>& c) { return c.values(); }
};

}  // namespace observable
