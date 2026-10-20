// The row indexes of the observable models against a plain vector, under random inserts and removals, with sizes
// that force the insertion-order index to compact its dead slots (more than 64 slots, more than half dead).
#include "check.hpp"

#include <row_index.hpp>

#include <QtCore/QCoreApplication>

#include <algorithm>
#include <optional>
#include <random>
#include <vector>

namespace {
constexpr unsigned kSeed = 7;
constexpr int kSteps = 30000;
constexpr int kKeyDomain = 700;  // wide enough for hundreds of live keys, narrow enough to collide
constexpr int kDrainEvery = 4000;  // now and then remove almost everything, so dead slots outnumber live ones

template <class Index, class Reference>
void agree(const Index& index, const Reference& expected) {
  for (std::size_t row = 0; row < expected.size(); ++row) {
    CHECK(index.keyAt(static_cast<qsizetype>(row)) == expected[row]);
    CHECK(index.rowOf(expected[row]) == static_cast<qsizetype>(row));
  }
}

void insertion_order() {
  observable::InsertionOrderIndex<unsigned> index;
  std::vector<unsigned> expected;
  std::mt19937 rng(kSeed);
  for (int step = 0; step < kSteps; ++step) {
    const unsigned key = rng() % kKeyDomain;
    const auto at = std::find(expected.begin(), expected.end(), key);
    if (step % kDrainEvery > kDrainEvery - kKeyDomain / 2 && at != expected.end()) {
      index.erase(key, at - expected.begin());
      expected.erase(at);
    } else if (at == expected.end() && step % 3 != 0) {
      CHECK(index.rowToInsert(key) == static_cast<qsizetype>(expected.size()));
      index.insert(key, static_cast<qsizetype>(expected.size()));
      expected.push_back(key);
    } else if (at != expected.end()) {
      index.erase(key, at - expected.begin());
      expected.erase(at);
    }
    if (step % 97 == 0) agree(index, expected);
  }
  agree(index, expected);
  index.clear();
  CHECK(index.rowToInsert(0u) == 0);
}

void sorted() {
  observable::SortedIndex<int> index;
  std::vector<int> expected;
  std::mt19937 rng(kSeed);
  for (int step = 0; step < kSteps; ++step) {
    const int key = static_cast<int>(rng() % kKeyDomain);
    const auto at = std::lower_bound(expected.begin(), expected.end(), key);
    const bool present = at != expected.end() && *at == key;
    const qsizetype row = at - expected.begin();
    CHECK(index.rowToInsert(key) == row);
    if (present) {
      index.erase(key, row);
      expected.erase(at);
    } else {
      index.insert(key, row);
      expected.insert(at, key);
    }
    if (step % 97 == 0) agree(index, expected);
  }
  agree(index, expected);
}

// The multi indexes against a plain list of the keys of every row. For an unordered multi container the row order
// is insertion order and the entry (key, n) is the n-th from the newest of that key; for a sorted one the rows
// are ordered by key, the newest of a key first, so (key, n) sits at the key's first row plus n.
template <class Index, bool Sorted>
void multi() {
  using Key = observable::MultiKey<int>;
  Index index;
  std::vector<int> rows;  // the key of each row
  std::mt19937 rng(kSeed);
  const auto first_row = [&](int key) { return static_cast<qsizetype>(std::lower_bound(rows.begin(), rows.end(), key) - rows.begin()); };
  const auto occurrence = [&](std::size_t row) -> qsizetype {
    if (Sorted) return static_cast<qsizetype>(row) - first_row(rows[row]);
    qsizetype newer = 0;
    for (std::size_t r = row + 1; r < rows.size(); ++r) newer += rows[r] == rows[row];
    return newer;
  };
  const auto row_of = [&](int key, qsizetype n) -> std::optional<std::size_t> {
    std::vector<std::size_t> of;
    for (std::size_t r = 0; r < rows.size(); ++r)
      if (rows[r] == key) of.push_back(r);
    if (n < 0 || n >= static_cast<qsizetype>(of.size())) return std::nullopt;
    return Sorted ? of[static_cast<std::size_t>(n)] : of[of.size() - 1 - static_cast<std::size_t>(n)];
  };
  const auto agrees = [&] {
    for (std::size_t r = 0; r < rows.size(); ++r) {
      const Key entry = index.keyAt(static_cast<qsizetype>(r));
      CHECK(entry.key == rows[r] && entry.occurrence == occurrence(r));
      CHECK(index.rowOf(entry) == static_cast<qsizetype>(r));
    }
  };
  for (int step = 0; step < kSteps; ++step) {
    const int key = static_cast<int>(rng() % 12);  // twelve keys: most entries share one
    const bool drain = step % kDrainEvery > kDrainEvery - 800;
    if (!drain && (rows.empty() || rng() % 3 != 0)) {
      const qsizetype row = Sorted ? first_row(key) : static_cast<qsizetype>(rows.size());
      CHECK(index.rowToInsert(Key{key, 0}) == row);
      index.insert(Key{key, 0}, row);
      rows.insert(rows.begin() + row, key);
    } else if (const auto at = row_of(key, static_cast<qsizetype>(rng() % 4))) {
      const Key entry{key, occurrence(*at)};
      index.erase(entry, static_cast<qsizetype>(*at));
      rows.erase(rows.begin() + static_cast<std::ptrdiff_t>(*at));
    }
    if (step % 101 == 0) agrees();
  }
  agrees();
  index.clear();
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  insertion_order();
  sorted();
  multi<observable::MultiSortedIndex<int>, true>();
  multi<observable::MultiInsertionIndex<int>, false>();
  return rqt_test::finish("capability_row_index");
}
