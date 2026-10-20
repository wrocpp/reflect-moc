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
    connect(source, &Source::entryChanged, this, [this](const qsizetype& key) {
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
  }

  int rowCount(const QModelIndex& parent = QModelIndex()) const override {
    return parent.isValid() ? 0 : static_cast<int>(m_source->size());
  }
  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
    if (!index.isValid() || index.row() >= rowCount()) return {};
    if (role == kKeyRole) return QVariant::fromValue(static_cast<qsizetype>(index.row()));
    if (role == Qt::DisplayRole) return m_source->value(index.row()).value;
    return {};
  }

 private:
  Source* m_source;
  Flaw m_flaw;
};

}  // namespace observable_test
