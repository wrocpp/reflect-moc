// Model protocol, part 7: the shape of a table model (headers, zero columns, one column) and the defaults a model
// inherits and does not override: match(), sort(), the view hooks, and the structural editing calls.
// QAbstractItemModelTester runs in fatal mode in every fixture. Catalogue rows: docs/qt-edge-cases/protocol.md
// (P-58 to P-66).
#include "protocol_support.hpp"

#include <climits>

namespace {
using namespace edge;

constexpr Qt::ItemDataRole kHeaderRolesOtherThanDisplay[] = {
    Qt::EditRole, Qt::ToolTipRole, Qt::TextAlignmentRole, Qt::DecorationRole, Qt::FontRole, Qt::UserRole, Qt::SizeHintRole,
};

int row_of(const std::vector<int>& rows, int value) {
  return static_cast<int>(std::find(rows.begin(), rows.end(), value) - rows.begin());
}

// P-58
void header_data_of_a_table_model() {
  for_each_model([](auto& fx, const std::string& kind) {
    using Fx = std::remove_cvref_t<decltype(fx)>;
    if constexpr (Fx::table) {
      SignalLog log(fx.model.get());
      KCHECK(fx.model->headerData(0, Qt::Horizontal).toString() == "value");
      KCHECK(fx.model->headerData(1, Qt::Horizontal, Qt::DisplayRole).toString() == "twin");
      for (const int section : {-1, kTableColumns, kTableColumns + 1, INT_MAX, INT_MIN})
        KCHECK(!fx.model->headerData(section, Qt::Horizontal).isValid());
      for (const int section : {-1, 0, 1, kSmallFill, INT_MAX}) KCHECK(!fx.model->headerData(section, Qt::Vertical).isValid());  // no row numbers
      for (const Qt::ItemDataRole role : kHeaderRolesOtherThanDisplay) {
        KCHECK(!fx.model->headerData(0, Qt::Horizontal, role).isValid());
        KCHECK(!fx.model->headerData(1, Qt::Horizontal, role).isValid());
      }
      fx.fill(kSmallFill);
      fx.source.clear();
      KCHECK(fx.model->headerData(0, Qt::Horizontal).toString() == "value");  // rows come and go, the columns stay
      KCHECK(log.count("headerDataChanged") == 0);
      KCHECK(!fx.model->setHeaderData(0, Qt::Horizontal, "renamed") && fx.model->headerData(0, Qt::Horizontal).toString() == "value");
    }
  });
}

// P-59: a list model does not override headerData, so Qt's default answers
void header_data_of_a_list_model_is_qts_default() {
  for_each_model([](auto& fx, const std::string& kind) {
    using Fx = std::remove_cvref_t<decltype(fx)>;
    if constexpr (!Fx::table) {
      for (int section = 0; section < kSmallFill; ++section) {
        KCHECK(fx.model->headerData(section, Qt::Horizontal).toInt() == section + 1);
        KCHECK(fx.model->headerData(section, Qt::Vertical).toInt() == section + 1);
      }
      for (const Qt::ItemDataRole role : kHeaderRolesOtherThanDisplay) KCHECK(!fx.model->headerData(0, Qt::Horizontal, role).isValid());
    }
  });
}

// What an insert, a remove, a move, a clear and a reset do to a table that has no column at all, and to one that
// has a single column. A replace is left out of the first: see limit_edge_protocol_shapes.
template <class C>
void zero_columns(const std::string& kind) {
  using Fx = Fixture<C, true>;
  observable::ObservableContainer<C> source;
  observable::ContainerTableModel<C> table(&source, std::vector<observable::Column<C>>{});
  QAbstractItemModel& model = table;
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  SignalLog log(&model);
  const auto add = [&](int n) {
    const typename Fx::Key key = [&] {
      if constexpr (Fx::indexed)
        return static_cast<typename Fx::Key>(source.size());
      else
        return Fx::named(n);
    }();
    return source.insert(key, Make<C>::value(n, key));
  };
  KCHECK(model.columnCount() == 0 && model.rowCount() == 0 && !model.hasChildren());
  KCHECK(add(1) && add(2) && add(3));
  KCHECK(log.count("rowsInserted") == 3);
  KCHECK(model.columnCount() == 0 && model.rowCount() == 3);       // the rows are there, with nothing in them
  KCHECK(!model.index(0, 0).isValid() && !model.hasIndex(0, 0));
  KCHECK(!model.hasChildren());                                    // Qt's default needs a row and a column
  KCHECK(!model.data(model.index(0, 0)).isValid() && !model.headerData(0, Qt::Horizontal).isValid());
  KCHECK(source.remove(Fx::indexed ? typename Fx::Key{1} : Fx::named(2)));
  KCHECK(model.rowCount() == 2);
  if constexpr (Fx::indexed) {
    KCHECK(source.insertRange(0, {Sample{7}, Sample{8}}) && source.removeRange(1, 2) && source.move(0, 1));
  }
  if constexpr (Fx::keyed) {
    KCHECK(source.insertMany(Fx::content({7, 8})) == 2 && source.removeMany({Fx::named(7)}) == 1);
  }
  KCHECK(log.count("columns") == 0);
  source.resetWith(Fx::content({4, 5}));
  KCHECK(model.rowCount() == 2);
  KCHECK(source.clear() && model.rowCount() == 0);
}

// P-60
void table_with_zero_columns() {
  zero_columns<QList<Sample>>("QList");
  zero_columns<QMap<int, Sample>>("QMap");
  zero_columns<QHash<unsigned, Sample>>("QHash");
  zero_columns<QSet<unsigned>>("QSet");
}

template <class C>
void one_column(const std::string& kind) {
  using Fx = Fixture<C, true>;
  using Key = typename Fx::Key;
  using Value = typename Fx::Value;
  observable::ObservableContainer<C> source;
  observable::ContainerTableModel<C> table(
      &source, std::vector<observable::Column<C>>{{.header = "only",
                                                   .display = [](const Key& key, const Value& value) { return QVariant(Make<C>::shown(key, value)); },
                                                   .edit = [](const Value&, const QVariant& v) -> std::optional<Value> {
                                                     bool ok = false;
                                                     const int n = v.toInt(&ok);
                                                     return ok ? std::optional<Value>(Fx::from_int(n)) : std::nullopt;
                                                   }}});
  QAbstractItemModel& model = table;
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
  SignalLog log(&model);
  for (int n = 1; n <= kSmallFill; ++n) {
    const Key key = [&] {
      if constexpr (Fx::indexed)
        return static_cast<Key>(source.size());
      else
        return Fx::named(n);
    }();
    KCHECK(source.insert(key, Make<C>::value(n, key)));
  }
  KCHECK(model.columnCount() == 1 && model.rowCount() == kSmallFill && model.hasChildren());
  KCHECK(model.index(0, 0).isValid() && !model.index(0, 1).isValid() && !model.hasIndex(0, 1));
  KCHECK(model.headerData(0, Qt::Horizontal).toString() == "only" && !model.headerData(1, Qt::Horizontal).isValid());
  log.take();
  if constexpr (Fx::assignable) {
    KCHECK(model.setData(model.index(1, 0), kEdited));
    KCHECK(log.take() == QStringList({"dataChanged 1,0 1,0 roles="}));  // the row is one cell wide
  }
  KCHECK(model.flags(model.index(0, 0)).testFlag(Qt::ItemIsEditable) == Fx::assignable);
}

// P-61
void table_with_one_column() {
  one_column<QList<Sample>>("QList");
  one_column<QMap<int, Sample>>("QMap");
  one_column<QHash<unsigned, Sample>>("QHash");
  one_column<QMultiMap<int, Sample>>("QMultiMap");
}

// P-62
void match_with_qts_defaults() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        const std::vector<int> rows = fx.shown();
        const int column = fx.model->columnCount() - 1;
        const int factor = fx.table ? kTwinFactor : 1;  // the last column of a table shows twice the number
        const QModelIndex start = fx.index(0, column);
        const int three = row_of(rows, 3);
        auto hits = fx.model->match(start, Qt::DisplayRole, factor * 3, 1, Qt::MatchExactly);
        KCHECK(hits.size() == 1 && hits[0].row() == three && hits[0].column() == column);
        KCHECK(fx.model->match(start, Qt::DisplayRole, factor * 3, -1, Qt::MatchExactly).size() == 1);
        KCHECK(fx.model->match(start, Qt::DisplayRole, kUnknownRole, -1, Qt::MatchExactly).isEmpty());
        KCHECK(fx.model->match(start, Qt::DisplayRole, QString::number(factor * 3)).size() == 1);  // Qt's default: starts with, wraps
        KCHECK(fx.model->match(QModelIndex(), Qt::DisplayRole, 3).isEmpty());
        KCHECK(fx.model->match(start, Qt::EditRole, factor * 3, -1, Qt::MatchExactly).isEmpty());  // the model does not serve it
        const int after = (three + 1) % kSmallFill;
        if (after > three) KCHECK(fx.model->match(fx.index(after, column), Qt::DisplayRole, factor * 3, 1, Qt::MatchExactly).isEmpty());
        hits = fx.model->match(fx.index(after, column), Qt::DisplayRole, factor * 3, 1, Qt::MatchExactly | Qt::MatchWrap);
        KCHECK(hits.size() == 1 && hits[0].row() == three);
      },
      false, kSmallFill);
}

// P-63
void match_by_the_key_role() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        const std::vector<int> rows = fx.shown();
        const auto hits = fx.model->match(fx.index(0), Qt::UserRole, QVariant::fromValue(Fx::named(3)), -1, Qt::MatchExactly);
        if constexpr (Fx::keyed)
          KCHECK(hits.size() == 1 && hits[0].row() == row_of(rows, 3));
        else
          KCHECK(hits.isEmpty());  // a list row and a multi occurrence offer no key
      },
      false, kSmallFill);
}

// P-64
void sort_is_qts_default_and_does_nothing() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        const std::vector<int> before = fx.shown();
        SignalLog log(fx.model.get());
        fx.model->sort(0, Qt::DescendingOrder);
        fx.model->sort(0, Qt::AscendingOrder);
        fx.model->sort(-1);
        fx.model->sort(fx.model->columnCount());
        KCHECK(log.silent() && fx.shown() == before);
      },
      false, kSmallFill);
}

// P-65
void view_hooks_are_qts_defaults() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        const std::vector<int> before = fx.shown();
        SignalLog log(fx.model.get());
        KCHECK(!fx.model->canFetchMore(QModelIndex()));
        fx.model->fetchMore(QModelIndex());
        KCHECK(fx.model->submit());
        fx.model->revert();
        for (int row = 0; row < fx.rows(); ++row) {
          KCHECK(fx.model->buddy(fx.index(row)) == fx.index(row));
          KCHECK(fx.model->span(fx.index(row)) == QSize(1, 1));
        }
        KCHECK(log.silent() && fx.shown() == before);
      },
      false, kSmallFill);
}

// P-66
void structural_editing_calls_are_refused() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        const std::vector<int> before = fx.shown();
        SignalLog log(fx.model.get());
        KCHECK(!fx.model->insertRows(0, 1) && !fx.model->insertRows(fx.rows(), 1) && !fx.model->insertRow(0));
        KCHECK(!fx.model->removeRows(0, 1) && !fx.model->removeRows(0, fx.rows()) && !fx.model->removeRow(fx.rows() - 1));
        KCHECK(!fx.model->moveRows(QModelIndex(), 0, 1, QModelIndex(), 3) && !fx.model->moveRow(QModelIndex(), 0, QModelIndex(), 3));
        KCHECK(!fx.model->insertColumns(0, 1) && !fx.model->removeColumns(0, 1) && !fx.model->moveColumns(QModelIndex(), 0, 1, QModelIndex(), 1));
        KCHECK(!fx.model->setHeaderData(0, Qt::Horizontal, QStringLiteral("renamed")));
        KCHECK(log.silent() && fx.shown() == before && fx.rows() == kSmallFill);
      },
      true, kSmallFill);
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  header_data_of_a_table_model();
  header_data_of_a_list_model_is_qts_default();
  table_with_zero_columns();
  table_with_one_column();
  match_with_qts_defaults();
  match_by_the_key_role();
  sort_is_qts_default_and_does_nothing();
  view_hooks_are_qts_defaults();
  structural_editing_calls_are_refused();
  return rqt_test::finish("capability_edge_protocol_shapes");
}
