// Role lists as constexpr std::array non-type template arguments (SPEC 6.2).
#pragma once

#include <QtCore/Qt>

#include <array>
#include <cstddef>
#include <type_traits>

namespace sel {
namespace detail {

template <class T>
struct is_std_array : std::false_type {};
template <class E, std::size_t N>
struct is_std_array<std::array<E, N>> : std::true_type {};

// The underlying type for an enum, the type itself otherwise. The enum specialisation is chosen through a
// defaulted bool, so std::underlying_type is never instantiated on a non-enum.
template <class T, bool IsEnum = std::is_enum_v<T>>
struct role_underlying {
    using type = T;
};
template <class T>
struct role_underlying<T, true> {
    using type = std::underlying_type_t<T>;
};

template <class R>
consteval long long roleValue(R role)
{
    using U = typename role_underlying<R>::type;
    return static_cast<long long>(static_cast<U>(role));
}

}  // namespace detail

template <class T>
inline constexpr bool is_std_array_v = detail::is_std_array<std::remove_cvref_t<T>>::value;

// STRICTLY greater than Qt::UserRole: UserRole itself and every built-in role are rejected.
template <class R>
consteval bool IsCustomRole(R role)
{
    return detail::roleValue(role) > static_cast<long long>(Qt::UserRole);
}

template <class E, std::size_t N>
consteval bool AllDistinct(const std::array<E, N> & roles)
{
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = i + 1; j < N; ++j)
            if (detail::roleValue(roles[i]) == detail::roleValue(roles[j]))
                return false;
    return true;
}

namespace detail {

template <auto Roles>
consteval bool allCustom()
{
    if constexpr (is_std_array_v<decltype(Roles)>) {
        for (const auto & r : Roles)
            if (!IsCustomRole(r))
                return false;
        return true;
    } else {
        return false;
    }
}

}  // namespace detail

// The conjunction, in the order of SPEC 6.2: array test first, so a non-array argument is false, not an error.
template <auto Roles>
concept ValidRoleList = is_std_array_v<decltype(Roles)>
                     && (Roles.size() > 0)
                     && detail::allCustom<Roles>()
                     && AllDistinct(Roles);

}  // namespace sel
