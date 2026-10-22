// A Qt container that announces every change before it happens and confirms it after, with typed signals.
// One class template serves every container that ContainerTraits describes. moc cannot declare signals in
// a class template; RQT_OBJECT can, so the key travels in the signals as its own type.
//
// The announcing is written once, in the mutators below, so an adapter cannot forget half of a pair.
// A mutator that is asked for something invalid (an index out of range, a key already present, a
// replace of a missing key, a move in a container that has no order) returns false, changes nothing
// and emits nothing.
//
// A mutator called from a receiver of this container's own signals (the "about" or the "done" signal of any change,
// or a signal of a model that follows it) is refused like an invalid request: it returns false (0, or an empty
// container, for a batch; resetWith, which returns nothing, does nothing), changes nothing and emits nothing, and a
// debug build asserts. A receiver that wants a further change queues it (QMetaObject::invokeMethod with
// Qt::QueuedConnection) or makes it after the call that is announcing has returned.
//
// A gesture on many entries is one event. insertMany, removeMany and takeMany (a map, hash or set) announce the
// list of keys once, and resetWith replaces the whole content between one pair of signals, so a listener never
// sees the empty state that clear followed by inserts would show. The data pair carries the roles that changed,
// and an empty list means all of them.
#pragma once

#include "container_traits.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QThread>

#include <set>
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

  // True while one of the mutators below is announcing, that is between its first signal and its last. A mutator that
  // is called then is a re-entrant change: the models would be told of a change inside a change.
  bool announcing() const { return m_announcing; }

  // Adds an entry. A list takes any index up to its size; a map, hash or set takes a key it does not hold.
  bool insert(const Key& key, Value value = Value{}) {
    assertOwnThread();
    if (refusedAsNested() || !Traits::can_insert(m_items, key)) return false;
    const Announcing announcing(*this);
    emit entryAboutToBeAdded(key);
    Traits::insert(m_items, key, std::move(value));
    emit entryAdded(key);
    return true;
  }

  bool remove(const Key& key) {
    assertOwnThread();
    if (refusedAsNested() || !Traits::contains(m_items, key)) return false;
    const Announcing announcing(*this);
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
      if (refusedAsNested() || !Traits::contains(m_items, key)) return false;
      const Announcing announcing(*this);
      emit entryAboutToBeChanged(key, {});
      Traits::assign(m_items, key, std::move(value));
      emit entryChanged(key, {});
      return true;
    }
  }

  // Changes the value of an entry in place: `edit` takes the value by reference. The change is announced with the
  // roles it touches, and an empty list means every role. `edit` runs on a copy before anything is announced, so
  // a throw from it changes nothing and emits nothing, and a listener at the "about" signal sees the old value.
  template <class Edit>
  bool editValue(const Key& key, Edit&& edit, QList<int> roles = {}) {
    if constexpr (!Traits::assignable) {
      return false;
    } else {
      assertOwnThread();
      if (refusedAsNested() || !Traits::contains(m_items, key)) return false;
      Value edited = Traits::value(m_items, key);
      std::forward<Edit>(edit)(edited);
      const Announcing announcing(*this);
      emit entryAboutToBeChanged(key, roles);
      Traits::assign(m_items, key, std::move(edited));
      emit entryChanged(key, roles);
      return true;
    }
  }

  // Moves the entry at `from` so that it ends up at `to`. Only a list has an order to move within.
  bool move(const Key& from, const Key& to) {
    if constexpr (!Traits::movable) {
      return false;
    } else {
      assertOwnThread();
      if (refusedAsNested() || !Traits::can_move(m_items, from, to)) return false;
      const Announcing announcing(*this);
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
      if (refusedAsNested() || values.isEmpty() || !Traits::can_insert(m_items, first)) return false;
      const Key last = first + values.size() - 1;
      const Announcing announcing(*this);
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
      if (refusedAsNested() || count <= 0 || !Traits::contains(m_items, first) || first + count > Traits::size(m_items)) return false;
      const Key last = first + count - 1;
      const Announcing announcing(*this);
      emit rangeAboutToBeRemoved(first, last);
      for (qsizetype i = 0; i < count; ++i) Traits::erase(m_items, first);
      emit rangeRemoved(first, last);
      return true;
    }
  }

  // Adds many entries at once to a map, hash or set: one announcement for the whole list of keys. Keys the
  // container already holds are skipped. Returns how many were added, and announces nothing when that is none.
  // A list has insertRange, and a multi container has no key to name an entry by before it exists: both return 0.
  qsizetype insertMany(const C& entries) {
    if constexpr (!batchable) {
      return 0;
    } else {
      assertOwnThread();
      if (refusedAsNested()) return 0;
      QList<Key> keys;
      for (const Key& key : Traits::keys(entries))
        if (Traits::can_insert(m_items, key)) keys.append(key);
      if (keys.isEmpty()) return 0;
      const Announcing announcing(*this);
      emit entriesAboutToBeAdded(keys);
      for (const Key& key : keys) Traits::insert(m_items, key, Traits::value(entries, key));
      emit entriesAdded(keys);
      return keys.size();
    }
  }

  // Removes many entries at once: absent keys and repeated keys are ignored, the keys that are left are announced
  // once in the order given. Returns how many were removed, and announces nothing when that is none.
  qsizetype removeMany(const QList<Key>& keys) {
    if constexpr (!batchable) {
      return 0;
    } else {
      assertOwnThread();
      if (refusedAsNested()) return 0;
      const QList<Key> present = presentKeys(keys);
      if (present.isEmpty()) return 0;
      const Announcing announcing(*this);
      emit entriesAboutToBeRemoved(present);
      for (const Key& key : present) Traits::erase(m_items, key);
      emit entriesRemoved(present);
      return present.size();
    }
  }

  // Like removeMany, and returns the removed entries as a container, which is empty when nothing was removed.
  // `to.insertMany(from.takeMany(keys))` moves entries between two containers, one announcement on each side.
  C takeMany(const QList<Key>& keys) {
    C taken;
    if constexpr (batchable) {
      assertOwnThread();
      if (refusedAsNested()) return taken;
      const QList<Key> present = presentKeys(keys);
      if (present.isEmpty()) return taken;
      for (const Key& key : present) Traits::insert(taken, key, Traits::value(m_items, key));
      const Announcing announcing(*this);
      emit entriesAboutToBeRemoved(present);
      for (const Key& key : present) Traits::erase(m_items, key);
      emit entriesRemoved(present);
    }
    return taken;
  }

  // Replaces the whole content in one step, between one pair of signals. It always announces, even when the old
  // and the new content are both empty, and no listener ever sees a container that is empty in between.
  void resetWith(C content) {
    assertOwnThread();
    if (refusedAsNested()) return;
    const Announcing announcing(*this);
    emit aboutToBeReset();
    m_items = std::move(content);
    emit resetCompleted();
  }

  // Removes every entry. An empty container announces nothing.
  bool clear() {
    assertOwnThread();
    if (refusedAsNested() || Traits::size(m_items) == 0) return false;
    const Announcing announcing(*this);
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
  rqt::signal<void(const Key&, const QList<int>&)> entryAboutToBeChanged;  // roles that change, empty: all
  rqt::signal<void(const Key&, const QList<int>&)> entryChanged;
  rqt::signal<void(const Key&, const Key&)> entryAboutToBeMoved;
  rqt::signal<void(const Key&, const Key&)> entryMoved;
  rqt::signal<void(const Key&, const Key&)> rangeAboutToBeAdded;  // first and last key, a list only
  rqt::signal<void(const Key&, const Key&)> rangeAdded;
  rqt::signal<void(const Key&, const Key&)> rangeAboutToBeRemoved;
  rqt::signal<void(const Key&, const Key&)> rangeRemoved;
  rqt::signal<void(const QList<Key>&)> entriesAboutToBeAdded;  // a map, hash or set
  rqt::signal<void(const QList<Key>&)> entriesAdded;
  rqt::signal<void(const QList<Key>&)> entriesAboutToBeRemoved;
  rqt::signal<void(const QList<Key>&)> entriesRemoved;
  rqt::signal<void()> aboutToBeCleared;
  rqt::signal<void()> cleared;
  rqt::signal<void()> aboutToBeReset;
  rqt::signal<void()> resetCompleted;

 private:
  // Marks the container as announcing for the length of one mutator, also when a receiver throws.
  class Announcing {
   public:
    explicit Announcing(ObservableContainer& container) : m_container(container) { m_container.m_announcing = true; }
    ~Announcing() { m_container.m_announcing = false; }
    Announcing(const Announcing&) = delete;
    Announcing& operator=(const Announcing&) = delete;

   private:
    ObservableContainer& m_container;
  };

  // True when the call has to be refused because a change of this container is being announced.
  bool refusedAsNested() const {
    Q_ASSERT_X(!m_announcing, "observable::ObservableContainer",
               "a container must not be changed from a receiver of its own signals (queue the change, or make it after "
               "the announcing call has returned)");
    return m_announcing;
  }

  // Batches name an entry by a key it does not hold yet, which a list (it has ranges) and a multi container (its
  // keys are numbered from the newest) cannot do.
  static constexpr bool batchable = Traits::order == Order::sorted || Traits::order == Order::unordered;

  // The keys the container holds, each once, in the order given.
  QList<Key> presentKeys(const QList<Key>& keys) const {
    QList<Key> out;
    if constexpr (Traits::order == Order::sorted) {
      std::set<Key> seen;  // a map orders its keys, so they compare, but they need not hash
      for (const Key& key : keys)
        if (Traits::contains(m_items, key) && seen.insert(key).second) out.append(key);
    } else {
      QSet<Key> seen;
      for (const Key& key : keys)
        if (Traits::contains(m_items, key) && !seen.contains(key)) {
          seen.insert(key);
          out.append(key);
        }
    }
    return out;
  }

  C m_items;
  bool m_announcing = false;
};

}  // namespace observable
