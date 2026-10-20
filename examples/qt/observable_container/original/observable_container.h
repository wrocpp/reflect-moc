// observable_container.h: a template, so no Q_OBJECT (moc would reject it). It inherits the signals.
#pragma once

#include "container_signals.h"
#include "container_traits.h"

#include <utility>

template <class Container>
class ObservableContainer : public ContainerSignals {
 public:
  using Key = typename ContainerTraits<Container>::Key;
  using Value = typename ContainerTraits<Container>::Value;

  void insert(const Key& key, Value value) {
    const QVariant boxedKey = QVariant::fromValue(key);
    emit entryAboutToBeAdded(boxedKey);
    m_items.insert(key, std::move(value));
    emit entryAdded(boxedKey);
  }

  Value value(const Key& key) const { return m_items[key]; }
  qsizetype size() const { return m_items.size(); }

 private:
  Container m_items;
};
