// Form (b): `static constexpr rqt::signal_of<A...> name{};` descriptors are
// found by reflection, connect by pointer or name, and emit either through
// the descriptor or through Object::emit with converted arguments.
#include "check.hpp"

#include <string_view>

namespace {

constexpr int slider_value = 12;
constexpr int moved_x = 1;
constexpr double moved_y = 2.5;
constexpr long wide_value = 99;

struct Slider : rqt::Object {
  static constexpr rqt::signal_of<int> valueChanged{};
  static constexpr int not_a_signal = 3;
  static constexpr rqt::signal_of<> released{};
  static constexpr rqt::signal_of<int, double> moved{};

  int value = 0;
  void setValue(int v) {
    value = v;
    valueChanged.emit(this, v);
  }
  void release() { emit<&Slider::released>(); }
};

constexpr auto table = rqt::signal_table<Slider>();
static_assert(table.size() == 3);
static_assert(std::string_view{table[0].name} == "valueChanged" && table[0].arity == 1);
static_assert(std::string_view{table[1].name} == "released" && table[1].arity == 0);
static_assert(std::string_view{table[2].name} == "moved" && table[2].arity == 2);

}  // namespace

int main() {
  Slider slider;
  int value_seen = 0;
  int released = 0;
  double y_seen = 0;

  slider.connect<&Slider::valueChanged>(slider, [&value_seen](int v) { value_seen = v; });
  slider.connect<"released">(slider, [&released] { ++released; });
  slider.connect<"moved">(slider, [&y_seen](int, double y) { y_seen = y; });

  slider.setValue(slider_value);
  RQT_CHECK(value_seen == slider_value);

  slider.release();
  RQT_CHECK(released == 1);

  slider.emit<"moved">(moved_x, moved_y);
  RQT_CHECK(y_seen == moved_y);

  slider.emit<&Slider::valueChanged>(wide_value);  // long converts to the signal's int
  RQT_CHECK(value_seen == wide_value);
  return rqt_test::result();
}
