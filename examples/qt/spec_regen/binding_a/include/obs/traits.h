#pragma once

#include <QHash>
#include <QList>
#include <QMap>
#include <QVariant>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

namespace obs {

namespace detail {

// The whitelist of section 6.1: one partial specialisation per accepted
// template, never a test for key_type / mapped_type.
template <class C>
struct ContainerInfo {
    static constexpr bool sequential = false;
    static constexpr bool associative = false;
};

template <class T>
struct ContainerInfo<QList<T>> {
    static constexpr bool sequential = true;
    static constexpr bool associative = false;
    using Key = qsizetype;
    using Value = T;
};

template <class T, class A>
struct ContainerInfo<std::vector<T, A>> {
    static constexpr bool sequential = true;
    static constexpr bool associative = false;
    using Key = qsizetype;
    using Value = T;
};

template <class K, class V>
struct ContainerInfo<QHash<K, V>> {
    static constexpr bool sequential = false;
    static constexpr bool associative = true;
    using Key = K;
    using Value = V;
};

template <class K, class V>
struct ContainerInfo<QMap<K, V>> {
    static constexpr bool sequential = false;
    static constexpr bool associative = true;
    using Key = K;
    using Value = V;
};

template <class K, class V, class Cmp, class A>
struct ContainerInfo<std::map<K, V, Cmp, A>> {
    static constexpr bool sequential = false;
    static constexpr bool associative = true;
    using Key = K;
    using Value = V;
};

template <class C>
using Info = ContainerInfo<std::remove_cvref_t<C>>;

}  // namespace detail

template <class C>
concept SequentialContainer = detail::Info<C>::sequential;

template <class C>
concept AssociativeContainer = detail::Info<C>::associative;

template <class C>
concept SupportedContainer = SequentialContainer<C> || AssociativeContainer<C>;

template <SupportedContainer C>
using KeyOf = typename detail::Info<C>::Key;

template <SupportedContainer C>
using ValueOf = typename detail::Info<C>::Value;

template <class K>
concept VariantKey = requires(const K & k) {
    QVariant::fromValue(k);
    { k == k } -> std::convertible_to<bool>;
};

template <class K>
concept HashableKey = requires(const K & k) {
    { qHash(k) } -> std::convertible_to<std::size_t>;
};

// Inclusive runs of an ascending, duplicate-free integer range.
template <class It>
std::vector<std::pair<int, int>> contiguousRuns(It first, It last)
{
    std::vector<std::pair<int, int>> runs;
    for (; first != last; ++first) {
        const int value = static_cast<int>(*first);
        if (!runs.empty() && runs.back().second + 1 == value)
            runs.back().second = value;
        else
            runs.emplace_back(value, value);
    }
    return runs;
}

// True iff 0 <= i < c.size(); a negative signed i is never valid and is never
// converted to a large unsigned value.
template <class Container, std::integral I>
bool validIndex(const Container & c, I i)
{
    if constexpr (std::is_signed_v<I>) {
        if (i < 0)
            return false;
    }
    return static_cast<std::uintmax_t>(i) < static_cast<std::uintmax_t>(c.size());
}

namespace detail {

template <class K>
QVariant box(const K & key)
{
    return QVariant::fromValue(key);
}

template <class K>
QList<QVariant> boxAll(const QList<K> & keys)
{
    QList<QVariant> out;
    out.reserve(keys.size());
    for (const K & k : keys)
        out.append(box(k));
    return out;
}

template <class K>
K unbox(const QVariant & v)
{
    if constexpr (std::is_same_v<K, qsizetype>)
        return v.toLongLong();
    else
        return v.template value<K>();
}

template <class K>
QList<K> unboxAll(const QList<QVariant> & keys)
{
    QList<K> out;
    out.reserve(keys.size());
    for (const QVariant & v : keys)
        out.append(unbox<K>(v));
    return out;
}

// The caller's keys with duplicates and the keys `keep` rejects removed, in
// the caller's order.
template <class K, class Keep>
QList<K> uniqueKeys(const QList<K> & keys, Keep keep)
{
    QList<K> out;
    if constexpr (HashableKey<K>) {
        QHash<K, char> seen;
        for (const K & k : keys) {
            if (!keep(k) || seen.contains(k))
                continue;
            seen.insert(k, 1);
            out.append(k);
        }
    } else {
        for (const K & k : keys) {
            if (keep(k) && !out.contains(k))
                out.append(k);
        }
    }
    return out;
}

}  // namespace detail

}  // namespace obs
