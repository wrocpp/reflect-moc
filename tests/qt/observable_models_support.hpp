// Support for the observable-model tests: what a view would believe, the signal contract, a plain
// reference implementation of each container's row order, and a seeded random driver that compares them.
//
// A ghost row is a row that a view shows and the model no longer has (or the reverse). A view only
// learns about changes from signals, so the ViewMirror below is built from the signals alone and
// compared with the live model after every step. Any change that was not announced, announced with
// the wrong rows, announced in the wrong order or never confirmed shows up as a difference.
#pragma once

#include "check.hpp"

#include <container_traits.hpp>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <algorithm>
#include <memory>
#include <optional>
#include <random>
#include <utility>
#include <vector>

namespace observable_test {

constexpr int kKeyDomain = 16;       // keys of a map, hash or set are drawn from [0, kKeyDomain), so they collide
constexpr int kIndexSlack = 2;       // a list index is drawn from [-1, size + kIndexSlack), so some are invalid
constexpr int kChangeKinds = 100;    // the weights below add up to this
constexpr int kWeightInsert = 30;
constexpr int kWeightRemove = 25;
constexpr int kWeightReplace = 15;
constexpr int kWeightMove = 14;
constexpr int kWeightSelect = 14;    // the rest is clear
constexpr int kKeyRole = Qt::UserRole;  // ContainerModel::KeyRole

struct Sample {
  int value = 0;
};

inline int to_int(const Sample& s) { return s.value; }
inline int to_int(const QString& s) { return s.toInt(); }
inline int to_int(unsigned v) { return static_cast<int>(v); }

template <class C>
struct Make {
  using Traits = observable::ContainerTraits<C>;
  using Key = typename Traits::Key;
  using Value = typename Traits::Value;
  static Value value(int n, const Key& key) {
    if constexpr (std::is_same_v<Value, Sample>)
      return Sample{n};
    else if constexpr (std::is_same_v<Value, QString>)
      return QString::number(n);
    else
      return static_cast<Value>(key);  // a set: the entry is its key
  }
  static int shown(const Key& key, const Value& value) {
    if constexpr (Traits::assignable)
      return to_int(value);
    else
      return to_int(key);
  }
};

// What a view believes, kept from the model's signals only.
struct MirrorRow {
  QVariant key;
  QVariant display;
  bool operator==(const MirrorRow&) const = default;
};

class ModelWatcher : public QObject {
 public:
  // keysAreStable is false for a list: its key is its row, which shifts under every insert and remove
  // without a signal, so a view cannot be expected to cache it.
  ModelWatcher(QAbstractItemModel* model, bool keysAreStable) : QObject(model), m_model(model), m_keysAreStable(keysAreStable) {
    m_rows = readAll();
    m_announcedCount = m_model->rowCount();
    using M = QAbstractItemModel;
    connect(model, &M::rowsAboutToBeInserted, this, [this](const QModelIndex&, int first, int last) {
      begin(Open::insert, first, last);
    });
    connect(model, &M::rowsInserted, this, [this](const QModelIndex&, int first, int last) {
      if (end(Open::insert, first, last, m_countBefore + (last - first + 1))) {
        for (int row = first; row <= last; ++row) m_rows.insert(m_rows.begin() + row, read(row));
      }
    });
    connect(model, &M::rowsAboutToBeRemoved, this, [this](const QModelIndex&, int first, int last) {
      begin(Open::remove, first, last);
    });
    connect(model, &M::rowsRemoved, this, [this](const QModelIndex&, int first, int last) {
      if (end(Open::remove, first, last, m_countBefore - (last - first + 1))) {
        if (last < static_cast<int>(m_rows.size())) m_rows.erase(m_rows.begin() + first, m_rows.begin() + last + 1);
      }
    });
    connect(model, &M::rowsAboutToBeMoved,
            this, [this](const QModelIndex&, int first, int last, const QModelIndex&, int destination) {
              begin(Open::move, first, last);
              m_moveDestination = destination;
            });
    connect(model, &M::rowsMoved, this, [this](const QModelIndex&, int first, int last, const QModelIndex&, int destination) {
      if (!end(Open::move, first, last, m_countBefore)) return;
      if (destination != m_moveDestination) violation("rowsMoved destination differs from rowsAboutToBeMoved");
      const int count = last - first + 1;
      std::vector<MirrorRow> block(m_rows.begin() + first, m_rows.begin() + last + 1);
      m_rows.erase(m_rows.begin() + first, m_rows.begin() + last + 1);
      const int at = destination > last ? destination - count : destination;
      m_rows.insert(m_rows.begin() + at, block.begin(), block.end());
    });
    connect(model, &M::modelAboutToBeReset, this, [this] { begin(Open::reset, 0, 0); });
    connect(model, &M::modelReset, this, [this] {
      if (m_open != Open::reset) violation("modelReset without modelAboutToBeReset");
      m_open = Open::none;
      m_rows = readAll();
      m_announcedCount = m_model->rowCount();
    });
    // A proxy that re-sorts after a value changed says so with layoutChanged; a view then reads every row again.
    connect(model, &M::layoutChanged, this, [this] {
      m_rows = readAll();
      m_announcedCount = m_model->rowCount();
    });
    connect(model, &M::dataChanged, this, [this](const QModelIndex& top, const QModelIndex& bottom) {
      for (int row = top.row(); row <= bottom.row() && row < static_cast<int>(m_rows.size()); ++row)
        m_rows[row] = read(row);
    });
  }

  // Call after every change: the change must be closed, and the mirror must equal the live model.
  void check() {
    if (m_open != Open::none) violation("a change was announced and never confirmed");
    if (m_model->rowCount() != m_announcedCount) violation("the row count changed without an announcement");
    const std::vector<MirrorRow> live = readAll();
    if (m_rows.size() != live.size()) {
      violation(QStringLiteral("a view would show %1 rows, the model has %2").arg(m_rows.size()).arg(live.size()));
    } else {
      for (std::size_t row = 0; row < live.size(); ++row) {
        if (m_rows[row] == live[row]) continue;
        violation(QStringLiteral("row %1: a view would show key %2 and %3, the model has key %4 and %5")
                      .arg(row).arg(m_rows[row].key.toString(), m_rows[row].display.toString(),
                                    live[row].key.toString(), live[row].display.toString()));
        break;
      }
    }
  }

  const QStringList& violations() const { return m_violations; }
  bool clean() const { return m_violations.isEmpty(); }

 private:
  enum class Open { none, insert, remove, move, reset };

  MirrorRow read(int row) const {
    const QModelIndex index = m_model->index(row, 0);
    return {m_keysAreStable ? m_model->data(index, kKeyRole) : QVariant(), m_model->data(index, Qt::DisplayRole)};
  }
  std::vector<MirrorRow> readAll() const {
    std::vector<MirrorRow> out;
    for (int row = 0; row < m_model->rowCount(); ++row) out.push_back(read(row));
    return out;
  }
  void violation(const QString& text) { m_violations.append(text); }
  void begin(Open kind, int first, int last) {
    if (m_open != Open::none) violation("a change was announced while another was still open");
    m_open = kind;
    m_first = first;
    m_last = last;
    m_countBefore = m_model->rowCount();
  }
  // Returns true when the confirmation matches the announcement and the row count is what it must be.
  bool end(Open kind, int first, int last, int expectedCount) {
    bool ok = true;
    if (m_open != kind) {
      violation("a change was confirmed that was not announced");
      ok = false;
    }
    if (first != m_first || last != m_last) {
      violation("the confirmation names other rows than the announcement");
      ok = false;
    }
    if (m_model->rowCount() != expectedCount) {
      violation("the row count after the change is not what the change implies");
      ok = false;
    }
    m_open = Open::none;
    m_announcedCount = m_model->rowCount();
    return ok;
  }

  QAbstractItemModel* m_model;
  bool m_keysAreStable;
  std::vector<MirrorRow> m_rows;
  QStringList m_violations;
  Open m_open = Open::none;
  int m_first = 0;
  int m_last = 0;
  int m_countBefore = 0;
  int m_moveDestination = 0;
  int m_announcedCount = 0;
};

// The expected rows, in model order, with an id per entry that never changes while it lives.
template <class C>
class Shadow {
 public:
  using Traits = observable::ContainerTraits<C>;
  using Key = typename Traits::Key;
  struct Row {
    Key key;
    int value;
    int id;
  };

  const std::vector<Row>& rows() const { return m_rows; }

  std::optional<int> find(const Key& key) const {
    if constexpr (Traits::order == observable::Order::indexed) {
      if (key >= 0 && key < static_cast<Key>(m_rows.size())) return static_cast<int>(key);
      return std::nullopt;
    } else {
      for (std::size_t i = 0; i < m_rows.size(); ++i)
        if (m_rows[i].key == key) return static_cast<int>(i);
      return std::nullopt;
    }
  }

  bool insert(const Key& key, int value) {
    if constexpr (Traits::order == observable::Order::indexed) {
      if (key < 0 || key > static_cast<Key>(m_rows.size())) return false;
      m_rows.insert(m_rows.begin() + key, Row{key, value, m_nextId++});
      renumber();
    } else {
      if (find(key)) return false;
      auto at = m_rows.end();
      if constexpr (Traits::order == observable::Order::sorted)
        at = std::lower_bound(m_rows.begin(), m_rows.end(), key, [](const Row& r, const Key& k) { return r.key < k; });
      m_rows.insert(at, Row{key, value, m_nextId++});
    }
    return true;
  }

  bool remove(const Key& key) {
    const auto row = find(key);
    if (!row) return false;
    m_rows.erase(m_rows.begin() + *row);
    renumber();
    return true;
  }

  bool replace(const Key& key, int value) {
    if constexpr (!Traits::assignable) {
      return false;
    } else {
      const auto row = find(key);
      if (!row) return false;
      m_rows[*row].value = value;
      return true;
    }
  }

  bool move(const Key& from, const Key& to) {
    if constexpr (!Traits::movable) {
      return false;
    } else {
      const auto source = find(from);
      const auto target = find(to);
      if (!source || !target || from == to) return false;
      const Row moved = m_rows[*source];
      m_rows.erase(m_rows.begin() + *source);
      m_rows.insert(m_rows.begin() + *target, moved);
      renumber();
      return true;
    }
  }

  bool clear() {
    if (m_rows.empty()) return false;
    m_rows.clear();
    return true;
  }

 private:
  void renumber() {
    if constexpr (Traits::order == observable::Order::indexed)
      for (std::size_t i = 0; i < m_rows.size(); ++i) m_rows[i].key = static_cast<Key>(i);
  }
  std::vector<Row> m_rows;
  int m_nextId = 0;
};

struct Report {
  int steps = 0;
  int applied = 0;           // changes the container accepted
  int refused = 0;           // invalid requests the container refused
  int mismatches = 0;        // the model, the container or the selection disagreed with the reference
  QStringList violations;    // from the ModelWatcher
  bool clean() const { return mismatches == 0 && violations.isEmpty(); }
};

// Applies `steps` random changes to a container, and after each one compares the container, the model, the
// view mirror, the selection and a persistent index with the reference. With `stop_at_first_violation` it
// returns as soon as the watcher reports something. With `with_proxy` a QSortFilterProxyModel sorted by key sits
// on the model: it re-reads the model from the same signals, so a wrong announcement leaves its rows out of
// order or out of step, and a second watcher mirrors it.
template <class C, class Source, class MakeModel>
Report run_random(unsigned seed, int steps, MakeModel&& make_model, bool stop_at_first_violation = false,
                  bool with_proxy = false) {
  using Traits = observable::ContainerTraits<C>;
  using Key = typename Traits::Key;

  Source source;
  std::unique_ptr<QAbstractItemModel> model = make_model(&source);
  ModelWatcher watcher(model.get(), Traits::order != observable::Order::indexed);
  QItemSelectionModel selection(model.get());
  std::unique_ptr<QSortFilterProxyModel> proxy;
  std::unique_ptr<ModelWatcher> proxyWatcher;
  if (with_proxy) {
    proxy = std::make_unique<QSortFilterProxyModel>();
    proxy->setSortRole(Traits::order == observable::Order::indexed ? Qt::DisplayRole : kKeyRole);
    proxy->setSourceModel(model.get());
    proxy->sort(0);
    proxyWatcher = std::make_unique<ModelWatcher>(proxy.get(), Traits::order != observable::Order::indexed);
  }
  QPersistentModelIndex tracked;
  std::optional<int> trackedId;
  QSet<int> selectedIds;
  Shadow<C> shadow;
  std::mt19937 rng(seed);
  Report report;
  int nextValue = 1;

  const auto draw = [&rng](int low, int high) { return std::uniform_int_distribution<int>(low, high - 1)(rng); };
  const auto random_key = [&]() -> Key {
    if constexpr (Traits::order == observable::Order::indexed)
      return static_cast<Key>(draw(-1, static_cast<int>(shadow.rows().size()) + kIndexSlack));
    else
      return static_cast<Key>(draw(0, kKeyDomain));
  };
  const auto expect = [&report](bool ok) {
    if (!ok) ++report.mismatches;
  };

  for (int step = 0; step < steps; ++step) {
    ++report.steps;
    const int kind = draw(0, kChangeKinds);
    const Key key = random_key();
    const int value = nextValue++;
    bool accepted = false;
    bool changed = true;

    if (kind < kWeightInsert) {
      const bool expected = shadow.insert(key, Traits::order == observable::Order::indexed || Traits::assignable ? value : static_cast<int>(key));
      accepted = source.insert(key, Make<C>::value(value, key));
      expect(accepted == expected);
    } else if (kind < kWeightInsert + kWeightRemove) {
      const auto row = shadow.find(key);
      const std::optional<int> id = row ? std::optional<int>(shadow.rows()[*row].id) : std::nullopt;
      const bool expected = shadow.remove(key);
      accepted = source.remove(key);
      expect(accepted == expected);
      if (accepted && id) selectedIds.remove(*id);
      if (accepted && id && trackedId == id) trackedId.reset();
    } else if (kind < kWeightInsert + kWeightRemove + kWeightReplace) {
      const bool expected = shadow.replace(key, value);
      accepted = source.replace(key, Make<C>::value(value, key));
      expect(accepted == expected);
    } else if (kind < kWeightInsert + kWeightRemove + kWeightReplace + kWeightMove) {
      const Key to = random_key();
      const bool expected = shadow.move(key, to);
      accepted = source.move(key, to);
      expect(accepted == expected);
    } else if (kind < kWeightInsert + kWeightRemove + kWeightReplace + kWeightMove + kWeightSelect) {
      changed = false;
      if (!shadow.rows().empty()) {
        const int row = draw(0, static_cast<int>(shadow.rows().size()));
        const int id = shadow.rows()[row].id;
        selection.select(model->index(row, 0), QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
        if (selectedIds.contains(id)) selectedIds.remove(id); else selectedIds.insert(id);
        tracked = model->index(row, 0);
        trackedId = id;
      }
    } else {
      const bool expected = shadow.clear();
      accepted = source.clear();
      expect(accepted == expected);
      if (accepted) {
        selectedIds.clear();
        trackedId.reset();
      }
    }
    if (changed) ++(accepted ? report.applied : report.refused);

    watcher.check();
    // A wrong adapter can leave Qt's own bookkeeping unbalanced, so a control run stops where the fault shows.
    if (stop_at_first_violation && !watcher.clean()) break;

    if (proxy) {
      proxyWatcher->check();
      if (stop_at_first_violation && !proxyWatcher->clean()) break;
      // The proxy lists the reference's rows ordered by key, and shows their values.
      std::vector<typename Shadow<C>::Row> sorted = shadow.rows();
      if constexpr (Traits::order == observable::Order::indexed)
        std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.value < b.value; });
      else
        std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.key < b.key; });
      expect(proxy->rowCount() == static_cast<int>(sorted.size()));
      for (int row = 0; row < proxy->rowCount() && row < static_cast<int>(sorted.size()); ++row) {
        expect(proxy->index(row, 0).data(Qt::DisplayRole).toInt() == sorted[static_cast<std::size_t>(row)].value);
        if constexpr (Traits::order != observable::Order::indexed)
          expect(proxy->index(row, 0).data(kKeyRole).template value<Key>() == sorted[static_cast<std::size_t>(row)].key);
      }
    }

    // The container holds what the reference holds.
    expect(source.size() == static_cast<qsizetype>(shadow.rows().size()));
    // The model lists what the reference lists, in the same order.
    expect(model->rowCount() == static_cast<int>(shadow.rows().size()));
    for (int row = 0; row < model->rowCount() && row < static_cast<int>(shadow.rows().size()); ++row) {
      const auto& expected = shadow.rows()[row];
      const QModelIndex index = model->index(row, 0);
      if constexpr (Traits::order == observable::Order::indexed)
        expect(!model->data(index, kKeyRole).isValid());  // a list offers no key role
      else
        expect(model->data(index, kKeyRole).template value<Key>() == expected.key);
      expect(model->data(index, Qt::DisplayRole).toInt() == expected.value);
    }
    // A list has no children: a valid parent has no rows.
    if (model->rowCount() > 0) expect(model->rowCount(model->index(0, 0)) == 0);
    // The selection follows its entries.
    QSet<int> selectedRows;
    for (const QModelIndex& index : selection.selectedRows()) selectedRows.insert(index.row());
    QSet<int> expectedRows;
    for (int row = 0; row < static_cast<int>(shadow.rows().size()); ++row)
      if (selectedIds.contains(shadow.rows()[row].id)) expectedRows.insert(row);
    expect(selectedRows == expectedRows);
    // A persistent index follows its entry, and is invalid once the entry is gone.
    if (trackedId) {
      const auto row = std::find_if(shadow.rows().begin(), shadow.rows().end(),
                                    [&](const typename Shadow<C>::Row& r) { return r.id == *trackedId; });
      expect(row != shadow.rows().end() && tracked.isValid() && tracked.row() == row - shadow.rows().begin());
    } else {
      expect(!tracked.isValid());
    }
  }
  report.violations = watcher.violations();
  if (proxyWatcher) report.violations += proxyWatcher->violations();
  return report;
}

}  // namespace observable_test
