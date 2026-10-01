// A slot may disconnect itself and a later slot of the same signal while the
// signal is being emitted: the later slot is not called, not even in the
// emit in progress, and a connection made during the emit waits for the next
// one.
#include "check.hpp"

namespace {

struct Sender : rqt::Object {
  [[= rqt::signal]] void fired() { rqt::emit{this}(); }
};

struct Receiver : rqt::Object {};

}  // namespace

int main() {
  Sender sender;
  Receiver receiver;
  int first_calls = 0;
  int second_calls = 0;
  int late_calls = 0;

  // The handles are assigned after construction because the first slot
  // refers to both of them.
  rqt::connection first;
  rqt::connection second;
  first = sender.connect<&Sender::fired>(receiver, [&] {
    ++first_calls;
    RQT_CHECK(first.disconnect());
    RQT_CHECK(second.disconnect());
    sender.connect<&Sender::fired>(receiver, [&late_calls] { ++late_calls; });
  });
  second = sender.connect<&Sender::fired>(receiver, [&second_calls] { ++second_calls; });

  sender.fired();
  RQT_CHECK(first_calls == 1);
  RQT_CHECK(second_calls == 0);
  RQT_CHECK(late_calls == 0);
  RQT_CHECK(!first.connected() && !second.connected());
  RQT_CHECK(!first.disconnect());

  sender.fired();
  RQT_CHECK(first_calls == 1);
  RQT_CHECK(late_calls == 1);
  RQT_CHECK(sender.receivers<&Sender::fired>() == 1);
  return rqt_test::result();
}
