#pragma once

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace rqt {

// A string literal usable as a template argument: `obj.set<"progress">(v)`.
template <std::size_t N>
struct fixed_string {
  char data[N]{};
  consteval fixed_string(char const (&s)[N]) { std::copy_n(s, N, data); }
  constexpr std::string_view view() const { return {data, N - 1}; }
};

}  // namespace rqt
