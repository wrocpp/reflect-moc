// A Qt container that announces every change before it happens and confirms it after, with typed signals.
// One class template serves every container that ContainerTraits describes. moc cannot declare signals in
// a class template; RQT_OBJECT can, so the key travels in the signals as its own type.
//
// The announcing is written once, in the mutators below, so an adapter cannot forget half of a pair.
// A mutator that is asked for something invalid (an index out of range, a key already present, a
// replace of a missing key, a move in a container that has no order) returns false, changes nothing
// and emits nothing.
#pragma once

#include "container_traits.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QObject>

#include <utility>

namespace observable {

template <class C>
class ObservableContainer : public QObject {
  RQT_OBJECT

 public:
  using Traits = ContainerTraits<C>;
  using Key = typename Traits::Key;
  using Value = typename Traits::Value;

  explicit ObservableContainer(QObject* parent = nullptr) : QObject(parent) {}
  explicit ObservableContainer(C items, QObject* parent = nullptr) : QObject(parent), m_items(std::move(items)) {}

  const C& items() const { return m_items; }
  qsizetype size() const { return Traits::size(m_items); }
  bool contains(const Key& key) const { return Traits::contains(m_items, key); }
  const Value& value(const Key& key) const { return Traits::value(m_items, key); }

  // Adds an entry. A list takes any index up to its size; a map, hash or set takes a key it does not hold.
  bool insert(const Key& key, Value value = Value{}) {
    if (!Traits::can_insert(m_items, key)) return false;
    emit entryAboutToBeAdded(key);
    Traits::insert(m_items, key, std::move(value));
    emit entryAdded(key);
    return true;
  }

  bool remove(const Key& key) {
    if (!Traits::contains(m_items, key)) return false;
    emit entryAboutToBeRemoved(key);
    Traits::erase(m_items, key);
    emit entryRemoved(key);
    return true;
  }

  // Replaces the value of an entry that is there. A set has no value to replace.
  bool replace(const Key& key, Value value) {
    if constexpr (!Traits::assignable) {
      return false;
    } else {
      if (!Traits::contains(m_items, key)) return false;
      emit entryAboutToBeChanged(key);
      Traits::assign(m_items, key, std::move(value));
      emit entryChanged(key);
      return true;
    }
  }

  // Moves the entry at `from` so that it ends up at `to`. Only a list has an order to move within.
  bool move(const Key& from, const Key& to) {
    if constexpr (!Traits::movable) {
      return false;
    } else {
      if (!Traits::can_move(m_items, from, to)) return false;
      emit entryAboutToBeMoved(from, to);
      Traits::move(m_items, from, to);
      emit entryMoved(from, to);
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

 signals:
  rqt::signal<void(const Key&)> entryAboutToBeAdded;
  rqt::signal<void(const Key&)> entryAdded;
  rqt::signal<void(const Key&)> entryAboutToBeRemoved;
  rqt::signal<void(const Key&)> entryRemoved;
  rqt::signal<void(const Key&)> entryAboutToBeChanged;
  rqt::signal<void(const Key&)> entryChanged;
  rqt::signal<void(const Key&, const Key&)> entryAboutToBeMoved;
  rqt::signal<void(const Key&, const Key&)> entryMoved;
  rqt::signal<void()> aboutToBeCleared;
  rqt::signal<void()> cleared;

 private:
  C m_items;
};

}  // namespace observable
