// Container classification, key traits and small utilities (SPEC 6.1, 6.4), binding B.
#pragma once

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QtGlobal>
#include <QtCore/qhashfunctions.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <type_traits>
#include <utility>
#include <vector>

namespace obs {
namespace detail {

// The whitelist. The primary template accepts nothing; each specialisation names the key and value type.
template <class C>
struct container_traits {
    static constexpr bool sequential = false;
    static constexpr bool associative = false;
};

template <class T>
struct container_traits<QList<T>> {
    static constexpr bool sequential = true;
    static constexpr bool associative = false;
    using key_type = qsizetype;
    using value_type = T;
};

template <class T, class A>
struct container_traits<std::vector<T, A>> {
    static constexpr bool sequential = true;
    static constexpr bool associative = false;
    using key_type = qsizetype;
    using value_type = T;
};

template <class K, class V>
struct container_traits<QHash<K, V>> {
    static constexpr bool sequential = false;
    static constexpr bool associative = true;
    using key_type = K;
    using value_type = V;
};

template <class K, class V>
struct container_traits<QMap<K, V>> {
    static constexpr bool sequential = false;
    static constexpr bool associative = true;
    using key_type = K;
    using value_type = V;
};

template <class K, class V, class Cmp, class A>
struct container_traits<std::map<K, V, Cmp, A>> {
    static constexpr bool sequential = false;
    static constexpr bool associative = true;
    using key_type = K;
    using value_type = V;
};

}  // namespace detail

template <class C>
concept SequentialContainer = detail::container_traits<std::remove_cvref_t<C>>::sequential;

template <class C>
concept AssociativeContainer = detail::container_traits<std::remove_cvref_t<C>>::associative;

template <class C>
concept SupportedContainer = SequentialContainer<C> || AssociativeContainer<C>;

template <SupportedContainer C>
using KeyOf = typename detail::container_traits<std::remove_cvref_t<C>>::key_type;

template <SupportedContainer C>
using ValueOf = typename detail::container_traits<std::remove_cvref_t<C>>::value_type;

// Binding B has no VariantKey: keys travel typed (SPEC 4.2).
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
        const int v = static_cast<int>(*first);
        if (!runs.empty() && runs.back().second + 1 == v)
            runs.back().second = v;
        else
            runs.emplace_back(v, v);
    }
    return runs;
}

// 0 <= i < size, without letting a negative signed i turn into a huge unsigned value.
template <class Container, std::integral I>
bool validIndex(const Container & c, I i)
{
    if constexpr (std::is_signed_v<I>) {
        if (i < 0)
            return false;
    }
    return static_cast<std::uintmax_t>(i) < static_cast<std::uintmax_t>(c.size());
}

}  // namespace obs
