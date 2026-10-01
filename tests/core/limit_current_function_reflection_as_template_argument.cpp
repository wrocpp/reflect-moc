// LIMIT (GCC 16.2): a template specialised on the reflection that
// std::meta::current_function() returns, and again on the same function
// named directly, is emitted twice under one mangled name. The two
// reflections compare equal, the translation unit compiles, and the
// assembler rejects it:
//
//   error: symbol '__Z3tagILDmfnN6Button7clickedEvEE' is already defined
//
// So rqt keys form (a) signals by a per-class anchor array plus the signal's
// index (detail::anchored_key), which substitutes only the class type.
//
// This file must NOT build on macOS (Mach-O assembler); the CTest test of the
// same name is registered only there and passes while the assembler still
// reports the duplicate symbol. On Linux the build succeeds, so the test is
// not registered. When it starts to build on macOS, the workaround can go.
#include <meta>

template <std::meta::info S>
inline constexpr char tag = 0;

template <std::meta::info S>
inline constexpr void const* key_v = &tag<S>;

consteval void const* key_of(std::meta::info f) {
  return std::meta::extract<void const*>(std::meta::substitute(^^key_v, {std::meta::reflect_constant(f)}));
}

struct Button {
  void const* clicked() {
    constexpr void const* key = key_of(std::meta::current_function());
    return key;
  }
};

int main() { return Button{}.clicked() == key_of(^^Button::clicked) ? 0 : 1; }
