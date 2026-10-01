#pragma once

#include <reflect_moc/core.hpp>

#include <cstdio>
#include <cstdlib>
#include <thread>

namespace rqt_test {

inline int failures = 0;

inline void check(bool ok, char const* expression, char const* file, int line) {
  if (ok) return;
  ++failures;
  std::fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expression);
}

inline int result() { return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE; }

// A thread running an rqt::EventLoop until stop() or destruction. Tasks and
// queued deliveries posted before stop() all run before it returns, since
// the quit request is queued behind them.
class loop_thread {
 public:
  loop_thread()
      : thread_{[] {
          rqt::EventLoop loop;
          loop.run();
        }},
        id_{thread_.get_id()} {}
  loop_thread(loop_thread const&) = delete;
  loop_thread& operator=(loop_thread const&) = delete;
  ~loop_thread() { stop(); }

  // Still the loop thread's id after stop(); joining resets thread_.get_id().
  std::thread::id id() const noexcept { return id_; }

  void stop() {
    if (!thread_.joinable()) return;
    rqt::post(id(), [] { rqt::EventLoop::current()->quit(); });
    thread_.join();
  }

 private:
  std::jthread thread_;
  std::thread::id id_;
};

}  // namespace rqt_test

#define RQT_CHECK(...) ::rqt_test::check(static_cast<bool>(__VA_ARGS__), #__VA_ARGS__, __FILE__, __LINE__)
