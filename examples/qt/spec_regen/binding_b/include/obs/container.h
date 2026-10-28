// obs::Container<C>: an observable container that announces every change before and after (SPEC 5, 7),
// binding B: the class template declares its own typed signals through reflect-moc, no moc input.
#pragma once

#include "container_signals.h"
#include "traits.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>

#include <deque>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>
#include <version>

// A queued call keeps the values and the callable it was given, which may be move-only (a value that owns a resource, a lambda
// that captures a std::unique_ptr), so std::function, which needs a copyable callable, would not do.
#ifndef __cpp_lib_move_only_function
#error "obs::Container queues calls that own move-only values and callables: it needs std::move_only_function (__cpp_lib_move_only_function)"
#endif

namespace obs {
namespace detail {

template <class C>
inline constexpr bool isQtAssociative = requires(const C & c, const KeyOf<C> & k) { c.constFind(k); };

template <class C>
const ValueOf<C> * lookup(const C & c, const KeyOf<C> & key)
{
    if constexpr (isQtAssociative<C>) {
        const auto it = c.constFind(key);
        return it == c.constEnd() ? nullptr : &it.value();
    } else {
        const auto it = c.find(key);
        return it == c.end() ? nullptr : &it->second;
    }
}

template <class C>
ValueOf<C> * lookupMutable(C & c, const KeyOf<C> & key)
{
    if constexpr (isQtAssociative<C>) {
        const auto it = c.find(key);
        return it == c.end() ? nullptr : &it.value();
    } else {
        const auto it = c.find(key);
        return it == c.end() ? nullptr : &it->second;
    }
}

template <class C>
void assocInsert(C & c, const KeyOf<C> & key, ValueOf<C> && value)
{
    if constexpr (isQtAssociative<C>)
        c.insert(key, std::move(value));
    else
        c.emplace(key, std::move(value));
}

template <class C>
void assocErase(C & c, const KeyOf<C> & key)
{
    if constexpr (isQtAssociative<C>)
        c.remove(key);
    else
        c.erase(key);
}

// f(key, value) for every entry, in the container's own iteration order
template <class C, class F>
void forEachEntry(const C & c, F && f)
{
    if constexpr (isQtAssociative<C>) {
        for (auto it = c.constBegin(); it != c.constEnd(); ++it)
            f(it.key(), it.value());
    } else {
        for (const auto & [key, value] : c)
            f(key, value);
    }
}

// the same with a mutable value, so a batch can move values out of its argument
template <class C, class F>
void forEachEntryMutable(C & c, F && f)
{
    if constexpr (isQtAssociative<C>) {
        for (auto it = c.begin(); it != c.end(); ++it)
            f(it.key(), it.value());
    } else {
        for (auto & [key, value] : c)
            f(key, value);
    }
}

// the caller's keys without duplicates and without keys not in c, in the caller's order
template <class C>
QList<KeyOf<C>> presentKeysOnce(const C & c, const QList<KeyOf<C>> & keys)
{
    using Key = KeyOf<C>;
    QList<Key> out;
    if constexpr (HashableKey<Key>) {
        QHash<Key, char> seen;
        for (const Key & k : keys)
            if (lookup(c, k) != nullptr && !seen.contains(k)) {
                seen.insert(k, char{1});
                out.append(k);
            }
    } else {
        for (const Key & k : keys) {
            bool dup = false;
            for (const Key & o : out)
                dup = dup || o == k;
            if (lookup(c, k) != nullptr && !dup)
                out.append(k);
        }
    }
    return out;
}

}  // namespace detail

// A mutating call opens a notification scope before its "about" and closes it when it returns, after the twin; the container is
// notifying (isNotifying()) while a scope is open. A mutating call made on THIS container while it is notifying is not applied:
// it is queued with copies of its arguments and returns at once, true for a bool function and the number of keys given for
// insertMany and removeMany (what it will do is decided at its turn). When the outermost scope closes, the queued calls are
// applied one by one in the order they were queued, each against the state it then finds, before that outermost call returns;
// the calls their receivers queue go to the back of the same queue. take and takeMany do nothing while the container is
// notifying: they return std::nullopt and an empty C. A call on another container follows that container's own state.
// Do not destroy the container from a receiver of an "about": the mutation still has to follow.
template <class C>
    requires SupportedContainer<C>
class Container : public QObject {
    RQT_OBJECT

public:
    using Key = KeyOf<C>;
    using Value = ValueOf<C>;

    Container() = default;
    explicit Container(C initial, QObject * parent = nullptr) : QObject(parent), data_(std::move(initial)) {}

signals:
    rqt::signal<void(const Key &)> entryAboutToBeAdded;
    rqt::signal<void(const Key &)> entryAdded;
    rqt::signal<void(const Key &)> entryAboutToBeRemoved;
    rqt::signal<void(const Key &)> entryRemoved;
    rqt::signal<void(const Key &, const QList<int> &)> entryDataAboutToChange;
    rqt::signal<void(const Key &, const QList<int> &)> entryDataChanged;
    rqt::signal<void(const QList<Key> &)> entriesAboutToBeAdded;
    rqt::signal<void(const QList<Key> &)> entriesAdded;
    rqt::signal<void(const QList<Key> &)> entriesAboutToBeRemoved;
    rqt::signal<void(const QList<Key> &)> entriesRemoved;
    rqt::signal<void()> aboutToBeCleared;
    rqt::signal<void()> cleared;
    rqt::signal<void()> aboutToBeReset;
    rqt::signal<void()> resetCompleted;

public:
    // true while a scope is open: a mutating call made now is queued (take and takeMany are refused)
    [[nodiscard]] bool isNotifying() const noexcept { return link_.running != nullptr && link_.running->depth > 0; }

    const C & data() const { return data_; }
    qsizetype size() const { return static_cast<qsizetype>(data_.size()); }

    bool contains(const Key & key) const { return find(key) != nullptr; }

    const Value * find(const Key & key) const
    {
        if constexpr (SequentialContainer<C>)
            return validIndex(data_, key) ? &data_[static_cast<typename C::size_type>(key)] : nullptr;
        else
            return detail::lookup(data_, key);
    }

    // sequential: the key is the index
    bool insert(qsizetype index, Value value)
        requires SequentialContainer<C>
    {
        if (isNotifying())
            return defer([this, index, value = std::move(value)]() mutable { insert(index, std::move(value)); });
        if (index < 0 || index > size())
            return false;
        const Scope scope{*this};
        entryAboutToBeAdded(index);
        data_.insert(data_.begin() + index, std::move(value));
        entryAdded(index);
        return true;
    }

    bool append(Value value)
        requires SequentialContainer<C>
    {
        if (isNotifying())
            return defer([this, value = std::move(value)]() mutable { append(std::move(value)); });
        return insert(size(), std::move(value));
    }

    // associative: a present key is NOT replaced
    bool insert(const Key & key, Value value)
        requires AssociativeContainer<C>
    {
        if (isNotifying())
            return defer([this, key, value = std::move(value)]() mutable { insert(key, std::move(value)); });
        if (contains(key))
            return false;
        const Scope scope{*this};
        entryAboutToBeAdded(key);
        detail::assocInsert(data_, key, std::move(value));
        entryAdded(key);
        return true;
    }

    bool remove(const Key & key)
    {
        if (isNotifying())
            return defer([this, key] { remove(key); });
        if (!contains(key))
            return false;
        const Scope scope{*this};
        entryAboutToBeRemoved(key);
        eraseKey(key);
        entryRemoved(key);
        return true;
    }

    std::optional<Value> take(const Key & key)
    {
        if (isNotifying())
            return std::nullopt;  // the result is the value: a delayed take would have nobody to hand it to
        if (!contains(key))
            return std::nullopt;
        const Scope scope{*this};
        entryAboutToBeRemoved(key);
        std::optional<Value> taken{std::move(*valueOf(key))};  // looked up again after the "about"
        eraseKey(key);
        entryRemoved(key);
        return taken;
    }

    bool replace(const Key & key, Value value)
    {
        if (isNotifying())
            return defer([this, key, value = std::move(value)]() mutable { replace(key, std::move(value)); });
        if (!contains(key))
            return false;
        const Scope scope{*this};
        entryDataAboutToChange(key, QList<int>{});
        *valueOf(key) = std::move(value);
        entryDataChanged(key, QList<int>{});
        return true;
    }

    // Calls f(Value &) between the "about" and the twin. While the container is notifying the call is queued: f is kept and
    // called when its turn comes, possibly after the code that made the call has moved on, so f must own what it captures. A
    // lambda that captures a local variable of the caller by reference finds it dead; the library cannot check this. A callable
    // that cannot be copied must be passed as an rvalue.
    template <class F>
    bool editValue(const Key & key, F && f, const QList<int> & roles = {})
    {
        if (isNotifying()) {
            static_assert(std::is_constructible_v<std::decay_t<F>, F>,
                          "editValue: while the container is notifying the call is queued, so the callable is copied or moved into the queue; pass a callable that cannot be copied as an rvalue");
            return defer([this, key, roles, f = std::forward<F>(f)]() mutable { editValue(key, std::move(f), roles); });
        }
        if (!contains(key))
            return false;
        const Scope scope{*this};
        entryDataAboutToChange(key, roles);
        std::forward<F>(f)(*valueOf(key));  // looked up again after the "about"
        entryDataChanged(key, roles);
        return true;
    }

    void clear()
    {
        if (isNotifying()) {
            defer([this] { clear(); });
            return;
        }
        if (size() == 0)
            return;
        const Scope scope{*this};
        aboutToBeCleared();
        data_.clear();
        cleared();
    }

    // the atomic replacement: one move-assignment between the two notifications, never clear() plus inserts
    void resetWith(C content)
    {
        if (isNotifying()) {
            defer([this, content = std::move(content)]() mutable { resetWith(std::move(content)); });
            return;
        }
        const Scope scope{*this};
        aboutToBeReset();
        data_ = std::move(content);
        resetCompleted();
    }

    // associative only: on a sequential C the static_assert fires at the call
    template <class Self = C>
    qsizetype insertMany(C entries)
    {
        static_assert(AssociativeContainer<Self>, "insertMany: batches need an associative container, a batch of positions is ambiguous");
        if (isNotifying()) {
            const auto requested = static_cast<qsizetype>(entries.size());
            defer([this, entries = std::move(entries)]() mutable { insertMany(std::move(entries)); });
            return requested;
        }
        QList<Key> keys;
        std::vector<std::pair<Key, Value>> fresh;
        detail::forEachEntryMutable(entries, [&](const Key & key, Value & value) {
            if (!contains(key)) {
                keys.append(key);
                fresh.emplace_back(key, std::move(value));
            }
        });
        if (keys.isEmpty())
            return 0;
        const Scope scope{*this};
        entriesAboutToBeAdded(keys);
        for (auto & [key, value] : fresh)
            detail::assocInsert(data_, key, std::move(value));
        entriesAdded(keys);
        return keys.size();
    }

    template <class Self = C>
    qsizetype removeMany(const QList<Key> & keys)
    {
        static_assert(AssociativeContainer<Self>, "removeMany: batches need an associative container, a batch of positions is ambiguous");
        if (isNotifying()) {
            defer([this, keys] { removeMany(keys); });
            return keys.size();
        }
        const QList<Key> present = detail::presentKeysOnce(data_, keys);
        if (present.isEmpty())
            return 0;
        const Scope scope{*this};
        entriesAboutToBeRemoved(present);
        for (const Key & key : present)
            eraseKey(key);
        entriesRemoved(present);
        return present.size();
    }

    template <class Self = C>
    C takeMany(const QList<Key> & keys)
    {
        static_assert(AssociativeContainer<Self>, "takeMany: batches need an associative container, a batch of positions is ambiguous");
        if (isNotifying())
            return C{};
        const QList<Key> present = detail::presentKeysOnce(data_, keys);
        C taken;
        if (present.isEmpty())
            return taken;
        const Scope scope{*this};
        entriesAboutToBeRemoved(present);
        for (const Key & key : present) {
            detail::assocInsert(taken, key, std::move(*valueOf(key)));
            eraseKey(key);
        }
        entriesRemoved(present);
        return taken;
    }

private:
    using QueuedCall = std::move_only_function<void()>;

    // The state of one outermost call and of the calls it drains. It lives on the stack of that call, not in the container, so a
    // receiver that destroys the container leaves the call with nothing of the container to touch.
    struct Gesture {
        std::unique_ptr<std::deque<QueuedCall>> queue;  // made by the first queued call
        int depth = 0;                                  // open scopes (N1)
        bool containerGone = false;                     // a receiver destroyed the container
    };

    // The container's only trace of a running gesture: where it is, and how to tell it that the container is gone.
    struct Link {
        Gesture * running = nullptr;

        Link() = default;
        Link(const Link &) = delete;
        Link & operator=(const Link &) = delete;
        ~Link()
        {
            if (running != nullptr)
                running->containerGone = true;
        }
    };

    // N1: opened before a mutating call emits its "about", closed when the call returns. The first scope of an idle container
    // owns the gesture and drains the queue when it closes (N3); a scope opened by a drained call joins that gesture and
    // drains nothing, so the drain never recurses (N4).
    class Scope {
    public:
        explicit Scope(Container & container) : container_(container), uncaught_(std::uncaught_exceptions())
        {
            if (container_.link_.running == nullptr) {
                container_.link_.running = &own_;
                owner_ = true;
            }
            joined_ = container_.link_.running;
            ++joined_->depth;
        }

        Scope(const Scope &) = delete;
        Scope & operator=(const Scope &) = delete;

        // It may throw what a receiver of a drained call throws, but never while another exception is unwinding: then the gesture
        // just ends and the calls still queued are dropped, so no receiver is called from here during unwinding.
        ~Scope() noexcept(false)
        {
            if (joined_->containerGone)
                return;
            --joined_->depth;
            if (!owner_)
                return;
            const Ending ending{container_.link_, own_};
            if (std::uncaught_exceptions() == uncaught_)
                drain(own_);
        }

    private:
        struct Ending {
            Ending(Link & link, Gesture & gesture) : link_(link), gesture_(gesture) {}
            Ending(const Ending &) = delete;
            Ending & operator=(const Ending &) = delete;
            ~Ending()
            {
                if (!gesture_.containerGone)
                    link_.running = nullptr;
            }

            Link & link_;
            Gesture & gesture_;
        };

        Container & container_;
        const int uncaught_;
        Gesture own_;
        Gesture * joined_ = nullptr;
        bool owner_ = false;
    };

    // N3: first in, first out, until the queue is empty; the calls queued meanwhile join the back. A call leaves the queue before
    // it runs, so what its receivers queue cannot disturb it. It touches the gesture only: the container may be gone after a call.
    static void drain(Gesture & gesture)
    {
        while (!gesture.containerGone && gesture.queue && !gesture.queue->empty()) {
            QueuedCall call = std::move(gesture.queue->front());
            gesture.queue->pop_front();
            call();
        }
    }

    // N2: keep a call for the drain. It reports that the call was queued, never that it was applied (N8).
    template <class Call>
    bool defer(Call && call)
    {
        Q_ASSERT(link_.running != nullptr);
        Gesture & gesture = *link_.running;
        if (!gesture.queue)
            gesture.queue = std::make_unique<std::deque<QueuedCall>>();
        gesture.queue->push_back(QueuedCall(std::forward<Call>(call)));
        return true;
    }

    Value * valueOf(const Key & key)
    {
        if constexpr (SequentialContainer<C>)
            return &data_[static_cast<typename C::size_type>(key)];
        else
            return detail::lookupMutable(data_, key);
    }

    void eraseKey(const Key & key)
    {
        if constexpr (SequentialContainer<C>)
            data_.erase(data_.begin() + key);
        else
            detail::assocErase(data_, key);
    }

    C data_{};
    Link link_;
};

}  // namespace obs
