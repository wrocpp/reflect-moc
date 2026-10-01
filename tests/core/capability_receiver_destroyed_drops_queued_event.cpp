// Destroying a receiver disconnects it, and a delivery already queued for it
// is dropped on its thread instead of calling into a dead object.
#include "check.hpp"

#include <memory>
#include <thread>

namespace {

constexpr int payload = 5;

struct Sender : rqt::Object {
  [[= rqt::signal]] void value(int v) { rqt::emit{this}(v); }
};

struct Receiver : rqt::Object {
  int* calls;
  explicit Receiver(int* c) : calls{c} {}
  void onValue(int) { ++*calls; }
};

}  // namespace

int main() {
  rqt::EventLoop loop;
  Sender sender;
  int calls = 0;
  auto receiver = std::make_unique<Receiver>(&calls);
  auto const link = sender.connect<&Sender::value>(*receiver, &Receiver::onValue);
  RQT_CHECK(sender.receivers<&Sender::value>() == 1);

  // Emitted on another thread, so the automatic connection queues to this one.
  std::jthread{[&sender] { sender.value(payload); }}.join();

  receiver.reset();
  RQT_CHECK(sender.receivers<&Sender::value>() == 0);
  RQT_CHECK(!link.connected());

  RQT_CHECK(loop.process_events() == 1);
  RQT_CHECK(calls == 0);

  sender.value(payload);
  RQT_CHECK(loop.process_events() == 0);
  RQT_CHECK(calls == 0);
  return rqt_test::result();
}
