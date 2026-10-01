// An automatic connection calls a receiver on the emitting thread directly
// and queues to a receiver on another thread, where the slot runs. Queued
// arguments are copies: changing the original after the emit does not reach
// the slot.
#include "check.hpp"

#include <latch>
#include <string>
#include <thread>

namespace {

struct Sender : rqt::Object {
  [[= rqt::signal]] void message(std::string const& text) { rqt::emit{this}(text); }
};

struct Receiver : rqt::Object {
  std::string text;
  std::thread::id ran_on;
  void onMessage(std::string const& t) {
    text = t;
    ran_on = std::this_thread::get_id();
  }
};

}  // namespace

int main() {
  rqt_test::loop_thread worker;
  Sender sender;
  Receiver here;
  Receiver there;
  there.move_to_thread(worker.id());
  sender.connect<&Sender::message>(here, &Receiver::onMessage);
  sender.connect<&Sender::message>(there, &Receiver::onMessage);

  // Hold the worker so the queued call cannot run before the original changes.
  std::latch release_worker{1};
  rqt::post(worker.id(), [&release_worker] { release_worker.wait(); });

  std::string text = "original";
  sender.message(text);
  RQT_CHECK(here.text == "original");
  RQT_CHECK(here.ran_on == std::this_thread::get_id());

  text = "changed after emit";
  release_worker.count_down();
  worker.stop();

  RQT_CHECK(there.text == "original");
  RQT_CHECK(there.ran_on == worker.id());
  return rqt_test::result();
}
