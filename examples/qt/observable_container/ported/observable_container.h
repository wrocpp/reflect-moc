// observable_container.h: one class template with typed signals. RQT_OBJECT works in a template; the signals
// carry the key as it is, so there is no signals-only base class and no QVariant.
#pragma once

#include "container_traits.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QObject>

#include <utility>

template <class Container>
class ObservableContainer : public QObject {
  RQT_OBJECT

 public:
  using Key = typename ContainerTraits<Container>::Key;
  using Value = typename ContainerTraits<Container>::Value;

  void insert(const Key& key, Value value) {
    emit entryAboutToBeAdded(key);
    m_items.insert(key, std::move(value));
    emit entryAdded(key);
  }

  Value value(const Key& key) const { return m_items[key]; }
  qsizetype size() const { return m_items.size(); }

 signals:
  rqt::signal<void(const Key&)> entryAboutToBeAdded;
  rqt::signal<void(const Key&)> entryAdded;

 private:
  Container m_items;
};
