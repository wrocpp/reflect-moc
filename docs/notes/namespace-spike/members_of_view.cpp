#include <meta>
#include <cstdio>
namespace m_ = std::meta;
using info = m_::info;
constexpr auto unchecked = m_::access_context::unchecked();
consteval int count(info ns) { return int(m_::members_of(ns, unchecked).size()); }
template <info Ns = m_::current_namespace()> inline constexpr int var_count = count(Ns);
template <info Ns = m_::current_namespace()> bool verify_fn(int* out) { constexpr int n = count(Ns); *out = n; return true; }
template <info Ns = m_::current_namespace()> inline const int lazy_var = count(Ns);
namespace A {
  enum E1 {};
  inline constexpr int early = var_count<>;
  inline const int& lazy_ref = lazy_var<>;
  inline int fn_result = 0;
  inline bool ok = verify_fn<>(&fn_result);
  enum E2 {};
  enum E3 {};
}
int main() {
  std::printf("constexpr var at line: %d\n", A::early);
  std::printf("lazy const var template: %d\n", A::lazy_ref);
  std::printf("function body (non-constexpr) : %d\n", A::fn_result);
  std::printf("direct from main: %d\n", count(^^A));
}
