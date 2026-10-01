// A thread has at most one EventLoop, reachable as EventLoop::current().
// process_events() runs what is queued, in order; tasks posted before a
// thread's loop exists wait for it; quit() ends run().
#include "check.hpp"

#include <latch>
#include <stdexcept>
#include <thread>
#include <vector>

int main() {
  RQT_CHECK(rqt::EventLoop::current() == nullptr);
  {
    rqt::EventLoop loop;
    RQT_CHECK(rqt::EventLoop::current() == &loop);
    RQT_CHECK(loop.thread() == std::this_thread::get_id());

    bool second_rejected = false;
    try {
      rqt::EventLoop second;
    } catch (std::logic_error const&) {
      second_rejected = true;
    }
    RQT_CHECK(second_rejected);
    RQT_CHECK(rqt::EventLoop::current() == &loop);

    std::vector<int> order;
    rqt::post(loop.thread(), [&order] { order.push_back(1); });
    rqt::post(loop.thread(), [&order] { order.push_back(2); });
    RQT_CHECK(loop.process_events() == 2);
    RQT_CHECK(order == std::vector{1, 2});
    RQT_CHECK(loop.process_events() == 0);

    rqt::post(loop.thread(), [&loop] { loop.quit(); });
    loop.run();
  }
  RQT_CHECK(rqt::EventLoop::current() == nullptr);

  // Posted before the thread's loop exists: runs once the loop starts.
  bool ran = false;
  std::latch go{1};
  std::jthread late{[&go] {
    go.wait();
    rqt::EventLoop loop;
    loop.run();
  }};
  rqt::post(late.get_id(), [&ran] { ran = true; });
  rqt::post(late.get_id(), [] { rqt::EventLoop::current()->quit(); });
  go.count_down();
  late.join();
  RQT_CHECK(ran);
  return rqt_test::result();
}
