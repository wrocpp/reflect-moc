#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>

#if !defined(__cpp_lib_move_only_function)
#error "reflect_moc core needs std::move_only_function (__cpp_lib_move_only_function)"
#endif

namespace rqt {

// A unit of work posted to a thread's event loop.
using task = std::move_only_function<void()>;

namespace detail {

// The queue of tasks posted to one thread. It exists before that thread
// starts its EventLoop, so a post never races the loop's construction.
class mailbox {
 public:
  void push(task t) {
    {
      std::scoped_lock lock{mutex_};
      tasks_.push_back(std::move(t));
    }
    ready_.notify_one();
  }

  // Everything queued so far, without waiting.
  std::deque<task> take() {
    std::scoped_lock lock{mutex_};
    return std::exchange(tasks_, {});
  }

  // Waits for tasks or a quit request; nullopt means quit.
  std::optional<std::deque<task>> wait() {
    std::unique_lock lock{mutex_};
    ready_.wait(lock, [this] { return quitting_ || !tasks_.empty(); });
    if (std::exchange(quitting_, false)) return std::nullopt;
    return std::exchange(tasks_, {});
  }

  void quit() {
    {
      std::scoped_lock lock{mutex_};
      quitting_ = true;
    }
    ready_.notify_one();
  }

 private:
  std::mutex mutex_;
  std::condition_variable ready_;
  std::deque<task> tasks_;
  bool quitting_ = false;
};

struct mailbox_registry {
  std::mutex mutex;
  std::unordered_map<std::thread::id, std::shared_ptr<mailbox>> boxes;
};

inline mailbox_registry& registry() {
  static mailbox_registry instance;
  return instance;
}

// The mailbox of `thread`, created on first use.
inline std::shared_ptr<mailbox> mailbox_for(std::thread::id thread) {
  auto& r = registry();
  std::scoped_lock lock{r.mutex};
  auto& box = r.boxes[thread];
  if (!box) box = std::make_shared<mailbox>();
  return box;
}

inline void retire_mailbox(std::thread::id thread) {
  auto& r = registry();
  std::scoped_lock lock{r.mutex};
  r.boxes.erase(thread);
}

// Runs the batch in order, destroying each task before the next starts, so a
// blocking emitter waiting on a task wakes as soon as its task is done.
inline void run_all(std::deque<task>& batch) {
  while (!batch.empty()) {
    task next = std::move(batch.front());
    batch.pop_front();
    next();
  }
}

}  // namespace detail

// The event loop of the thread that constructs it; one per thread.
// Queued signal deliveries and `rqt::post` calls for that thread run inside
// `run()` or `process_events()`, in the order they were posted.
class EventLoop {
 public:
  // Throws std::logic_error if this thread already has an EventLoop.
  EventLoop() : thread_{std::this_thread::get_id()}, mailbox_{detail::mailbox_for(thread_)} {
    if (current()) throw std::logic_error("rqt::EventLoop: this thread already has an event loop");
    set_current(this);
  }

  // Tasks still queued for this thread are destroyed without running.
  ~EventLoop() {
    set_current(nullptr);
    detail::retire_mailbox(thread_);
  }

  EventLoop(EventLoop const&) = delete;
  EventLoop& operator=(EventLoop const&) = delete;

  // Runs tasks until quit() is called. Tasks posted after quit() stay queued
  // for the next run() or process_events().
  void run() {
    while (auto batch = mailbox_->wait()) detail::run_all(*batch);
  }

  // Runs the tasks queued so far and returns how many there were, including
  // queued deliveries dropped because their receiver died.
  std::size_t process_events() {
    auto batch = mailbox_->take();
    std::size_t const count = batch.size();
    detail::run_all(batch);
    return count;
  }

  // Thread-safe: makes run() return after the task it is running.
  void quit() { mailbox_->quit(); }

  std::thread::id thread() const noexcept { return thread_; }

  // The loop of the calling thread, or nullptr.
  static EventLoop* current() noexcept { return current_slot(); }

 private:
  static EventLoop*& current_slot() noexcept {
    thread_local EventLoop* loop = nullptr;
    return loop;
  }
  static void set_current(EventLoop* loop) noexcept { current_slot() = loop; }

  std::thread::id thread_;
  std::shared_ptr<detail::mailbox> mailbox_;
};

// Queues `t` to run on `thread`'s event loop. A thread that has no loop yet
// keeps the task until its loop starts.
inline void post(std::thread::id thread, task t) { detail::mailbox_for(thread)->push(std::move(t)); }

}  // namespace rqt
