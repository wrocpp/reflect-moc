// The key and the value of the two containers the example observes: a QHash by its key, a QList by its index.
#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>

template <class C>
struct ContainerTraits;

template <class K, class V>
struct ContainerTraits<QHash<K, V>> {
  using Key = K;
  using Value = V;
};

template <class V>
struct ContainerTraits<QList<V>> {
  using Key = qsizetype;
  using Value = V;
};
