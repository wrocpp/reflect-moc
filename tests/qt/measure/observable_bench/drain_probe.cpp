// Where the cost of removing from a hash model goes, with no signals at all. The model keeps its keys in a
// QList in the order they were added, finds a key with indexOf and removes the row with removeAt. This times those
// two steps alone, for a list of N keys drained in random order, and repeats it with the list allocated at
// different offsets (pad bytes allocated first), to see whether the figure depends on the signal transport or on
// where the list happens to sit in memory.
//
// Output: probe,<pad>,<scan ns>,<remove ns>,<both ns>   (ns per key, mean over the drain)
#include <QtCore/QList>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <random>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
constexpr unsigned kSeed = 20261020;

double ns_per(Clock::time_point start, long n) {
  return std::chrono::duration<double, std::nano>(Clock::now() - start).count() / static_cast<double>(n);
}
}  // namespace

int main(int argc, char** argv) {
  const int n = argc > 1 ? std::atoi(argv[1]) : 100000;
  const int pad = argc > 2 ? std::atoi(argv[2]) : 0;
  const int reps = argc > 3 ? std::atoi(argv[3]) : 3;

  std::mt19937 rng(kSeed);
  std::vector<quint32> added(static_cast<std::size_t>(n));
  std::iota(added.begin(), added.end(), quint32{});
  std::shuffle(added.begin(), added.end(), rng);
  std::vector<quint32> later = added;
  std::shuffle(later.begin(), later.end(), rng);

  // The row each key has at the moment it is removed, worked out beforehand so the timed loop does only the two steps.
  std::vector<quint32> sim = added;
  std::vector<qsizetype> rows(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    const auto at = std::find(sim.begin(), sim.end(), later[static_cast<std::size_t>(i)]);
    rows[static_cast<std::size_t>(i)] = at - sim.begin();
    sim.erase(at);
  }

  void* keep = pad ? std::malloc(static_cast<std::size_t>(pad)) : nullptr;
  double scan = 0, remove = 0, both = 0;
  long sink = 0;
  for (int rep = 0; rep < reps; ++rep) {
    QList<quint32> order;
    for (const quint32 k : added) order.append(k);
    QList<quint32> copy = order;
    copy.detach();

    auto t = Clock::now();
    for (int i = 0; i < n; ++i) sink += copy.indexOf(later[static_cast<std::size_t>(i)]);  // the list does not shrink
    scan += ns_per(t, n) / reps;

    t = Clock::now();
    for (int i = 0; i < n; ++i) order.removeAt(rows[static_cast<std::size_t>(i)]);
    remove += ns_per(t, n) / reps;

    order.clear();
    for (const quint32 k : added) order.append(k);
    t = Clock::now();
    for (int i = 0; i < n; ++i) {
      const qsizetype row = order.indexOf(later[static_cast<std::size_t>(i)]);
      order.removeAt(row);
    }
    both += ns_per(t, n) / reps;
  }
  std::printf("probe,%d,%.0f,%.0f,%.0f\n", pad, scan, remove, both);
  std::free(keep);
  return sink == -1;
}
