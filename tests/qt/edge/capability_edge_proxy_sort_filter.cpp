// Proxy edge cases, sorting and filtering a flat model: QSortFilterProxyModel over the list and table models of a
// QList, QMap, QHash and QSet, with every sort role and both settings of dynamicSortFilter. After every change the
// proxy must be a consistent view of the model (each row maps to one source row and back and shows its data), and
// it must list what the model lists in the order the sort role gives. QAbstractItemModelTester runs in fatal mode
// on the model and on every proxy.
// Catalogue: docs/qt-edge-cases/proxies-selection.md.
#include "proxy_support.hpp"

#include <QtCore/QCoreApplication>
#include <QtCore/QLocale>
#include <QtCore/QRegularExpression>

#include <algorithm>

namespace {
using namespace edge_proxy;

using Entry = std::pair<int, QString>;

template <class C>
C entries_of(const std::vector<Entry>& list) {
  C out;
  for (const Entry& e : list) {
    if constexpr (Setup<C>::isList)
      out.append(e.second);
    else if constexpr (Setup<C>::hasValue)
      out.insert(static_cast<typename Setup<C>::Key>(e.first), e.second);
    else
      out.insert(static_cast<typename Setup<C>::Key>(e.first));
  }
  return out;
}

struct Options {
  int sortRole = Qt::DisplayRole;
  bool dynamic = true;
  Qt::CaseSensitivity sortCase = Qt::CaseSensitive;
};

bool less_for(const Options& o, const QVariant& a, const QVariant& b) {
  if (a.metaType().id() == QMetaType::QString) return a.toString().compare(b.toString(), o.sortCase) < 0;
  return a.toInt() < b.toInt();
}

// The proxy lists exactly the model's rows. Where it sorts, the rows come in non-decreasing order of the sort role:
// entries that compare equal (Apple and apple, case-insensitively) may come in either order, which no contract fixes.
template <bool Strict>
void expect_view(const QSortFilterProxyModel& proxy, const QAbstractItemModel& model, const Options& o) {
  CHECK(mapping_is_consistent(proxy));
  CHECK(proxy.rowCount() == model.rowCount());
  QList<int> seen;
  for (int row = 0; row < proxy.rowCount(); ++row) seen.append(proxy.mapToSource(proxy.index(row, 0)).row());
  QList<int> sorted = seen;
  std::sort(sorted.begin(), sorted.end());
  for (int row = 0; row < sorted.size(); ++row) CHECK(sorted[row] == row);  // a permutation of the source rows
  if constexpr (Strict) {
    for (int row = 1; row < proxy.rowCount(); ++row)
      CHECK(!less_for(o, proxy.index(row, 0).data(o.sortRole), proxy.index(row - 1, 0).data(o.sortRole)));
  }
}

template <class C, bool Table>
void sorted_through_every_change(const Options& o) {
  using S = Setup<C>;
  Rig<C, Table> rig;
  ProxyRig pr(rig.model.get());
  QSortFilterProxyModel& proxy = *pr.proxy;
  proxy.setSortRole(o.sortRole);
  proxy.setSortCaseSensitivity(o.sortCase);
  proxy.setDynamicSortFilter(o.dynamic);
  proxy.sort(0);

  const auto check = [&] {
    if (o.dynamic) {
      expect_view<true>(proxy, *rig.model, o);
    } else {
      expect_view<false>(proxy, *rig.model, o);
      proxy.invalidate();  // the one way to get the order back when it does not follow the data
      expect_view<true>(proxy, *rig.model, o);
    }
  };
  const auto key_of_row = [&](int row) {
    if constexpr (S::isList)
      return static_cast<typename S::Key>(row);
    else
      return rig.model->index(row, 0).data(kKeyRole).template value<typename S::Key>();
  };

  const std::vector<Entry> first{{5, "pear"}, {2, "Apple"}, {9, "banana"}, {1, "cherry"}, {7, "apple"}, {3, "Banana"}};
  for (const Entry& e : first) CHECK(S::add(rig.source, e.first, e.second));
  check();

  if constexpr (S::hasValue) {
    // A value that moves the entry to the other end of the display order.
    CHECK(rig.source.replace(key_of_row(0), QString("zzz")));
    check();
    CHECK(rig.source.editValue(key_of_row(2), [](QString& v) { v = "000"; }));
    check();
    CHECK(rig.source.editValue(key_of_row(1), [](QString& v) { v += "!"; }, {Qt::DisplayRole}));
    check();
  }
  CHECK(rig.source.remove(key_of_row(1)));
  check();

  if constexpr (S::isList) {
    QList<QString> run{"m1", "a0", "z9"};
    CHECK(rig.source.insertRange(1, run));
    check();
    CHECK(rig.source.removeRange(0, 2));
    check();
    CHECK(rig.source.move(0, rig.source.size() - 1));
    check();
  } else {
    CHECK(rig.source.insertMany(entries_of<C>({{40, "kiwi"}, {41, "Kiwi"}, {12, "lime"}})) == 3);
    check();
    CHECK(rig.source.removeMany({40, 12, 99}) == 2);
    check();
  }
  rig.source.resetWith(entries_of<C>({{3, "x"}, {1, "Y"}, {2, "w"}}));
  check();
  CHECK(rig.source.clear());
  check();
  CHECK(proxy.rowCount() == 0);
}

template <class C, bool Table>
void every_option() {
  for (const bool dynamic : {true, false}) {
    sorted_through_every_change<C, Table>({Qt::DisplayRole, dynamic, Qt::CaseSensitive});
    sorted_through_every_change<C, Table>({Qt::DisplayRole, dynamic, Qt::CaseInsensitive});
    if constexpr (!Setup<C>::isList) sorted_through_every_change<C, Table>({kKeyRole, dynamic, Qt::CaseSensitive});
  }
}

// the four containers, list and table, through every sort role and setting.
void sort_roles_and_dynamic_sort_filter() {
  every_option<QList<QString>, false>();
  every_option<QList<QString>, true>();
  every_option<QMap<int, QString>, false>();
  every_option<QMap<int, QString>, true>();
  every_option<QHash<int, QString>, false>();
  every_option<QHash<int, QString>, true>();
  every_option<QSet<int>, false>();
  every_option<QSet<int>, true>();
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  sort_roles_and_dynamic_sort_filter();
  return rqt_test::finish("capability_edge_proxy_sort_filter");
}
