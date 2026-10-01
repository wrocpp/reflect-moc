// The annotations a class uses to describe its Qt meta-object, and the
// consteval helpers that read them back.
#pragma once

#ifndef QT_NO_KEYWORDS
#error "reflect-moc needs QT_NO_KEYWORDS: Qt's `emit`, `signals` and `slots` macros break rqt::emit"
#endif

#include <meta>

#include <vector>

namespace rqt {
namespace meta = std::meta;

struct signal_t {};
inline constexpr signal_t signal{};
struct slot_t {};
inline constexpr slot_t slot{};
struct invokable_t {};
inline constexpr invokable_t invokable{};
struct enum_t {};
inline constexpr enum_t enum_{};
struct flag_t {};
inline constexpr flag_t flag{};

// On a getter (READ is the getter) or on a data member (MEMBER). Every name
// is optional; an empty name means "none".
struct property {
  char const* read = "";
  char const* write = "";
  char const* notify = "";
  char const* reset = "";
};

struct classinfo {
  char const* key;
  char const* value;
};

namespace detail {

template <class Tag>
consteval bool has(meta::info r) {
  for (auto a : meta::annotations_of(r))
    if (meta::remove_cvref(meta::type_of(a)) == ^^Tag) return true;
  return false;
}

template <class Tag>
consteval Tag get(meta::info r) {
  for (auto a : meta::annotations_of(r))
    if (meta::remove_cvref(meta::type_of(a)) == ^^Tag) return meta::extract<Tag>(a);
  throw meta::exception("missing annotation", r);
}

template <class Tag>
consteval std::vector<Tag> all(meta::info r) {
  std::vector<Tag> out;
  for (auto a : meta::annotations_of(r))
    if (meta::remove_cvref(meta::type_of(a)) == ^^Tag) out.push_back(meta::extract<Tag>(a));
  return out;
}

}  // namespace detail
}  // namespace rqt
