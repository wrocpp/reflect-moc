// A host of a selection tracker, driven by thousands of random changes: rows inserted and removed one at a time and in runs, the
// content replaced and cleared, rows selected and deselected, the selection cleared. After every change the host's selected role,
// count, presence and selected value must equal a plain reference, and the notifications must be exact: "has selection" is
// announced once when the presence changed and never otherwise, the count or the selected row once when it changed and never
// otherwise, and dataChanged names exactly the rows whose isSelected changed by a request. QAbstractItemModelTester runs in
// fatal mode.
#include "check.hpp"
#include "selection_hosts.hpp"

#include <QtCore/QCoreApplication>
#include <QtTest/QAbstractItemModelTester>

#include <cstdio>
#include <optional>
#include <random>
#include <set>
#include <vector>

namespace {
using namespace selection_test;

constexpr int kSteps = 4000;
constexpr unsigned kSeeds[] = {1, 2, 3, 4, 5, 6, 7, 8};
constexpr int kMaxRows = 14;      // the content never grows past this, so rows collide with the selection often
constexpr int kLongestRun = 4;
constexpr int kKinds = 100;
constexpr int kWeightInsert = 14;
constexpr int kWeightRemove = 12;
constexpr int kWeightInsertRange = 8;
constexpr int kWeightRemoveRange = 8;
constexpr int kWeightReset = 4;
constexpr int kWeightClearContent = 2;
constexpr int kWeightSelect = 36;
constexpr int kWeightClearSelection = 6;  // the rest is a deselect

struct Row {
  int id;
  bool selected;
};

template <class Host>
constexpr bool isMulti = requires(Host& h) { h.selectedCountChanged; };

template <class Host>
int run(unsigned seed) {
  Host host;
  QAbstractItemModelTester tester(&host, QAbstractItemModelTester::FailureReportingMode::Fatal);
  std::mt19937 rng(seed);
  const auto draw = [&](int low, int high) { return std::uniform_int_distribution<int>(low, high - 1)(rng); };
  std::vector<Row> shadow;
  int nextId = 0;
  int problems = 0;
  int withSelection = 0;  // steps that ended with something selected: the driver must reach that state
  int hasEmitted = 0, countEmitted = 0, rowEmitted = 0;
  std::set<int> changedRows;  // rows named by dataChanged with the selected role
  QObject::connect(&host, &Host::hasSelectionChanged, &host, [&] { ++hasEmitted; });
  if constexpr (isMulti<Host>) QObject::connect(&host, &Host::selectedCountChanged, &host, [&] { ++countEmitted; });
  else QObject::connect(&host, &Host::selectedRowChanged, &host, [&] { ++rowEmitted; });
  QObject::connect(&host, &QAbstractItemModel::dataChanged, &host, [&](const QModelIndex& a, const QModelIndex& b, const QList<int>& roles) {
    if (roles == QList<int>{Host::SelectedRole})
      for (int row = a.row(); row <= b.row(); ++row) changedRows.insert(row);
  });

  const auto count = [&] { return static_cast<int>(std::count_if(shadow.begin(), shadow.end(), [](const Row& r) { return r.selected; })); };
  const auto selected_row = [&]() -> std::optional<int> {
    for (std::size_t i = 0; i < shadow.size(); ++i)
      if (shadow[i].selected) return static_cast<int>(i);
    return std::nullopt;
  };
  const auto item = [&](int id) { return ColorItem{.name = QStringLiteral("c%1").arg(id), .hex = QStringLiteral("#%1").arg(id)}; };
  const auto fresh_items = [&](int n) {
    ColorList items;
    for (int i = 0; i < n; ++i) items.append(item(nextId + i));
    return items;
  };

  for (int step = 0; step < kSteps; ++step) {
    const bool hadSelection = count() > 0;
    const int countBefore = count();
    const std::optional<int> rowBefore = selected_row();
    std::vector<bool> selectedBefore;
    for (const Row& r : shadow) selectedBefore.push_back(r.selected);
    hasEmitted = countEmitted = rowEmitted = 0;
    changedRows.clear();
    bool requestFlips = false;  // a request changed isSelected of rows (and must say so with dataChanged)
    std::set<int> flipped;

    const int kind = draw(0, kKinds);
    int edge = 0;
    const auto at = [&](int weight) { return kind < (edge += weight); };
    const int size = static_cast<int>(shadow.size());
    if (at(kWeightInsert) && size < kMaxRows) {
      const int row = draw(0, size + 1);
      host.insertItem(row, item(nextId));
      shadow.insert(shadow.begin() + row, Row{nextId++, false});
    } else if (at(kWeightRemove) && size > 0) {
      const int row = draw(0, size);
      host.removeItem(row);
      shadow.erase(shadow.begin() + row);
    } else if (at(kWeightInsertRange) && size < kMaxRows - kLongestRun) {
      const int row = draw(0, size + 1);
      const int n = draw(1, kLongestRun + 1);
      host.container().insertRange(row, fresh_items(n));
      for (int i = 0; i < n; ++i) shadow.insert(shadow.begin() + row + i, Row{nextId++, false});
    } else if (at(kWeightRemoveRange) && size > 0) {
      const int row = draw(0, size);
      const int n = std::min(draw(1, kLongestRun + 1), size - row);
      host.container().removeRange(row, n);
      shadow.erase(shadow.begin() + row, shadow.begin() + row + n);
    } else if (at(kWeightReset)) {
      const int n = draw(0, kMaxRows / 2);
      host.resetWith(fresh_items(n));
      shadow.clear();
      for (int i = 0; i < n; ++i) shadow.push_back(Row{nextId++, false});
    } else if (at(kWeightClearContent)) {
      host.container().clear();
      shadow.clear();
    } else if (at(kWeightSelect) && size > 0) {
      const int row = draw(0, size);
      const bool value = draw(0, 3) != 0;  // mostly select
      if constexpr (isMulti<Host>) {
        flipped = shadow[static_cast<std::size_t>(row)].selected != value ? std::set<int>{row} : std::set<int>{};
        shadow[static_cast<std::size_t>(row)].selected = value;
      } else {
        if (value) {
          for (std::size_t i = 0; i < shadow.size(); ++i)
            if (shadow[i].selected && static_cast<int>(i) != row) flipped.insert(static_cast<int>(i));
          if (!shadow[static_cast<std::size_t>(row)].selected) flipped.insert(row);
          for (Row& r : shadow) r.selected = false;
          shadow[static_cast<std::size_t>(row)].selected = true;
        } else if (shadow[static_cast<std::size_t>(row)].selected) {
          flipped.insert(row);
          shadow[static_cast<std::size_t>(row)].selected = false;
        }
      }
      requestFlips = true;
      host.setData(host.index(row), value, Host::SelectedRole);
    } else if (at(kWeightClearSelection)) {
      for (std::size_t i = 0; i < shadow.size(); ++i)
        if (shadow[i].selected) flipped.insert(static_cast<int>(i));
      for (Row& r : shadow) r.selected = false;
      requestFlips = true;
      host.clearSelection();
    }

    withSelection += count() > 0 ? 1 : 0;
    const auto expect = [&](bool ok, const char* what) {
      if (ok) return;
      ++problems;
      if (problems <= 3) std::fprintf(stderr, "seed %u step %d: %s\n", seed, step, what);
    };
    expect(host.rowCount() == static_cast<int>(shadow.size()), "row count");
    for (int row = 0; row < host.rowCount() && row < static_cast<int>(shadow.size()); ++row)
      expect(host.data(host.index(row), Host::SelectedRole).toBool() == shadow[static_cast<std::size_t>(row)].selected, "isSelected of a row");
    expect(host.hasSelection() == (count() > 0), "hasSelection");
    if constexpr (isMulti<Host>) {
      expect(host.selectedCount() == count(), "selectedCount");
    } else {
      expect(host.selectedName() == (selected_row() ? item(shadow[static_cast<std::size_t>(*selected_row())].id).name : QString()), "selectedName");
    }
    // Exactly one notification when the presence changed, none when it did not.
    expect(hasEmitted == ((count() > 0) != hadSelection ? 1 : 0), "hasSelectionChanged exactly when the presence changed");
    if constexpr (isMulti<Host>)
      expect(countEmitted == (count() != countBefore ? 1 : 0), "selectedCountChanged exactly when the count changed");
    else
      expect(rowEmitted == (selected_row() != rowBefore ? 1 : 0), "selectedRowChanged exactly when the selected row changed");
    if (requestFlips) {
      // dataChanged names the rows whose isSelected a request changed, and no others.
      expect(changedRows == flipped, "dataChanged names exactly the rows that flipped");
    } else {
      expect(changedRows.empty(), "a structural change does not say dataChanged for the selected role");
    }
  }
  if (withSelection < kSteps / 4) {
    std::fprintf(stderr, "seed %u: only %d steps had a selection\n", seed, withSelection);
    ++problems;
  }
  return problems;
}

template <class Host>
bool every_seed(const char* name) {
  int problems = 0;
  for (const unsigned seed : kSeeds) problems += run<Host>(seed);
  std::printf("%-8s %d problems\n", name, problems);
  return problems == 0;
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  CHECK(every_seed<ColorSingleSelectionModel>("Single"));
  CHECK(every_seed<ColorMultiSelectionModel>("Multi"));
  return rqt_test::finish("capability_observable_selection_random");
}
