#pragma once

#include <reflect_moc/core/event_loop.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <latch>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace rqt {

// How a signal reaches a slot.
enum class connection_type {
  automatic,       // direct if the receiver lives on the emitting thread, else queued
  direct,          // called on the emitting thread, arguments by reference
  queued,          // posted to the receiver's thread, arguments copied
  blocking_queued  // queued, and the emitter waits for the slot; direct on the receiver's own thread
};

namespace detail {

// Pointers to the emitted arguments, borrowed for the duration of the emit.
using arg_span = std::span<void const* const>;

struct connection_record;

// The part of an Object that outlives it for as long as a connection, a
// queued event or an emit in progress still refers to it.
struct object_state {
  explicit object_state(std::thread::id home) : affinity{home} {}

  std::atomic<std::thread::id> affinity;
  std::atomic<bool> alive{true};
  std::mutex mutex;                                          // guards outgoing
  std::vector<std::shared_ptr<connection_record>> outgoing;  // connections from this sender
};

// Who a connection delivers to. An unbound connection (a free callable with
// no context object) is always called directly.
struct receiver_token {
  std::weak_ptr<object_state> state;
  bool bound;
};

// The receiver's thread, or nullopt once it is destroyed.
inline std::optional<std::thread::id> home_of(receiver_token const& receiver) {
  if (!receiver.bound) return std::this_thread::get_id();
  auto const state = receiver.state.lock();
  if (!state || !state->alive.load()) return std::nullopt;
  return state->affinity.load();
}

// A type-erased slot: the callable and two thunks generated for the signal's
// parameter types.
struct slot_call {
  std::shared_ptr<void const> target;
  void (*invoke)(void const* target, arg_span args);
  task (*package)(std::shared_ptr<void const> const& target, arg_span args);
};

struct connection_spec {
  std::uint64_t id;
  void const* signal;
  receiver_token receiver;
  connection_type type;
  slot_call call;
};

struct connection_record {
  explicit connection_record(connection_spec s) : spec{std::move(s)} {}

  connection_spec const spec;
  std::atomic<bool> connected{true};
};

inline std::uint64_t next_connection_id() {
  static std::atomic<std::uint64_t> last{0};
  return last.fetch_add(1, std::memory_order_relaxed) + 1;
}

enum class delivery { direct, queued, blocking };

constexpr delivery delivery_for(connection_type type, bool receiver_is_here) {
  switch (type) {
    case connection_type::direct:
      return delivery::direct;
    case connection_type::queued:
      return delivery::queued;
    case connection_type::blocking_queued:
      return receiver_is_here ? delivery::direct : delivery::blocking;
    case connection_type::automatic:
      break;
  }
  return receiver_is_here ? delivery::direct : delivery::queued;
}

// A queued delivery. The arguments are copied here: this is the hand-off to
// another thread. The receiver's liveness is checked again on its own thread.
inline task queued_call(connection_record const& c, arg_span args) {
  return [receiver = c.spec.receiver, call = c.spec.call.package(c.spec.call.target, args)]() mutable {
    if (home_of(receiver)) call();
  };
}

// Counts the latch down when the task that owns it is destroyed, whether it
// ran or was dropped, so a blocking emitter never waits forever.
class completion {
 public:
  explicit completion(std::shared_ptr<std::latch> done) : done_{std::move(done)} {}
  completion(completion&&) noexcept = default;
  completion& operator=(completion&&) noexcept = default;
  ~completion() {
    if (done_) done_->count_down();
  }

 private:
  std::shared_ptr<std::latch> done_;
};

inline void post_and_wait(std::thread::id home, task call) {
  auto done = std::make_shared<std::latch>(1);
  post(home, [signal_done = completion{done}, call = std::move(call)]() mutable { call(); });
  done->wait();
}

inline void deliver(connection_record const& c, arg_span args) {
  auto const home = home_of(c.spec.receiver);
  if (!home) return;
  switch (delivery_for(c.spec.type, *home == std::this_thread::get_id())) {
    case delivery::direct:
      return c.spec.call.invoke(c.spec.call.target.get(), args);
    case delivery::queued:
      return post(*home, queued_call(c, args));
    case delivery::blocking:
      return post_and_wait(*home, queued_call(c, args));
  }
}

// The connections of one signal, copied under the lock so that slots may
// connect and disconnect while the emit runs.
inline std::vector<std::shared_ptr<connection_record const>> snapshot(object_state& sender, void const* signal) {
  std::scoped_lock lock{sender.mutex};
  return sender.outgoing | std::views::filter([signal](auto const& c) { return c->spec.signal == signal; }) |
         std::ranges::to<std::vector<std::shared_ptr<connection_record const>>>();
}

inline void activate(object_state& sender, void const* signal, arg_span args) {
  for (auto const& c : snapshot(sender, signal))
    if (c->connected.load()) deliver(*c, args);
}

inline bool receiver_gone(std::shared_ptr<connection_record> const& c) { return !home_of(c->spec.receiver); }

inline std::shared_ptr<connection_record> attach(object_state& sender, connection_spec spec) {
  auto record = std::make_shared<connection_record>(std::move(spec));
  std::scoped_lock lock{sender.mutex};
  std::erase_if(sender.outgoing, receiver_gone);
  sender.outgoing.push_back(record);
  return record;
}

inline bool detach(object_state& sender, connection_record& record) {
  std::scoped_lock lock{sender.mutex};
  record.connected.store(false);
  return std::erase_if(sender.outgoing, [&record](auto const& c) { return c.get() == &record; }) > 0;
}

inline void detach_all(object_state& sender) {
  std::scoped_lock lock{sender.mutex};
  for (auto const& c : sender.outgoing) c->connected.store(false);
  sender.outgoing.clear();
}

inline std::size_t live_receivers(object_state& sender, void const* signal) {
  std::scoped_lock lock{sender.mutex};
  return static_cast<std::size_t>(std::ranges::count_if(
      sender.outgoing, [signal](auto const& c) { return c->spec.signal == signal && !receiver_gone(c); }));
}

}  // namespace detail

// A handle to one connection. Copyable; it does not keep the sender alive.
class connection {
 public:
  connection() = default;
  connection(std::weak_ptr<detail::object_state> sender, std::shared_ptr<detail::connection_record> const& record)
      : sender_{std::move(sender)}, record_{record}, id_{record->spec.id} {}

  // Unique for the life of the process; 0 for a default-constructed handle.
  std::uint64_t id() const noexcept { return id_; }

  // False once disconnected, or once the sender or the receiver is destroyed.
  bool connected() const {
    auto const record = record_.lock();
    return record && record->connected.load() && detail::home_of(record->spec.receiver);
  }

  // Safe to call from a slot while the signal is being emitted: the slot is
  // not called again, even later in the same emit. Returns false if the
  // connection was already gone.
  bool disconnect() {
    auto const sender = sender_.lock();
    auto const record = record_.lock();
    return sender && record && detail::detach(*sender, *record);
  }

 private:
  std::weak_ptr<detail::object_state> sender_;
  std::weak_ptr<detail::connection_record> record_;
  std::uint64_t id_ = 0;
};

}  // namespace rqt
