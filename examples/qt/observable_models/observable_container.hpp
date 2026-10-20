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

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QThread>

#include <type_traits>
#include <utility>

namespace observable {

template <class C>
class ObservableContainer : public QObject {
  RQT_OBJECT

 public:
  using Traits = ContainerTraits<C>;
  using Key = typename Traits::Key;
  using Value = typename Traits::Value;

  // A model begins its row change at the "about" signal and ends it at the other one. A Qt container allocates
  // inside the insert, so nothing can be prepared before the announcement, and Qt does not support an exception
  // that leaves a slot half done. What can be refused is a value whose own move can throw.
  static_assert(std::is_nothrow_move_constructible_v<Value> && std::is_nothrow_move_assignable_v<Value>,
                "observable::ObservableContainer: the value type must be nothrow move constructible and assignable, "
                "because a throw between the 'about' and the 'done' signal leaves every model mid-change");

  explicit ObservableContainer(QObject* parent = nullptr) : QObject(parent) {}
  explicit ObservableContainer(C items, QObject* parent = nullptr) : QObject(parent), m_items(std::move(items)) {}

  const C& items() const { return m_items; }
  qsizetype size() const { return Traits::size(m_items); }
  bool contains(const Key& key) const { return Traits::contains(m_items, key); }
  const Value& value(const Key& key) const { return Traits::value(m_items, key); }

  // A container and the models that follow it live in one thread: the signals are delivered directly, and a model
  // that heard a change from another thread would read the container mid-change. Debug builds assert it.
  void assertOwnThread() const {
    Q_ASSERT_X(QThread::currentThread() == thread(), "observable::ObservableContainer",
               "a container must be changed from the thread it lives in (move data between threads, not the model)");
  }

  // Adds an entry. A list takes any index up to its size; a map, hash or set takes a key it does not hold.
  bool insert(const Key& key, Value value = Value{}) {
    assertOwnThread();
    if (!Traits::can_insert(m_items, key)) return false;
    emit entryAboutToBeAdded(key);
    Traits::insert(m_items, key, std::move(value));
    emit entryAdded(key);
    return true;
  }

  bool remove(const Key& key) {
    assertOwnThread();
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
      assertOwnThread();
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
      assertOwnThread();
      if (!Traits::can_move(m_items, from, to)) return false;
      emit entryAboutToBeMoved(from, to);
      Traits::move(m_items, from, to);
      emit entryMoved(from, to);
      return true;
    }
  }

  // Adds a run of entries at once, for a list: one announcement, so a model begins and ends one change however
  // long the run is. Only a list has contiguous positions to announce; anything else returns false.
  bool insertRange(const Key& first, QList<Value> values) {
    if constexpr (Traits::order != Order::indexed) {
      return false;
    } else {
      assertOwnThread();
      if (values.isEmpty() || !Traits::can_insert(m_items, first)) return false;
      const Key last = first + values.size() - 1;
      emit rangeAboutToBeAdded(first, last);
      for (qsizetype i = 0; i < values.size(); ++i) Traits::insert(m_items, first + i, std::move(values[i]));
      emit rangeAdded(first, last);
      return true;
    }
  }

  // Removes `count` entries from `first` on, for a list, in one announcement.
  bool removeRange(const Key& first, qsizetype count) {
    if constexpr (Traits::order != Order::indexed) {
      return false;
    } else {
      assertOwnThread();
      if (count <= 0 || !Traits::contains(m_items, first) || first + count > Traits::size(m_items)) return false;
      const Key last = first + count - 1;
      emit rangeAboutToBeRemoved(first, last);
      for (qsizetype i = 0; i < count; ++i) Traits::erase(m_items, first);
      emit rangeRemoved(first, last);
      return true;
    }
  }

  // Removes every entry. An empty container announces nothing.
  bool clear() {
    assertOwnThread();
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
  rqt::signal<void(const Key&, const Key&)> rangeAboutToBeAdded;  // first and last key, a list only
  rqt::signal<void(const Key&, const Key&)> rangeAdded;
  rqt::signal<void(const Key&, const Key&)> rangeAboutToBeRemoved;
  rqt::signal<void(const Key&, const Key&)> rangeRemoved;
  rqt::signal<void()> aboutToBeCleared;
  rqt::signal<void()> cleared;

 private:
  C m_items;
};

}  // namespace observable
