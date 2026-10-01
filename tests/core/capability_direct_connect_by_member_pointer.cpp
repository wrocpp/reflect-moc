// A signal named by member pointer reaches a member function slot, a slot
// taking a prefix of the arguments, a callable with a context object and a
// free callable, all synchronously on the emitting thread.
#include "check.hpp"

#include <thread>

namespace {

constexpr int clicked_value = 7;

struct Button : rqt::Object {
  [[= rqt::signal]] void clicked(int x) { rqt::emit{this}(x); }
};

struct Counter : rqt::Object {
  int last = 0;
  int calls = 0;
  int bare_calls = 0;
  std::thread::id ran_on;

  [[= rqt::slot]] void onClicked(int x) {
    last = x;
    ++calls;
    ran_on = std::this_thread::get_id();
  }
  void onAnything() { ++bare_calls; }
};

}  // namespace

int main() {
  Button button;
  Counter counter;

  auto const member = button.connect<&Button::clicked>(counter, &Counter::onClicked);
  auto const prefix = button.connect<&Button::clicked>(counter, &Counter::onAnything);
  int with_context = 0;
  auto const context = rqt::connect<&Button::clicked>(button, counter, [&with_context](int x) { with_context = x; });
  int free_callable = 0;
  auto const unbound = button.connect<&Button::clicked>([&free_callable](int x) { free_callable = x; });

  RQT_CHECK(member.connected() && prefix.connected() && context.connected() && unbound.connected());
  RQT_CHECK(member.id() != 0 && member.id() != prefix.id() && prefix.id() != context.id());
  RQT_CHECK(button.receivers<&Button::clicked>() == 4);

  button.clicked(clicked_value);

  RQT_CHECK(counter.calls == 1);
  RQT_CHECK(counter.last == clicked_value);
  RQT_CHECK(counter.ran_on == std::this_thread::get_id());
  RQT_CHECK(counter.bare_calls == 1);
  RQT_CHECK(with_context == clicked_value);
  RQT_CHECK(free_callable == clicked_value);
  return rqt_test::result();
}
