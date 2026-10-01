// Queued deliveries to one receiver run in emit order, including when they
// are mixed with plain posted tasks.
#include "check.hpp"

#include <numeric>
#include <vector>

namespace {

constexpr int emits = 1000;
constexpr int marker_every = 100;
constexpr int marker = -1;

struct Sender : rqt::Object {
  [[= rqt::signal]] void value(int v) { rqt::emit{this}(v); }
};

struct Receiver : rqt::Object {
  std::vector<int> seen;
  void onValue(int v) { seen.push_back(v); }
};

std::vector<int> expected_sequence() {
  std::vector<int> out;
  for (int i = 0; i < emits; ++i) {
    out.push_back(i);
    if (i % marker_every == 0) out.push_back(marker);
  }
  return out;
}

}  // namespace

int main() {
  rqt_test::loop_thread worker;
  Sender sender;
  Receiver receiver;
  receiver.move_to_thread(worker.id());
  sender.connect<&Sender::value>(receiver, &Receiver::onValue, rqt::connection_type::queued);

  for (int i = 0; i < emits; ++i) {
    sender.value(i);
    if (i % marker_every == 0) rqt::post(worker.id(), [&receiver] { receiver.seen.push_back(marker); });
  }
  worker.stop();

  RQT_CHECK(receiver.seen == expected_sequence());
  return rqt_test::result();
}
