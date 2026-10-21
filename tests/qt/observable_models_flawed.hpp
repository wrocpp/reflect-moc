// Adapters with one deliberate mistake each, the kind that produces ghost rows when the wiring between a
// container and a view is written by hand. They exist so that the checks in observable_models_support.hpp
// are shown to fail on a wrong model, and not only to pass on a right one.
#pragma once

#include "observable_models_support.hpp"

#include <container_model.hpp>

#include <QtCore/QAbstractListModel>

namespace observable_test {

enum class Flaw {
  none,
  forgot_end_insert,     // beginInsertRows with no endInsertRows
  late_announce_insert,  // the insert is announced after the container has changed
  wrong_row_remove,      // the removal is announced for the next row
  silent_replace,        // a value changes and no dataChanged follows
  forgot_end_move,       // beginMoveRows with no endMoveRows
  forgot_end_reset,      // beginResetModel with no endResetModel
};

inline const char* name_of(Flaw flaw) {
  switch (flaw) {
    case Flaw::none: return "none";
    case Flaw::forgot_end_insert: return "forgot_end_insert";
    case Flaw::late_announce_insert: return "late_announce_insert";
    case Flaw::wrong_row_remove: return "wrong_row_remove";
    case Flaw::silent_replace: return "silent_replace";
    case Flaw::forgot_end_move: return "forgot_end_move";
    case Flaw::forgot_end_reset: return "forgot_end_reset";
  }
  return "?";
}

class FlawedListModel : public QAbstractListModel {
  RQT_OBJECT

 public:
  using Source = observable::ObservableContainer<QList<Sample>>;

  FlawedListModel(Source* source, Flaw flaw) : m_source(source), m_flaw(flaw) {
    connect(source, &Source::entryAboutToBeAdded, this, [this](const qsizetype& key) {
      if (m_flaw == Flaw::late_announce_insert) return;
      beginInsertRows(QModelIndex(), static_cast<int>(key), static_cast<int>(key));
    });
    connect(source, &Source::entryAdded, this, [this](const qsizetype& key) {
      if (m_flaw == Flaw::late_announce_insert) beginInsertRows(QModelIndex(), static_cast<int>(key), static_cast<int>(key));
      if (m_flaw == Flaw::forgot_end_insert) return;
      endInsertRows();
    });
    connect(source, &Source::entryAboutToBeRemoved, this, [this](const qsizetype& key) {
      const int count = static_cast<int>(m_source->size());
      const int row = m_flaw == Flaw::wrong_row_remove && count > 1 ? static_cast<int>((key + 1) % count) : static_cast<int>(key);
      beginRemoveRows(QModelIndex(), row, row);
    });
    connect(source, &Source::entryRemoved, this, [this](const qsizetype&) { endRemoveRows(); });
    connect(source, &Source::entryChanged, this, [this](const qsizetype& key, const QList<int>&) {
      if (m_flaw == Flaw::silent_replace) return;
      emit dataChanged(index(static_cast<int>(key)), index(static_cast<int>(key)));
    });
    connect(source, &Source::entryAboutToBeMoved, this, [this](const qsizetype& from, const qsizetype& to) {
      const int source_row = static_cast<int>(from);
      const int target = static_cast<int>(to);
      beginMoveRows(QModelIndex(), source_row, source_row, QModelIndex(), target > source_row ? target + 1 : target);
    });
    connect(source, &Source::entryMoved, this, [this](const qsizetype&, const qsizetype&) {
      if (m_flaw == Flaw::forgot_end_move) return;
      endMoveRows();
    });
    connect(source, &Source::aboutToBeCleared, this, [this] { beginResetModel(); });
    connect(source, &Source::cleared, this, [this] {
      if (m_flaw == Flaw::forgot_end_reset) return;
      endResetModel();
    });
    connect(source, &Source::aboutToBeReset, this, [this] { beginResetModel(); });
    connect(source, &Source::resetCompleted, this, [this] {
      if (m_flaw == Flaw::forgot_end_reset) return;
      endResetModel();
    });
  }

  int rowCount(const QModelIndex& parent = QModelIndex()) const override {
    return parent.isValid() ? 0 : static_cast<int>(m_source->size());
  }
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
    if (!index.isValid() || index.row() >= rowCount()) return {};
    if (role == Qt::DisplayRole) return m_source->value(index.row()).value;
    return {};
  }

 private:
  Source* m_source;
  Flaw m_flaw;
};

// Batches have their own ways to go wrong, and each has a case here: runs removed from the top so the rows of
// the runs still to do have moved, a batch of scattered rows announced as one span, a reset that keeps the old rows,
// and a row count read from the container, which already holds the final content while a batch is announced.
enum class BatchFlaw {
  none,
  removal_ascending,    // runs of a batch removal go from the top, so the rows of the later runs are stale
  insertion_one_span,   // a batch insertion is announced as one span from its first to its last new row
  reset_keeps_rows,     // a reset ends without the model having read the new content
  count_from_container  // rowCount asks the container, which differs from the model's rows during a batch
};

inline const char* name_of(BatchFlaw flaw) {
  switch (flaw) {
    case BatchFlaw::none: return "none";
    case BatchFlaw::removal_ascending: return "removal_ascending";
    case BatchFlaw::insertion_one_span: return "insertion_one_span";
    case BatchFlaw::reset_keeps_rows: return "reset_keeps_rows";
    case BatchFlaw::count_from_container: return "count_from_container";
  }
  return "?";
}

// A model of a map written by hand, with a list of its own rows and one mistake at a time.
class FlawedMapModel : public QAbstractListModel {
  RQT_OBJECT

 public:
  using Source = observable::ObservableContainer<QMap<int, Sample>>;

  FlawedMapModel(Source* source, BatchFlaw flaw) : m_source(source), m_flaw(flaw) {
    read();
    connect(source, &Source::entryAboutToBeAdded, this, [this](const int& key) {
      m_pending = row_to_insert(key);
      beginInsertRows(QModelIndex(), m_pending, m_pending);
    });
    connect(source, &Source::entryAdded, this, [this](const int& key) {
      m_rows.insert(m_rows.begin() + m_pending, key);
      endInsertRows();
    });
    connect(source, &Source::entryAboutToBeRemoved, this, [this](const int& key) {
      m_pending = row_of(key);
      beginRemoveRows(QModelIndex(), m_pending, m_pending);
    });
    connect(source, &Source::entryRemoved, this, [this](const int&) {
      m_rows.erase(m_rows.begin() + m_pending);
      endRemoveRows();
    });
    connect(source, &Source::entryChanged, this, [this](const int& key, const QList<int>& roles) {
      emit dataChanged(index(row_of(key)), index(row_of(key)), roles);
    });
    connect(source, &Source::aboutToBeCleared, this, [this] { beginResetModel(); });
    connect(source, &Source::cleared, this, [this] {
      m_rows.clear();
      endResetModel();
    });
    connect(source, &Source::aboutToBeReset, this, [this] { beginResetModel(); });
    connect(source, &Source::resetCompleted, this, [this] {
      if (m_flaw != BatchFlaw::reset_keeps_rows) read();
      endResetModel();
    });
    connect(source, &Source::entriesAdded, this, [this](const QList<int>& keys) { insert_batch(keys); });
    connect(source, &Source::entriesRemoved, this, [this](const QList<int>& keys) { remove_batch(keys); });
  }

  int rowCount(const QModelIndex& parent = QModelIndex()) const override {
    if (parent.isValid()) return 0;
    return m_flaw == BatchFlaw::count_from_container ? static_cast<int>(m_source->size()) : static_cast<int>(m_rows.size());
  }
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
    if (!index.isValid() || index.row() >= static_cast<int>(m_rows.size())) return {};
    const int key = m_rows[static_cast<std::size_t>(index.row())];
    if (role == kKeyRole) return key;
    if (role == Qt::DisplayRole && m_source->contains(key)) return m_source->value(key).value;
    return {};
  }

 private:
  void read() {
    m_rows.clear();
    for (const int key : m_source->items().keys()) m_rows.push_back(key);
  }
  int row_to_insert(int key) const { return static_cast<int>(std::lower_bound(m_rows.begin(), m_rows.end(), key) - m_rows.begin()); }
  int row_of(int key) const { return row_to_insert(key); }

  void insert_batch(const QList<int>& keys) {  // ascending: a map announces its keys in key order
    std::vector<int> rows;
    for (qsizetype i = 0; i < keys.size(); ++i) rows.push_back(row_to_insert(keys[i]) + static_cast<int>(i));
    if (m_flaw == BatchFlaw::insertion_one_span) {
      beginInsertRows(QModelIndex(), rows.front(), rows.back());
      for (std::size_t k = 0; k < rows.size(); ++k) m_rows.insert(m_rows.begin() + rows[k], keys[static_cast<qsizetype>(k)]);
      endInsertRows();
      return;
    }
    for (std::size_t from = 0; from < rows.size();) {
      std::size_t to = from;
      while (to + 1 < rows.size() && rows[to + 1] == rows[to] + 1) ++to;
      beginInsertRows(QModelIndex(), rows[from], rows[to]);
      for (std::size_t k = from; k <= to; ++k) m_rows.insert(m_rows.begin() + rows[k], keys[static_cast<qsizetype>(k)]);
      endInsertRows();
      from = to + 1;
    }
  }

  void remove_batch(const QList<int>& keys) {
    std::vector<int> rows;
    for (const int key : keys) rows.push_back(row_of(key));
    std::sort(rows.begin(), rows.end());
    std::vector<std::pair<int, int>> runs;
    for (const int row : rows) {
      if (!runs.empty() && runs.back().second + 1 == row) ++runs.back().second;
      else runs.emplace_back(row, row);
    }
    if (m_flaw != BatchFlaw::removal_ascending) std::reverse(runs.begin(), runs.end());
    for (const auto& [first, last] : runs) {
      beginRemoveRows(QModelIndex(), first, last);
      m_rows.erase(m_rows.begin() + first, m_rows.begin() + last + 1);
      endRemoveRows();
    }
  }

  Source* m_source;
  BatchFlaw m_flaw;
  std::vector<int> m_rows;
  int m_pending = 0;
};

}  // namespace observable_test
