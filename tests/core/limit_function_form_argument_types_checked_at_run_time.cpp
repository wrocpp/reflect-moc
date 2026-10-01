// LIMIT: in form (a), `rqt::emit{this}(args...)` learns which signal it is
// from std::meta::current_function(), inside a consteval constructor. The
// resulting signal_id is a run-time value, so emit's operator() cannot
// static_assert on the signal's parameter types. Trying
// `static_assert(id.params.size() == sizeof...(A));` there (GCC 16.2):
//
//   error: non-constant condition for static assertion
//   error: '*(const checked_emit<Button>*)this' is not a constant expression
//
// The check therefore runs at emit time: arguments_match compares per-type
// tags, and a mismatch aborts with the signal's name. Forms (b) and
// Object::emit<"name"> resolve the signal at compile time and convert.
#include "check.hpp"

#include <string>

namespace {

struct Button : rqt::Object {
  [[= rqt::signal]] void clicked(int x) { rqt::emit{this}(x); }
  [[= rqt::signal]] void renamed(std::string const& to) { rqt::emit{this}(to); }
};

}  // namespace

int main() {
  constexpr rqt::detail::signal_id clicked{^^Button::clicked};
  constexpr rqt::detail::signal_id renamed{^^Button::renamed};

  RQT_CHECK(rqt::detail::arguments_match<int>(clicked));
  RQT_CHECK(rqt::detail::arguments_match<int const&>(clicked));
  RQT_CHECK(!rqt::detail::arguments_match<double>(clicked));
  RQT_CHECK(!rqt::detail::arguments_match<>(clicked));
  RQT_CHECK(!rqt::detail::arguments_match<int, int>(clicked));

  RQT_CHECK(rqt::detail::arguments_match<std::string>(renamed));
  RQT_CHECK(!rqt::detail::arguments_match<char const*>(renamed));
  return rqt_test::result();
}
