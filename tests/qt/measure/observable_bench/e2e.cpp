// End-to-end cost of one change to an observable container with its list model connected: the mutator, the
// "about" and "done" signals, the model's slot and Qt's begin/end bookkeeping. No view and no model tester is
// attached, so what is timed is the signal path plus the model, and nothing else.
//
// One source, two builds. -DBENCH_QV builds the moc design (examples/qt/observable_models_qvariant, keys boxed
// in a QVariant); the default builds examples/qt/observable_models (typed rqt::signal). The row logic of the
// two models is identical, so the cost of finding a row cancels and the difference is how the key travels.
//
// Output, one line per measurement: e2e,<variant>,<container>,<size>,<phase>,<ns per change>
// A phase runs over a container that grows from empty to <size> (fill), is changed in place (replace, move) and
// is emptied again (drain), so each figure is the mean over sizes 0..<size>.
#ifdef BENCH_QV
#include <container_model.h>
namespace impl = observable_qv;
constexpr const char* kVariant = "qvariant";
#else
#include <container_model.hpp>
namespace impl = observable;
constexpr const char* kVariant = "rqt";
#endif

#include <QtCore/QCoreApplication>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace {
constexpr unsigned kSeed = 20261020;
constexpr int kTotalChanges = 100'000;  // per phase, split into repetitions so every size does the same work
constexpr int kDefaultSize = 1000;

struct Sample {
  int value = 0;
};

using Clock = std::chrono::steady_clock;

double ns_since(Clock::time_point start, long changes) {
  const auto elapsed = std::chrono::duration<double, std::nano>(Clock::now() - start).count();
  return elapsed / static_cast<double>(changes);
}

void print(const char* container, int size, const char* phase, double ns) {
  std::printf("e2e,%s,%s,%d,%s,%.1f\n", kVariant, container, size, phase, ns);
}

// The keys a run uses: for a list, an index that is valid at the moment of the change; for the others a
// shuffled set of distinct keys. Drawn before timing so the generator is not part of the measurement.
template <class C>
void run(const char* name, int size, int reps) {
  using Traits = observable::ContainerTraits<C>;
  using Key = typename Traits::Key;
  using Source = impl::ObservableContainer<C>;
  constexpr bool indexed = Traits::order == observable::Order::indexed;

  std::mt19937 rng(kSeed);
  std::vector<Key> keys(static_cast<std::size_t>(size));
  std::iota(keys.begin(), keys.end(), Key{});
  std::shuffle(keys.begin(), keys.end(), rng);
  std::vector<Key> later = keys;  // replace and drain visit the keys in another order than they were added in
  std::shuffle(later.begin(), later.end(), rng);
  std::vector<Key> picks(static_cast<std::size_t>(size));  // list: a random row for every step
  for (int i = 0; i < size; ++i) picks[static_cast<std::size_t>(i)] = static_cast<Key>(rng() % static_cast<unsigned>(i + 1));

  double fill = 0, replace = 0, move = 0, drain = 0;
  long moves = 0;
  for (int rep = 0; rep < reps; ++rep) {
    Source source;
    impl::ContainerModel<C> model(&source, [](const Key&, const typename Traits::Value&) { return QVariant(1); });

    auto t = Clock::now();
    for (int i = 0; i < size; ++i) source.insert(indexed ? picks[static_cast<std::size_t>(i)] : keys[static_cast<std::size_t>(i)], {});
    fill += ns_since(t, size) / reps;

    t = Clock::now();
    for (int i = 0; i < size; ++i) source.replace(indexed ? picks[static_cast<std::size_t>(i)] % size : later[static_cast<std::size_t>(i)], {});
    replace += ns_since(t, size) / reps;

    if constexpr (Traits::movable) {
      t = Clock::now();
      for (int i = 0; i + 1 < size; ++i) source.move(picks[static_cast<std::size_t>(i)] % size, static_cast<Key>((i * 7) % size));
      move += ns_since(t, size - 1) / reps;
      ++moves;
    }

    t = Clock::now();
    for (int i = 0; i < size; ++i) source.remove(indexed ? Key{0} + (picks[static_cast<std::size_t>(i)] % (size - i)) : later[static_cast<std::size_t>(i)]);
    drain += ns_since(t, size) / reps;
  }
  print(name, size, "fill", fill);
  print(name, size, "replace", replace);
  if (moves) print(name, size, "move", move);
  print(name, size, "drain", drain);
}

template <class C>
void sweep(const char* name, const std::vector<int>& sizes) {
  for (const int size : sizes) run<C>(name, size, std::max(1, kTotalChanges / size));
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  const bool scale = argc > 1 && std::string(argv[1]) == "--scale";
  // --size N [--only QList|QHash|QMap] runs one size, for chasing a single figure.
  int one = 0;
  std::string only;
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--size") one = std::atoi(argv[i + 1]);
    if (std::string(argv[i]) == "--only") only = argv[i + 1];
  }
  const std::vector<int> sizes = one ? std::vector<int>{one}
                                     : scale ? std::vector<int>{100, 1000, 10000, 100000} : std::vector<int>{kDefaultSize};
  const std::vector<int> mapSizes = scale ? std::vector<int>{100, 1000, 10000} : sizes;  // QMap's row count walks the tree

  std::printf("size,%s,QList<Sample>,%zu\n", kVariant, sizeof(impl::ObservableContainer<QList<Sample>>));
  std::printf("size,%s,QHash<quint32,Sample>,%zu\n", kVariant, sizeof(impl::ObservableContainer<QHash<quint32, Sample>>));
  if (only.empty() || only == "QList") sweep<QList<Sample>>("QList", sizes);
  if (only.empty() || only == "QHash") sweep<QHash<quint32, Sample>>("QHash", sizes);
  if (only.empty() || only == "QMap") sweep<QMap<quint32, Sample>>("QMap", mapSizes);
  return 0;
}
