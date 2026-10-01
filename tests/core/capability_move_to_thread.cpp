// An Object's affinity starts at its constructing thread and follows
// move_to_thread; automatic connections resolve against the current affinity
// at each emit.
#include "check.hpp"

#include <thread>
#include <vector>

namespace {

struct Sender : rqt::Object {
  [[= rqt::signal]] void ping() { rqt::emit{this}(); }
};

struct Receiver : rqt::Object {
  std::vector<std::thread::id> ran_on;
  void onPing() { ran_on.push_back(std::this_thread::get_id()); }
};

}  // namespace

int main() {
  auto const main_thread = std::this_thread::get_id();
  rqt_test::loop_thread worker;
  Sender sender;
  Receiver receiver;
  RQT_CHECK(receiver.thread() == main_thread);
  sender.connect<&Sender::ping>(receiver, &Receiver::onPing);

  receiver.move_to_thread(worker.id());
  RQT_CHECK(receiver.thread() == worker.id());
  sender.ping();

  // Round-trip through the worker so the queued ping has run before moving back.
  rqt::post(worker.id(), [&receiver, main_thread] { receiver.move_to_thread(main_thread); });
  worker.stop();
  RQT_CHECK(receiver.thread() == main_thread);
  sender.ping();

  RQT_CHECK(receiver.ran_on == std::vector{worker.id(), main_thread});
  return rqt_test::result();
}
