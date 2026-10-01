// Form (a): an annotated member function whose body is
// `rqt::emit{this}(args...)` emits itself, with any number of arguments,
// found by name or pointer, in a table ordered by declaration and bases first.
#include "check.hpp"

#include <string>
#include <string_view>

namespace {

constexpr int clicked_value = 3;

struct Widget : rqt::Object {
  [[= rqt::signal]] void destroyedSoon() const { rqt::emit{this}(); }
};

struct Button : Widget {
  [[= rqt::signal]] void clicked(int x) { rqt::emit{this}(x); }
  [[= rqt::signal]] void released() { rqt::emit{this}(); }
  [[= rqt::signal]] void renamed(std::string const& to, bool permanent) { rqt::emit{this}(to, permanent); }
  void press() { clicked(clicked_value); }
};

constexpr auto table = rqt::signal_table<Button>();
static_assert(table.size() == 4);
static_assert(std::string_view{table[0].name} == "destroyedSoon" && table[0].arity == 0);
static_assert(std::string_view{table[1].name} == "clicked" && table[1].arity == 1);
static_assert(std::string_view{table[2].name} == "released" && table[2].arity == 0);
static_assert(std::string_view{table[3].name} == "renamed" && table[3].arity == 2);

}  // namespace

int main() {
  Button button;
  int clicked_with = 0;
  int released = 0;
  int doomed = 0;
  std::string renamed_to;
  bool permanent = false;

  button.connect<&Button::clicked>(button, [&clicked_with](int x) { clicked_with = x; });
  button.connect<"released">(button, [&released] { ++released; });
  button.connect<"destroyedSoon">(button, [&doomed] { ++doomed; });
  button.connect<&Button::renamed>(button, [&](std::string const& to, bool p) {
    renamed_to = to;
    permanent = p;
  });

  button.press();
  button.released();
  button.renamed("ok", true);
  static_cast<Widget const&>(button).destroyedSoon();

  RQT_CHECK(clicked_with == clicked_value);
  RQT_CHECK(released == 1);
  RQT_CHECK(renamed_to == "ok" && permanent);
  RQT_CHECK(doomed == 1);
  return rqt_test::result();
}
