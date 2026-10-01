// Spike 04: compile-time string -> member.
//
// Question: can a fixed_string NTTP select a data member or member function
// by identifier_of, fail with a readable diagnostic when there is none, and
// be used from a CRTP base member function template?
//
// Build:  g++-16 -std=c++26 -freflection -Wall -Wextra spikes/04-member-named.cpp
// Probe:  -DSPIKE_NO_MEMBER (the diagnostic for a misspelled name)
#include <meta>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace meta = std::meta;

#if defined(__clang__)
// clang-p2996 has no std::meta::exception. Calling a non-constexpr function
// also stops constant evaluation, with a less readable diagnostic.
inline void reflection_error(std::string_view) {}
#define SPIKE_THROW(msg, r) (reflection_error(msg), throw 0)
#else
#define SPIKE_THROW(msg, r) throw meta::exception(msg, r)
#endif

template <std::size_t N>
struct fixed_string {
  char data[N]{};
  consteval fixed_string(char const (&s)[N]) { std::copy_n(s, N, data); }
  constexpr std::string_view view() const { return {data, N - 1}; }
};

template <class T, fixed_string Name>
consteval meta::info member_named() {
  for (auto m : meta::members_of(^^T, meta::access_context::unchecked()))
    if (meta::has_identifier(m) && meta::identifier_of(m) == Name.view() &&
        (meta::is_nonstatic_data_member(m) || meta::is_function(m)))
      return m;
  std::string msg = "no member named '";
  msg += Name.view();
  msg += "' in ";
  msg += meta::identifier_of(^^T);
  SPIKE_THROW(msg, ^^T);
}

template <class D>
struct properties {
  // A data member is assigned; a member function is called as a setter.
  template <fixed_string Name, class V>
  void set(V&& v) {
    constexpr auto m = member_named<D, Name>();
    auto& self = static_cast<D&>(*this);
    if constexpr (meta::is_function(m))
      self.[:m:](std::forward<V>(v));
    else
      self.[:m:] = std::forward<V>(v);
  }

  template <fixed_string Name>
  decltype(auto) get() const {
    constexpr auto m = member_named<D, Name>();
    auto const& self = static_cast<D const&>(*this);
    if constexpr (meta::is_function(m))
      return self.[:m:]();
    else
      return (self.[:m:]);
  }
};

struct Widget : properties<Widget> {
  int width = 0;
  int height_ = 0;
  void setHeight(int h) { height_ = h * 2; }
  int height() const { return height_; }
};

template <class W>
void generic_caller(W& w) {
  w.template set<"width">(9);
}

int main() {
  static_assert(member_named<Widget, "width">() == ^^Widget::width);
  static_assert(member_named<Widget, "setHeight">() == ^^Widget::setHeight);

  Widget w;
  w.set<"width">(5);
  assert(w.width == 5);
  w.set<"setHeight">(4);
  assert(w.height_ == 8);
  assert(w.get<"height">() == 8);
  assert(w.get<"width">() == 5);

  generic_caller(w);
  assert(w.width == 9);

#ifdef SPIKE_NO_MEMBER
  w.set<"widht">(1);
#endif

  std::puts("04 member_named: data member, setter fn, CRTP set/get, dependent .template call OK");
}
