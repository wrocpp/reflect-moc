// The emit path of a signal with no connection, 10 million emits per run, three runs, for a first and a
// last signal of a class with four. Build twice (-O2), once with -DSTATIC_SIGNALS:
//   non-static: rqt::signal members, `a(v)`
//   static:     rqt::static_signal members, `emit a(v)` with reflect_moc/compat.hpp
#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>

#include <chrono>
#include <cstdio>

#if defined(STATIC_SIGNALS)
#include <reflect_moc/compat.hpp>
#define DECL(Sig, name) static inline rqt::static_signal<Sig> name{};
#define FIRE(name, ...) emit name(__VA_ARGS__)
#define VARIANT "static"
#else
#define DECL(Sig, name) rqt::signal<Sig> name;
#define FIRE(name, ...) name(__VA_ARGS__)
#define VARIANT "non-static"
#endif

namespace {
constexpr int kEmits = 10'000'000;
constexpr int kRuns = 3;
}  // namespace

class Bench : public QObject {
  RQT_OBJECT

 public:
  DECL(void(int), a)
  DECL(void(int), b)
  DECL(void(), c)
  DECL(void(int, int), d)
  [[gnu::noinline]] void fire_last(int v) { FIRE(d, v, v); }
  [[gnu::noinline]] void fire_first(int v) { FIRE(a, v); }
};

template <class F>
double nanoseconds_per_call(F f) {
  auto const begin = std::chrono::steady_clock::now();
  for (int i = 0; i < kEmits; ++i) f(i);
  auto const end = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::nano>(end - begin).count() / kEmits;
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Bench bench;
  for (int run = 0; run < kRuns; ++run) {
    double const first = nanoseconds_per_call([&](int i) { bench.fire_first(i); });
    double const last = nanoseconds_per_call([&](int i) { bench.fire_last(i); });
    std::printf("%s: first signal %.2f ns/emit, last signal %.2f ns/emit (sizeof %zu)\n", VARIANT, first, last,
                sizeof(Bench));
  }
}
