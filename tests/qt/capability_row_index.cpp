// The row indexes of the observable models against a plain vector, under random inserts and removals, with sizes
// that force the insertion-order index to compact its dead slots (more than 64 slots, more than half dead).
#include "check.hpp"

#include <row_index.hpp>

#include <QtCore/QCoreApplication>

#include <algorithm>
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
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  insertion_order();
  sorted();
  return rqt_test::finish("capability_row_index");
}
