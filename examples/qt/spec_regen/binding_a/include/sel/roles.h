#pragma once

#include <QtCore/qnamespace.h>

#include <array>
#include <cstddef>
#include <type_traits>

namespace sel {

namespace detail {

template <class T>
inline constexpr bool isStdArray = false;

template <class E, std::size_t N>
inline constexpr bool isStdArray<std::array<E, N>> = true;

}  // namespace detail

template <class T>
inline constexpr bool is_std_array_v = detail::isStdArray<std::remove_cvref_t<T>>;

namespace detail {

// An enum is read through its underlying type, anything else as itself; the
// bool parameter keeps std::underlying_type away from non-enums.
template <class T, bool IsEnum = std::is_enum_v<T>>
struct RoleValueType {
    using type = T;
};

template <class T>
struct RoleValueType<T, true> {
    using type = std::underlying_type_t<T>;
};

template <class T>
constexpr long long roleValue(T role)
{
    return static_cast<long long>(static_cast<typename RoleValueType<T>::type>(role));
}

}  // namespace detail

template <class R>
consteval bool IsCustomRole(R role)
{
    return detail::roleValue(role) > static_cast<long long>(Qt::UserRole);
}

template <class E, std::size_t N>
consteval bool AllDistinct(const std::array<E, N> & roles)
{
    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = i + 1; j < N; ++j) {
            if (roles[i] == roles[j])
                return false;
        }
    }
    return true;
}

namespace detail {

template <class E, std::size_t N>
consteval bool AllCustom(const std::array<E, N> & roles)
{
    for (std::size_t i = 0; i < N; ++i) {
        if (!IsCustomRole(roles[i]))
            return false;
    }
    return true;
}

}  // namespace detail

// The conjunction stops at the first false operand, so a non-array argument
// gives false instead of a hard error.
template <auto Roles>
concept ValidRoleList = is_std_array_v<decltype(Roles)>
                     && (Roles.size() > 0)
                     && detail::AllCustom(Roles)
                     && AllDistinct(Roles);

}  // namespace sel
