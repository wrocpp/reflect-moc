# reflect_moc core

Signals, slots, properties and thread affinity without Qt and without moc.
C++26 reflection finds the signals. Needs GCC 16.2 with `-std=c++26 -freflection -pthread`.

```cpp
#include <reflect_moc/core.hpp>
```

## Objects

`rqt::Object` is a plain (non-template, non-virtual) base. Its member templates
take an explicit object parameter (`this Self& self`), so reflection sees the
most-derived class: no CRTP.

An Object belongs to the thread that constructed it (`thread()`); `move_to_thread(id)`
changes that. Destroy it on its own thread. Destruction disconnects everything it
sends or receives and drops queued deliveries still addressed to it.

## Signals

| Form | Declaration | Emit |
|---|---|---|
| (a) function | `[[=rqt::signal]] void clicked(int x) { rqt::emit{this}(x); }` | call `clicked(3)` |
| (b) descriptor | `static constexpr rqt::signal_of<int> valueChanged{};` | `valueChanged.emit(this, 3)` |
| NOTIFY | `[[=rqt::property{.notify = true}]] int progress;` | `set<"progress">(v)` emits `progressChanged(int)` when the value changes |

Form (a) checks at run time that the arguments passed to `rqt::emit{this}(...)` have the
signal's parameter types, and aborts with a message if not.

`obj.emit<D>(args...)` emits a signal by name or member pointer from outside.

## Connecting

```cpp
sender.connect<&Sender::clicked>(receiver, &Receiver::onClicked);   // by member pointer
sender.connect<"clicked">(receiver, &Receiver::onClicked);          // by name, checked at compile time
rqt::connect<"clicked">(sender, receiver, &Receiver::onClicked);    // free-function spelling
sender.connect<"clicked">(callable);                                // no receiver: always direct
```

An unknown name is a compile error (`no signal named 'x' in Sender`). The slot may take
a prefix of the signal's arguments. The optional last argument is the `rqt::connection_type`:

| Type | Behaviour |
|---|---|
| `automatic` (default) | direct if the receiver lives on the emitting thread, else queued |
| `direct` | called on the emitting thread, arguments by reference |
| `queued` | posted to the receiver's thread; arguments are copied into the event |
| `blocking_queued` | queued, and the emitter waits for the slot; direct on the receiver's own thread |

`connect` returns an `rqt::connection` (copyable handle): `connected()`, `disconnect()`, `id()`.
Disconnecting from inside a slot during an emit is safe: emit iterates a snapshot and skips
connections dropped meanwhile. `receivers<D>()` counts connections with a live receiver.

## Properties

`get<"name">()` and `set<"name">(v)` work on members annotated `[[=rqt::property]]`.
`set` skips the assignment and the NOTIFY signal if the new value compares equal.

## Event loops

`rqt::EventLoop` is one per thread (a second throws `std::logic_error`). `run()` processes
posted tasks and queued deliveries in post order until `quit()` (thread-safe);
`process_events()` runs what is queued and returns the count. `rqt::post(thread_id, task)`
queues any callable for a thread's loop; a thread without a loop yet keeps it until the loop starts.
Queued events still pending when a loop is destroyed are dropped.

## Known limits

See `tests/core/limit_*`: each pins a GCC 16.2 behaviour with the compiler text.
