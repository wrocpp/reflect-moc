// The typed half of the moc design: a class template cannot have Q_OBJECT, so it inherits the signals from
// ContainerSignals and boxes each key in a QVariant at every emit. Everything else is the same as
// examples/qt/observable_models/observable_container.hpp, line for line.
#pragma once

#include "container_signals.h"
#include <container_traits.hpp>

#include <utility>

namespace observable_qv {

using observable::ContainerTraits;

template <class C>
class ObservableContainer : public ContainerSignals {

 public:
  using Traits = ContainerTraits<C>;
  using Key = typename Traits::Key;
  using Value = typename Traits::Value;

  explicit ObservableContainer(QObject* parent = nullptr) : ContainerSignals(parent) {}
  explicit ObservableContainer(C items, QObject* parent = nullptr) : ContainerSignals(parent), m_items(std::move(items)) {}

  const C& items() const { return m_items; }
  qsizetype size() const { return Traits::size(m_items); }
  bool contains(const Key& key) const { return Traits::contains(m_items, key); }
  const Value& value(const Key& key) const { return Traits::value(m_items, key); }

  // Adds an entry. A list takes any index up to its size; a map, hash or set takes a key it does not hold.
  bool insert(const Key& key, Value value = Value{}) {
    if (!Traits::can_insert(m_items, key)) return false;
    emit entryAboutToBeAdded(QVariant::fromValue(key));
    Traits::insert(m_items, key, std::move(value));
    emit entryAdded(QVariant::fromValue(key));
    return true;
  }

  bool remove(const Key& key) {
    if (!Traits::contains(m_items, key)) return false;
    emit entryAboutToBeRemoved(QVariant::fromValue(key));
    Traits::erase(m_items, key);
    emit entryRemoved(QVariant::fromValue(key));
    return true;
  }

  // Replaces the value of an entry that is there. A set has no value to replace.
  bool replace(const Key& key, Value value) {
    if constexpr (!Traits::assignable) {
      return false;
    } else {
      if (!Traits::contains(m_items, key)) return false;
      emit entryAboutToBeChanged(QVariant::fromValue(key));
      Traits::assign(m_items, key, std::move(value));
      emit entryChanged(QVariant::fromValue(key));
      return true;
    }
  }

  // Moves the entry at `from` so that it ends up at `to`. Only a list has an order to move within.
  bool move(const Key& from, const Key& to) {
    if constexpr (!Traits::movable) {
      return false;
    } else {
      if (!Traits::can_move(m_items, from, to)) return false;
      emit entryAboutToBeMoved(QVariant::fromValue(from), QVariant::fromValue(to));
      Traits::move(m_items, from, to);
      emit entryMoved(QVariant::fromValue(from), QVariant::fromValue(to));
      return true;
    }
  }

  // Removes every entry. An empty container announces nothing.
  bool clear() {
    if (Traits::size(m_items) == 0) return false;
    emit aboutToBeCleared();
    m_items.clear();
    emit cleared();
    return true;
  }

 private:
  C m_items;
};

}  // namespace observable_qv
