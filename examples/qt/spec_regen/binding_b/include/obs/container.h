// obs::Container<C>: an observable container that announces every change before and after (SPEC 5, 7),
// binding B: the class template declares its own typed signals through reflect-moc, no moc input.
#pragma once

#include "container_signals.h"
#include "traits.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>

#include <map>
#include <optional>
#include <utility>

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
        if (index < 0 || index > size())
            return false;
        entryAboutToBeAdded(index);
        data_.insert(data_.begin() + index, std::move(value));
        entryAdded(index);
        return true;
    }

    bool append(Value value)
        requires SequentialContainer<C>
    {
        return insert(size(), std::move(value));
    }

    // associative: a present key is NOT replaced
    bool insert(const Key & key, Value value)
        requires AssociativeContainer<C>
    {
        if (contains(key))
            return false;
        entryAboutToBeAdded(key);
        detail::assocInsert(data_, key, std::move(value));
        entryAdded(key);
        return true;
    }

    bool remove(const Key & key)
    {
        if (!contains(key))
            return false;
        entryAboutToBeRemoved(key);
        eraseKey(key);
        entryRemoved(key);
        return true;
    }

    std::optional<Value> take(const Key & key)
    {
        if (!contains(key))
            return std::nullopt;
        entryAboutToBeRemoved(key);
        std::optional<Value> taken{std::move(*valueOf(key))};
        eraseKey(key);
        entryRemoved(key);
        return taken;
    }

    bool replace(const Key & key, Value value)
    {
        if (!contains(key))
            return false;
        entryDataAboutToChange(key, QList<int>{});
        *valueOf(key) = std::move(value);
        entryDataChanged(key, QList<int>{});
        return true;
    }

    template <class F>
    bool editValue(const Key & key, F && f, const QList<int> & roles = {})
    {
        if (!contains(key))
            return false;
        entryDataAboutToChange(key, roles);
        std::forward<F>(f)(*valueOf(key));
        entryDataChanged(key, roles);
        return true;
    }

    void clear()
    {
        if (size() == 0)
            return;
        aboutToBeCleared();
        data_.clear();
        cleared();
    }

    // the atomic replacement: one move-assignment between the two notifications, never clear() plus inserts
    void resetWith(C content)
    {
        aboutToBeReset();
        data_ = std::move(content);
        resetCompleted();
    }

    // associative only: on a sequential C the static_assert fires at the call
    template <class Self = C>
    qsizetype insertMany(C entries)
    {
        static_assert(AssociativeContainer<Self>, "insertMany: batches need an associative container, a batch of positions is ambiguous");
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
        const QList<Key> present = detail::presentKeysOnce(data_, keys);
        if (present.isEmpty())
            return 0;
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
        const QList<Key> present = detail::presentKeysOnce(data_, keys);
        C taken;
        if (present.isEmpty())
            return taken;
        entriesAboutToBeRemoved(present);
        for (const Key & key : present) {
            detail::assocInsert(taken, key, std::move(*valueOf(key)));
            eraseKey(key);
        }
        entriesRemoved(present);
        return taken;
    }

private:
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
};

}  // namespace obs
