// Pinned: a Qt container counts in qsizetype (64 bit), a model in int. ContainerModelBase takes the row count with
// static_cast<int>(source->size()) when it is made and after a reset, and casts every row, key and run bound to int
// in the same way. Past INT_MAX the cast wraps: a size of 2^31 reads as a negative row count, and 2^32 + 3 as 3,
// with no error and nothing for QAbstractItemModelTester to say once a view has trusted the number.
// A container with 2^31 entries is not built here: a traits specialisation for a stand-in type reports the size, and
// holds nothing. The same casts appear (static_cast<int>) in connectSource, insertRuns and removeRuns for a
// pending row, a changed row, a move, the range signals and the bounds of every run.
// A fix refuses to follow a container larger than INT_MAX (or saturates the count and says so); this file then
// becomes capability_edge_data_size_int_cast.
// Catalogue: docs/qt-edge-cases/data-types.md (D-30).
#include "data_support.hpp"

#include <climits>

namespace {
struct BigList {
  qsizetype entries = 0;
  void clear() { entries = 0; }
};
}  // namespace

namespace observable {
template <>
struct ContainerTraits<BigList> {
  using Key = qsizetype;
  using Value = int;
  static constexpr Order order = Order::indexed;
  static constexpr bool movable = false;
  static constexpr bool assignable = true;
  static qsizetype size(const BigList& c) { return c.entries; }
  static bool contains(const BigList& c, Key k) { return k >= 0 && k < c.entries; }
  static bool can_insert(const BigList& c, Key k) { return k >= 0 && k <= c.entries; }
  static bool can_move(const BigList&, Key, Key) { return false; }
  static void insert(BigList& c, Key, int) { ++c.entries; }
  static void erase(BigList& c, Key) { --c.entries; }
  static void assign(BigList&, Key, int) {}
  static void move(BigList&, Key, Key) {}
  static const int& value(const BigList&, Key) {
    static const int zero = 0;
    return zero;
  }
  static QList<Key> keys(const BigList&) { return {};  // the model only needs a list's keys to be rows, which it computes
  }
};
}  // namespace observable

namespace {
void a_size_past_int_max_wraps() {
  using observable::ContainerModel;
  using observable::ObservableContainer;
  const qsizetype pastIntMax = qsizetype{INT_MAX} + 1;

  // The largest count an int holds is carried faithfully.
  ObservableContainer<BigList> fits(BigList{INT_MAX});
  ContainerModel<BigList> fitsModel(&fits, [](const qsizetype&, const int&) { return QVariant(); });
  CHECK(fitsModel.rowCount() == INT_MAX);

  ObservableContainer<BigList> source(BigList{pastIntMax});
  ContainerModel<BigList> model(&source, [](const qsizetype&, const int&) { return QVariant(); });
  CHECK(source.size() == pastIntMax);
  CHECK(model.rowCount() == INT_MIN);  // wrapped to a negative count

  source.resetWith(BigList{(qsizetype{1} << 32) + 3});
  CHECK(source.size() == (qsizetype{1} << 32) + 3);
  CHECK(model.rowCount() == 3);        // wrapped to a small, plausible and wrong count
}
}  // namespace

int main() {
  a_size_past_int_max_wraps();
  return rqt_test::finish("limit_edge_data_size_int_cast");
}
