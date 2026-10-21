#pragma once

#include "obs/container_signals.h"
#include "obs/traits.h"

#include <QHash>
#include <QList>
#include <QMap>
#include <QVariant>

#include <functional>
#include <map>
#include <optional>
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

    // sequential only
    bool insert(qsizetype index, Value value)
        requires SequentialContainer<C>
    {
        if (index < 0 || index > size())
            return false;
        emit entryAboutToBeAdded(detail::box(index));
        m_data.insert(m_data.begin() + index, std::move(value));
        emit entryAdded(detail::box(index));
        return true;
    }

    bool append(Value value)
        requires SequentialContainer<C>
    {
        return insert(size(), std::move(value));
    }

    // associative only
    bool insert(const Key & key, Value value)
        requires AssociativeContainer<C>
    {
        if (contains(key))
            return false;
        emit entryAboutToBeAdded(detail::box(key));
        detail::insertEntry(m_data, key, std::move(value));
        emit entryAdded(detail::box(key));
        return true;
    }

    bool remove(const Key & key)
    {
        if (!contains(key))
            return false;
        emit entryAboutToBeRemoved(detail::box(key));
        eraseKnown(key);
        emit entryRemoved(detail::box(key));
        return true;
    }

    std::optional<Value> take(const Key & key)
    {
        if (!contains(key))
            return std::nullopt;
        emit entryAboutToBeRemoved(detail::box(key));
        std::optional<Value> taken{std::move(valueRef(key))};
        eraseKnown(key);
        emit entryRemoved(detail::box(key));
        return taken;
    }

    bool replace(const Key & key, Value value)
    {
        if (!contains(key))
            return false;
        emit entryDataAboutToChange(detail::box(key), QList<int>{});
        valueRef(key) = std::move(value);
        emit entryDataChanged(detail::box(key), QList<int>{});
        return true;
    }

    template <class F>
    bool editValue(const Key & key, F && f, const QList<int> & roles = {})
    {
        if (!contains(key))
            return false;
        emit entryDataAboutToChange(detail::box(key), roles);
        std::invoke(std::forward<F>(f), valueRef(key));
        emit entryDataChanged(detail::box(key), roles);
        return true;
    }

    void clear()
    {
        if (size() == 0)
            return;
        emit aboutToBeCleared();
        m_data.clear();
        emit cleared();
    }

    void resetWith(C content)
    {
        emit aboutToBeReset();
        m_data = std::move(content);
        emit resetCompleted();
    }

    // associative only; on a sequential C the call does not compile (7.2 B2)
    template <class Self = C>
    qsizetype insertMany(C entries)
    {
        static_assert(AssociativeContainer<Self>,
                      "insertMany: batch operations exist for associative containers only");
        if constexpr (AssociativeContainer<Self>) {
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
            emit entriesAboutToBeAdded(detail::boxAll(keys));
            for (auto & [k, v] : fresh)
                detail::insertEntry(m_data, k, std::move(v));
            emit entriesAdded(detail::boxAll(keys));
            return static_cast<qsizetype>(fresh.size());
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
            const QList<Key> doomed = detail::uniqueKeys(keys, [this](const Key & k) { return contains(k); });
            if (doomed.isEmpty())
                return 0;
            emit entriesAboutToBeRemoved(detail::boxAll(doomed));
            for (const Key & k : doomed)
                eraseKnown(k);
            emit entriesRemoved(detail::boxAll(doomed));
            return doomed.size();
        } else {
            return 0;
        }
    }

    template <class Self = C>
    C takeMany(const QList<Key> & keys)
    {
        static_assert(AssociativeContainer<Self>,
                      "takeMany: batch operations exist for associative containers only");
        C taken;
        if constexpr (AssociativeContainer<Self>) {
            const QList<Key> doomed = detail::uniqueKeys(keys, [this](const Key & k) { return contains(k); });
            if (doomed.isEmpty())
                return taken;
            emit entriesAboutToBeRemoved(detail::boxAll(doomed));
            for (const Key & k : doomed) {
                detail::insertEntry(taken, k, std::move(valueRef(k)));
                eraseKnown(k);
            }
            emit entriesRemoved(detail::boxAll(doomed));
        }
        return taken;
    }

private:
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
};

}  // namespace obs
