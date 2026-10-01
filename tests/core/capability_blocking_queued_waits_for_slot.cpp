// A blocking_queued connection runs the slot on the receiver's thread and
// returns to the emitter only after the slot is done. On the receiver's own
// thread it is a direct call instead of a deadlock. A blocking emit to a
// receiver that dies first returns instead of waiting forever.
#include "check.hpp"

#include <latch>
#include <memory>
#include <thread>

namespace {

constexpr int payload = 17;

struct Sender : rqt::Object {
  [[= rqt::signal]] void value(int v) { rqt::emit{this}(v); }
};

struct Receiver : rqt::Object {
  int seen = 0;
  std::thread::id ran_on;
  void onValue(int v) {
    seen = v;
    ran_on = std::this_thread::get_id();
  }
};

}  // namespace

int main() {
  rqt_test::loop_thread worker;
  Sender sender;
  Receiver there;
  Receiver here;
  there.move_to_thread(worker.id());
  sender.connect<&Sender::value>(there, &Receiver::onValue, rqt::connection_type::blocking_queued);
  sender.connect<&Sender::value>(here, &Receiver::onValue, rqt::connection_type::blocking_queued);

  sender.value(payload);
  // No stop() before these reads: the blocking emit already synchronised.
  RQT_CHECK(there.seen == payload);
  RQT_CHECK(there.ran_on == worker.id());
  RQT_CHECK(here.seen == payload);
  RQT_CHECK(here.ran_on == std::this_thread::get_id());

  // A blocking delivery that is dropped (its receiver died, or its loop was
  // destroyed) still releases the emitter, because the task's destruction
  // counts the latch down.
  auto done = std::make_shared<std::latch>(1);
  {
    rqt::task dropped = [signal_done = rqt::detail::completion{done}] {};
  }
  RQT_CHECK(done->try_wait());
  worker.stop();
  return rqt_test::result();
}
