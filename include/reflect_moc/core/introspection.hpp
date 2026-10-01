#pragma once

#include <reflect_moc/core/annotations.hpp>

#include <meta>

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace rqt {

// One row of a class's compile-time signal table.
struct signal_info {
  char const* name;
  std::size_t arity;
};

// Names a signal in `connect<...>`, `emit<...>` and `receivers<...>`: either
// by name, `"progressChanged"`, or by pointer, `&Button::clicked`,
// `&Slider::valueChanged` (a descriptor) or `&Worker::progress` (a NOTIFY
// property). Resolution happens at compile time; a name or pointer that is
// not a signal of the sender's class is a compile error.
struct signal_ref {
  char const* name;
  std::meta::info pointer;

  template <std::size_t N>
  consteval signal_ref(char const (&s)[N]) : name{std::define_static_string(std::string_view{s, N - 1})}, pointer{} {}

  template <class P>
    requires std::is_pointer_v<P> || std::is_member_pointer_v<P>
  consteval signal_ref(P p) : name{nullptr}, pointer{std::meta::reflect_constant(p)} {}
};

namespace detail {

namespace meta = std::meta;

inline constexpr std::string_view notify_suffix = "Changed";

template <class Tag>
consteval bool has_annotation(meta::info r) {
  return std::ranges::any_of(meta::annotations_of(r),
                             [](meta::info a) { return meta::remove_cvref(meta::type_of(a)) == ^^Tag; });
}

// Call only after has_annotation<Tag>(r).
template <class Tag>
consteval Tag annotation_of(meta::info r) {
  auto const all = meta::annotations_of(r);
  return meta::extract<Tag>(
      *std::ranges::find_if(all, [](meta::info a) { return meta::remove_cvref(meta::type_of(a)) == ^^Tag; }));
}

consteval bool is_function_signal(meta::info m) { return meta::is_function(m) && has_annotation<signal_t>(m); }

consteval bool is_descriptor_signal(meta::info m) {
  if (!meta::is_variable(m)) return false;
  auto const t = meta::remove_cvref(meta::type_of(m));
  return meta::has_template_arguments(t) && meta::template_of(t) == ^^signal_of;
}

consteval bool is_property(meta::info m) { return meta::is_nonstatic_data_member(m) && has_annotation<property>(m); }

consteval bool is_notify_property(meta::info m) { return is_property(m) && annotation_of<property>(m).notify; }

consteval bool is_signal(meta::info m) {
  return is_function_signal(m) || is_descriptor_signal(m) || is_notify_property(m);
}

consteval std::vector<meta::info> own_members_where(meta::info cls, bool (*pred)(meta::info)) {
  return meta::members_of(cls, meta::access_context::unchecked()) | std::views::filter(pred) |
         std::ranges::to<std::vector>();
}

// Members of `cls` and its bases that satisfy `pred`, bases first, each in
// declaration order.
consteval std::vector<meta::info> members_where(meta::info cls, bool (*pred)(meta::info)) {
  std::vector<meta::info> found;
  for (auto base : meta::bases_of(cls, meta::access_context::unchecked()))
    found.append_range(members_where(meta::type_of(base), pred));
  found.append_range(own_members_where(cls, pred));
  return found;
}

consteval std::vector<meta::info> signals_of(meta::info cls) { return members_where(cls, is_signal); }

consteval std::string signal_name(meta::info s) {
  std::string name{meta::identifier_of(s)};
  if (is_notify_property(s)) name += notify_suffix;
  return name;
}

consteval std::string class_name(meta::info cls) {
  return meta::has_identifier(cls) ? std::string{meta::identifier_of(cls)} : std::string{"this class"};
}

consteval std::string missing(std::string_view kind, std::string_view name, meta::info cls) {
  return std::string{"no "} + std::string{kind} + " named '" + std::string{name} + "' in " + class_name(cls);
}

consteval meta::info signal_named(meta::info cls, std::string_view name) {
  auto const all = signals_of(cls);
  auto const it = std::ranges::find_if(all, [name](meta::info s) { return signal_name(s) == name; });
  if (it == all.end()) throw meta::exception(missing("signal", name, cls), cls);
  return *it;
}

consteval meta::info property_named(meta::info cls, std::string_view name) {
  auto const all = members_where(cls, is_property);
  auto const it = std::ranges::find_if(all, [name](meta::info p) { return meta::identifier_of(p) == name; });
  if (it == all.end()) throw meta::exception(missing("property", name, cls), cls);
  return *it;
}

// True if the pointer P is &S (a member function, data member or descriptor).
template <meta::info S, auto P>
consteval bool designates() {
  if constexpr (std::is_same_v<decltype(&[:S:]), decltype(P)>)
    return &[:S:] == P;
  else
    return false;
}

template <class T, auto P>
consteval meta::info signal_at() {
  constexpr std::size_t count = signals_of(^^T).size();
  constexpr std::size_t hit = []<std::size_t... I>(std::index_sequence<I...>) {
    std::size_t found = count;
    (void)((designates<signals_of(^^T)[I], P>() ? (found = I, true) : false) || ...);
    return found;
  }(std::make_index_sequence<count>{});
  if (hit == count) throw meta::exception("the pointer does not name a signal of " + class_name(^^T), ^^T);
  return signals_of(^^T)[hit];
}

template <class T, signal_ref D>
consteval meta::info resolve_signal() {
  if constexpr (D.name != nullptr)
    return signal_named(^^T, D.name);
  else
    return signal_at<T, ([:D.pointer:])>();
}

consteval meta::info value_type_of(meta::info parameter) { return meta::remove_cvref(meta::type_of(parameter)); }

// The value types a signal carries, in order.
consteval std::vector<meta::info> signal_params(meta::info s) {
  if (is_descriptor_signal(s)) return meta::template_arguments_of(meta::remove_cvref(meta::type_of(s)));
  if (is_notify_property(s)) return {value_type_of(s)};
  return meta::parameters_of(s) | std::views::transform(value_type_of) | std::ranges::to<std::vector>();
}

// --- run-time identity of a signal --------------------------------------------
//
// A descriptor is its own key (its address). A function signal or a NOTIFY
// property is keyed by one byte of a per-class anchor array, at the signal's
// index among the class's own signals. The key cannot be a variable template
// specialised on the signal's reflection: see
// limit_current_function_reflection_as_template_argument.

consteval std::size_t own_signal_count(meta::info cls) { return own_members_where(cls, is_signal).size(); }

template <class C>
inline constexpr std::array<char, own_signal_count(^^C)> signal_anchor{};

template <class C>
inline constexpr char const* signal_anchor_begin = signal_anchor<C>.data();

consteval void const* anchored_key(meta::info s) {
  auto const cls = meta::parent_of(s);
  auto const own = own_members_where(cls, is_signal);
  auto const index = std::ranges::find(own, s) - own.begin();
  return meta::extract<char const*>(meta::substitute(^^signal_anchor_begin, {cls})) + index;
}

template <meta::info S>
consteval void const* signal_key() {
  if constexpr (is_descriptor_signal(S))
    return &[:S:];
  else
    return anchored_key(S);
}

// Per-type tags that let a form (a) emit check its arguments at run time.
template <class T>
inline constexpr char type_tag = 0;

template <class T>
inline constexpr void const* type_key = &type_tag<T>;

consteval void const* type_key_of(meta::info type) {
  return meta::extract<void const*>(meta::substitute(^^type_key, {type}));
}

consteval std::span<void const* const> param_type_keys(meta::info s) {
  return std::define_static_array(signal_params(s) | std::views::transform(type_key_of));
}

consteval signal_info describe(meta::info s) {
  return {.name = std::define_static_string(signal_name(s)), .arity = signal_params(s).size()};
}

}  // namespace detail

// The signals of T and its bases, bases first, in declaration order.
template <class T>
consteval std::span<signal_info const> signal_table() {
  return std::define_static_array(detail::signals_of(^^T) | std::views::transform(detail::describe));
}

}  // namespace rqt
