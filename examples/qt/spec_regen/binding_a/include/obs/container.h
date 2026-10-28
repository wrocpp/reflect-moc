#pragma once

#include "obs/container_signals.h"
#include "obs/traits.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QVariant>

#include <deque>
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace obs {

namespace detail {

template <class K, class V>
const V * findValue(const QHash<K, V> & c, const K & k)
{
    const auto it = c.constFind(k);
    return it == c.constEnd() ? nullptr : &it.value();
}

template <class K, class V>
const V * findValue(const QMap<K, V> & c, const K & k)
{
    const auto it = c.constFind(k);
    return it == c.constEnd() ? nullptr : &it.value();
}

template <class K, class V, class Cmp, class A>
const V * findValue(const std::map<K, V, Cmp, A> & c, const K & k)
{
    const auto it = c.find(k);
    return it == c.end() ? nullptr : &it->second;
}

// Mutable access to a value known to be present. Never goes through a const
// reference: QHash and QMap are implicitly shared and must detach first.
template <class K, class V>
V & mutableValue(QHash<K, V> & c, const K & k)
{
    return c.find(k).value();
}

template <class K, class V>
V & mutableValue(QMap<K, V> & c, const K & k)
{
    return c.find(k).value();
}

template <class K, class V, class Cmp, class A>
V & mutableValue(std::map<K, V, Cmp, A> & c, const K & k)
{
    return c.find(k)->second;
}

// std::map and QHash emplace; QMap (in Qt 6.10) only has insert(key, value).
template <class C, class K, class V>
void insertEntry(C & c, const K & k, V && v)
{
    if constexpr (requires { c.emplace(k, std::forward<V>(v)); })
        c.emplace(k, std::forward<V>(v));
    else
        c.insert(k, std::forward<V>(v));
}

template <class C, class F>
void forEachEntry(C & c, F && f)
{
    if constexpr (requires { c.begin().key(); }) {
        for (auto it = c.begin(); it != c.end(); ++it)
            f(it.key(), it.value());
    } else {
        for (auto & entry : c)
            f(entry.first, entry.second);
    }
}

template <class C, class F>
void forEachKey(const C & c, F && f)
{
    if constexpr (requires { c.constBegin().key(); }) {
        for (auto it = c.constBegin(); it != c.constEnd(); ++it)
            f(it.key());
    } else {
        for (const auto & entry : c)
            f(entry.first);
    }
}

}  // namespace detail

// A mutating function opens a notification scope before its "about" and closes it after its twin. A mutating call made on the
// container while a scope is open is queued, with its arguments, and the call returns at once; the outermost call applies
// the queue in order before it returns (addendum N1-N4). take and takeMany refuse instead (N9).
template <class C>
    requires SupportedContainer<C> && VariantKey<KeyOf<C>>
class Container : public ContainerSignals {
public:
    using Key = KeyOf<C>;
    using Value = ValueOf<C>;

    Container() = default;
    explicit Container(C initial, QObject * parent = nullptr)
        : ContainerSignals(parent), m_data(std::move(initial))
    {}

    const C & data() const { return m_data; }
    qsizetype size() const { return static_cast<qsizetype>(m_data.size()); }

    bool contains(const Key & key) const { return find(key) != nullptr; }

    const Value * find(const Key & key) const
    {
        if constexpr (SequentialContainer<C>)
            return validIndex(m_data, key) ? &m_data[static_cast<std::size_t>(key)] : nullptr;
        else
            return detail::findValue(m_data, key);
    }

    // True while a mutating function of this container is between the start of its "about" and the end of its twin, receivers
    // included. A mutating call made then is queued (N2).
    [[nodiscard]] bool isNotifying() const noexcept { return m_scopes > 0; }

    // sequential only
    bool insert(qsizetype index, Value value)
        requires SequentialContainer<C>
    {
        return submitInsert(index, std::move(value));
    }

    // The end is read when the call is applied, so a queued append still goes to the end.
    bool append(Value value)
        requires SequentialContainer<C>
    {
        if (isNotifying()) {
            enqueue([queued = std::move(value)](Container & self) mutable { self.applyInsert(self.size(), std::move(queued)); });
            return true;
        }
        const bool done = applyInsert(size(), std::move(value));
        drainQueue();
        return done;
    }

    // associative only
    bool insert(const Key & key, Value value)
        requires AssociativeContainer<C>
    {
        return submitInsert(key, std::move(value));
    }

    bool remove(const Key & key)
    {
        if (isNotifying()) {
            enqueue([key](Container & self) { self.applyRemove(key); });
            return true;
        }
        const bool done = applyRemove(key);
        drainQueue();
        return done;
    }

    // Refused while the container is notifying: the result is the value, and a queued take would have nobody to give it to (N9).
    std::optional<Value> take(const Key & key)
    {
        if (isNotifying())
            return std::nullopt;
        std::optional<Value> taken = applyTake(key);
        drainQueue();
        return taken;
    }

    bool replace(const Key & key, Value value)
    {
        if (isNotifying()) {
            enqueue([key, queued = std::move(value)](Container & self) mutable { self.applyReplace(key, std::move(queued)); });
            return true;
        }
        const bool done = applyReplace(key, std::move(value));
        drainQueue();
        return done;
    }

    // Calls f(Value &) between the "about" and the twin. While the container is notifying, f itself is queued and runs at its turn,
    // after the gesture that was running: f must own everything it uses. A lambda that captures a local variable of the caller by
    // reference finds it dead when it runs, and the container cannot check this (N10). f must be copy- or move-constructible for
    // the same reason.
    template <class F>
    bool editValue(const Key & key, F && f, const QList<int> & roles = {})
    {
        static_assert(std::is_constructible_v<std::decay_t<F>, F>,
                      "editValue: f is queued while the container notifies, so it must be copy- or move-constructible");
        if (isNotifying()) {
            enqueue([key, roles, edit = std::forward<F>(f)](Container & self) mutable {
                self.applyEditValue(key, std::move(edit), roles);
            });
            return true;
        }
        const bool done = applyEditValue(key, std::forward<F>(f), roles);
        drainQueue();
        return done;
    }

    void clear()
    {
        if (isNotifying()) {
            enqueue([](Container & self) { self.applyClear(); });
            return;
        }
        applyClear();
        drainQueue();
    }

    void resetWith(C content)
    {
        if (isNotifying()) {
            enqueue([queued = std::move(content)](Container & self) mutable { self.applyResetWith(std::move(queued)); });
            return;
        }
        applyResetWith(std::move(content));
        drainQueue();
    }

    // associative only; on a sequential C the call does not compile (7.2 B2)
    template <class Self = C>
    qsizetype insertMany(C entries)
    {
        static_assert(AssociativeContainer<Self>,
                      "insertMany: batch operations exist for associative containers only");
        if constexpr (AssociativeContainer<Self>) {
            if (isNotifying()) {
                const qsizetype requested = static_cast<qsizetype>(entries.size());
                enqueue([queued = std::move(entries)](Container & self) mutable { self.applyInsertMany(std::move(queued)); });
                return requested;
            }
            const qsizetype inserted = applyInsertMany(std::move(entries));
            drainQueue();
            return inserted;
        } else {
            return 0;
        }
    }

    template <class Self = C>
    qsizetype removeMany(const QList<Key> & keys)
    {
        static_assert(AssociativeContainer<Self>,
                      "removeMany: batch operations exist for associative containers only");
        if constexpr (AssociativeContainer<Self>) {
            if (isNotifying()) {
                enqueue([keys](Container & self) { self.applyRemoveMany(keys); });
                return keys.size();
            }
            const qsizetype removed = applyRemoveMany(keys);
            drainQueue();
            return removed;
        } else {
            return 0;
        }
    }

    // Refused while the container is notifying, like take (N9).
    template <class Self = C>
    C takeMany(const QList<Key> & keys)
    {
        static_assert(AssociativeContainer<Self>,
                      "takeMany: batch operations exist for associative containers only");
        C taken;
        if constexpr (AssociativeContainer<Self>) {
            if (isNotifying())
                return taken;
            taken = applyTakeMany(keys);
            drainQueue();
        }
        return taken;
    }

private:
    // A call waiting for its turn. Type-erased by hand: a std::function needs a copyable callable, and a queued call owns its
    // arguments, which may be move-only (std::move_only_function is C++23).
    class QueuedCall {
    public:
        QueuedCall() = default;
        QueuedCall(const QueuedCall &) = delete;
        QueuedCall & operator=(const QueuedCall &) = delete;
        virtual ~QueuedCall() = default;
        virtual void apply(Container & self) = 0;
    };

    template <class F>
    class QueuedCallOf final : public QueuedCall {
    public:
        explicit QueuedCallOf(F call) : m_call(std::move(call)) {}
        void apply(Container & self) override { m_call(self); }

    private:
        F m_call;
    };

    // N1: open from before the "about" of a mutating function to after its twin. When a receiver throws, the gesture is
    // abandoned: what the receivers had queued is dropped rather than applied by whatever gesture comes next.
    class Scope {
    public:
        explicit Scope(Container & owner) noexcept : m_owner(owner), m_exceptionsAtEntry(std::uncaught_exceptions())
        {
            ++m_owner.m_scopes;
        }
        Scope(const Scope &) = delete;
        Scope & operator=(const Scope &) = delete;
        ~Scope()
        {
            --m_owner.m_scopes;
            if (m_owner.m_scopes == 0 && std::uncaught_exceptions() > m_exceptionsAtEntry)
                m_owner.m_queue.clear();
        }

    private:
        Container & m_owner;
        const int m_exceptionsAtEntry;
    };

    // N3: set while the queue is being applied, so a gesture that opens and closes a scope during the drain does not start another.
    class Draining {
    public:
        explicit Draining(Container & owner) noexcept : m_owner(owner) { m_owner.m_draining = true; }
        Draining(const Draining &) = delete;
        Draining & operator=(const Draining &) = delete;
        ~Draining() { m_owner.m_draining = false; }

    private:
        Container & m_owner;
    };

    template <class F>
    void enqueue(F call)
    {
        m_queue.push_back(std::make_unique<QueuedCallOf<F>>(std::move(call)));
    }

    // Applies the queued calls first to last, once per outermost gesture. A call queued while the drain runs goes to the back
    // of the same queue (N4), so the loop sees it.
    void drainQueue()
    {
        if (m_draining)
            return;
        const Draining draining{*this};
        while (!m_queue.empty()) {
            const std::unique_ptr<QueuedCall> call = std::move(m_queue.front());
            m_queue.pop_front();
            call->apply(*this);
        }
    }

    bool submitInsert(const Key & key, Value value)
    {
        if (isNotifying()) {
            enqueue([key, queued = std::move(value)](Container & self) mutable { self.applyInsert(key, std::move(queued)); });
            return true;
        }
        const bool done = applyInsert(key, std::move(value));
        drainQueue();
        return done;
    }

    // The apply functions validate against the state they find (N5), emit, change and emit, each inside its own scope.
    bool applyInsert(const Key & key, Value value)
    {
        if constexpr (SequentialContainer<C>) {
            if (key < 0 || key > size())
                return false;
        } else {
            if (contains(key))
                return false;
        }
        const Scope scope{*this};
        emit entryAboutToBeAdded(detail::box(key));
        if constexpr (SequentialContainer<C>)
            m_data.insert(m_data.begin() + key, std::move(value));
        else
            detail::insertEntry(m_data, key, std::move(value));
        emit entryAdded(detail::box(key));
        return true;
    }

    bool applyRemove(const Key & key)
    {
        if (!contains(key))
            return false;
        const Scope scope{*this};
        emit entryAboutToBeRemoved(detail::box(key));
        eraseKnown(key);
        emit entryRemoved(detail::box(key));
        return true;
    }

    std::optional<Value> applyTake(const Key & key)
    {
        if (!contains(key))
            return std::nullopt;
        const Scope scope{*this};
        emit entryAboutToBeRemoved(detail::box(key));
        std::optional<Value> taken{std::move(valueRef(key))};
        eraseKnown(key);
        emit entryRemoved(detail::box(key));
        return taken;
    }

    bool applyReplace(const Key & key, Value value)
    {
        if (!contains(key))
            return false;
        const Scope scope{*this};
        emit entryDataAboutToChange(detail::box(key), QList<int>{});
        valueRef(key) = std::move(value);
        emit entryDataChanged(detail::box(key), QList<int>{});
        return true;
    }

    // The entry is looked up again after the "about" (A.5): valueRef does it at the point of use.
    template <class F>
    bool applyEditValue(const Key & key, F && f, const QList<int> & roles)
    {
        if (!contains(key))
            return false;
        const Scope scope{*this};
        emit entryDataAboutToChange(detail::box(key), roles);
        std::invoke(std::forward<F>(f), valueRef(key));
        emit entryDataChanged(detail::box(key), roles);
        return true;
    }

    void applyClear()
    {
        if (size() == 0)
            return;
        const Scope scope{*this};
        emit aboutToBeCleared();
        m_data.clear();
        emit cleared();
    }

    void applyResetWith(C content)
    {
        const Scope scope{*this};
        emit aboutToBeReset();
        m_data = std::move(content);
        emit resetCompleted();
    }

    // The three batch functions below exist for an associative C only; they are named from inside `if constexpr` branches.
    qsizetype applyInsertMany(C entries)
    {
        std::vector<std::pair<Key, Value>> fresh;
        QList<Key> keys;
        detail::forEachEntry(entries, [&](const Key & k, Value & v) {
            if (contains(k))
                return;
            keys.append(k);
            fresh.emplace_back(k, std::move(v));
        });
        if (fresh.empty())
            return 0;
        const Scope scope{*this};
        emit entriesAboutToBeAdded(detail::boxAll(keys));
        for (auto & [k, v] : fresh)
            detail::insertEntry(m_data, k, std::move(v));
        emit entriesAdded(detail::boxAll(keys));
        return static_cast<qsizetype>(fresh.size());
    }

    qsizetype applyRemoveMany(const QList<Key> & keys)
    {
        const QList<Key> doomed = detail::uniqueKeys(keys, [this](const Key & k) { return contains(k); });
        if (doomed.isEmpty())
            return 0;
        const Scope scope{*this};
        emit entriesAboutToBeRemoved(detail::boxAll(doomed));
        for (const Key & k : doomed)
            eraseKnown(k);
        emit entriesRemoved(detail::boxAll(doomed));
        return doomed.size();
    }

    C applyTakeMany(const QList<Key> & keys)
    {
        C taken;
        const QList<Key> doomed = detail::uniqueKeys(keys, [this](const Key & k) { return contains(k); });
        if (doomed.isEmpty())
            return taken;
        const Scope scope{*this};
        emit entriesAboutToBeRemoved(detail::boxAll(doomed));
        for (const Key & k : doomed) {
            detail::insertEntry(taken, k, std::move(valueRef(k)));
            eraseKnown(k);
        }
        emit entriesRemoved(detail::boxAll(doomed));
        return taken;
    }

    Value & valueRef(const Key & key)
    {
        if constexpr (SequentialContainer<C>)
            return m_data[static_cast<std::size_t>(key)];
        else
            return detail::mutableValue(m_data, key);
    }

    void eraseKnown(const Key & key)
    {
        if constexpr (SequentialContainer<C>)
            m_data.erase(m_data.begin() + key);
        else if constexpr (requires { m_data.remove(key); })
            m_data.remove(key);
        else
            m_data.erase(key);
    }

    C m_data{};
    int m_scopes = 0;
    bool m_draining = false;
    std::deque<std::unique_ptr<QueuedCall>> m_queue;
};

}  // namespace obs
