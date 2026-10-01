// LIMIT (GCC 16.2): inside a template, a call to a function template whose
// template argument is a std::meta::info NTTP, written as a member of a
// designated initializer, is rejected while the template body is parsed:
//
//   error: consteval-only expressions are only allowed in a constant-evaluated
//   context [-Wtemplate-body]
//       .call = bind_for_signal<S>(std::move(f))})};
//
// A reduced reproducer without the library compiles cleanly; the trigger
// needs the connect_to shape (a constexpr local next to the call and a
// braced argument). The same call as a plain statement compiles, so rqt
// hoists it into a local (detail::connect_to). This file must NOT build; the
// CTest test of the same name passes while GCC still rejects it.
#include <reflect_moc/core.hpp>

namespace rqt::detail {

template <std::meta::info S, class F>
connection connect_to_inline(Object const& sender, receiver_token receiver, connection_type type, F f) {
  constexpr void const* key = signal_key<S>();
  auto const& handle = access::handle(sender);
  return connection{handle, attach(*handle, {.id = next_connection_id(),
                                             .signal = key,
                                             .receiver = std::move(receiver),
                                             .type = type,
                                             .call = bind_for_signal<S>(std::move(f))})};
}

}  // namespace rqt::detail

struct Sender : rqt::Object {
  [[= rqt::signal]] void ping(int x) { rqt::emit{this}(x); }
};

int main() {
  Sender sender;
  constexpr std::meta::info S = rqt::detail::resolve_signal<Sender, &Sender::ping>();
  rqt::detail::connect_to_inline<S>(sender, {.state = {}, .bound = false}, rqt::connection_type::direct, [](int) {});
}
