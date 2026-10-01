#pragma once

namespace rqt {

// Marks a member function as a signal (form a). Its body is the one line
// `rqt::emit{this}(args...);`.
struct signal_t {};
inline constexpr signal_t signal{};

// Marks a member function as a slot. Optional: any member function or
// callable can be connected; the annotation is for introspection only.
struct slot_t {};
inline constexpr slot_t slot{};

// Marks a data member as a property, accessed with `obj.set<"name">(v)` and
// `obj.get<"name">()`. With `.notify = true` the class gets a generated
// signal `<name>Changed(T const&)` that `set` emits when the value changes.
struct property {
  bool notify = false;
};

// A static descriptor signal (form b): `static constexpr rqt::signal_of<int> valueChanged{};`
template <class... A>
struct signal_of;

}  // namespace rqt
