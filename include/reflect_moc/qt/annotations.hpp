// The annotations a class uses to describe its Qt meta-object, and the
// consteval helpers that read them back.
#pragma once

#ifndef QT_NO_KEYWORDS
#error "reflect-moc needs QT_NO_KEYWORDS: Qt's `emit`, `signals` and `slots` macros break rqt::emit"
#endif

#include <meta>

#include <cstddef>
#include <string_view>
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

// Text inside an annotation. A `char const*` that points at a string literal
// is not a structural value, so reading the annotation back (extract) fails
// with `reflect_constant failed`; the characters are stored inline instead.
// A string literal converts implicitly: `.write = "setValue"`.
inline constexpr std::size_t max_name_length = 64;
inline constexpr std::size_t max_text_length = 256;

template <std::size_t Capacity>
struct text {
  char data[Capacity]{};

  constexpr text() = default;
  template <std::size_t N>
  constexpr text(char const (&literal)[N]) {
    static_assert(N <= Capacity, "rqt: annotation text is longer than the capacity (max_name_length, max_text_length)");
    for (std::size_t i = 0; i < N; ++i) data[i] = literal[i];
  }
  constexpr explicit text(std::string_view s) {
    if (s.size() >= Capacity) throw "rqt: annotation text is longer than the capacity";
    for (std::size_t i = 0; i < s.size(); ++i) data[i] = s[i];
  }
  constexpr std::string_view view() const { return data; }
};

using short_text = text<max_name_length>;

// On a getter (READ is the getter) or on a data member (MEMBER). Every name
// is optional; an empty name means "none". The fields are in this order, and
// a designated initializer must keep it.
struct property {
  short_text read{};    // READ function; empty: the annotated getter
  short_text write{};   // WRITE function
  short_text notify{};  // NOTIFY signal
  short_text reset{};   // RESET function
  short_text name{};    // the property name; empty: the name of the annotated getter or member
  bool final = false;
  bool constant = false;
  bool required = false;
  bool user = false;
  bool designable = true;
  bool scriptable = true;
  bool stored = true;
};

struct classinfo {
  short_text key;
  text<max_text_length> value;
};

namespace detail {

// annotations_of throws for a template (member function templates, constructor
// templates); a template carries no rqt annotation.
template <class Tag>
consteval bool has(meta::info r) {
  if (meta::is_template(r)) return false;
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
  if (meta::is_template(r)) return out;
  for (auto a : meta::annotations_of(r))
    if (meta::remove_cvref(meta::type_of(a)) == ^^Tag) out.push_back(meta::extract<Tag>(a));
  return out;
}

}  // namespace detail
}  // namespace rqt
