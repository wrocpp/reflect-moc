// Model protocol, a known gap in a table without columns (docs/qt-edge-cases/protocol.md, P-67). A change to a value
// is announced with dataChanged(index(row, 0), index(row, columnCount - 1)); with no column both ends are invalid
// indexes, and QAbstractItemModelTester reports "topLeft.isValid()". The test passes while the gap exists.
#include "protocol_support.hpp"

namespace {
using namespace edge;

template <class C>
void zero_column_change(const std::string& kind) {
  using Fx = Fixture<C, true>;
  observable::ObservableContainer<C> source;
  observable::ContainerTableModel<C> table(&source, std::vector<observable::Column<C>>{});
  QAbstractItemModel& model = table;
  for (int n = 1; n <= 2; ++n) {
    const typename Fx::Key key = [&] {
      if constexpr (Fx::indexed)
        return static_cast<typename Fx::Key>(source.size());
      else
        return Fx::named(n);
    }();
    source.insert(key, Make<C>::value(n, key));
  }
  SignalLog log(&model);
  if constexpr (Fx::assignable) {
    const typename Fx::Key first = [&] {
      if constexpr (Fx::indexed)
        return static_cast<typename Fx::Key>(0);
      else
        return Fx::named(1);
    }();
    KCHECK(source.replace(first, Fx::from_int(kEdited)));
    KCHECK(source.editValue(first, [](auto& v) { v = Fx::from_int(kEdited + 1); }, QList<int>{Qt::UserRole + 1}));
    KCHECK(log.changes().size() == 2);
    for (const SignalLog::Change& change : log.changes()) KCHECK(!change.top.isValid() && !change.bottom.isValid());  // the gap
  }
}

// P-67
void limit_zero_columns_data_changed_has_invalid_indexes() {
  zero_column_change<QList<Sample>>("QList");
  zero_column_change<QMap<int, Sample>>("QMap");
  zero_column_change<QHash<unsigned, Sample>>("QHash");
  zero_column_change<QMultiMap<int, Sample>>("QMultiMap");

  observable::ObservableContainer<QList<Sample>> source;
  observable::ContainerTableModel<QList<Sample>> table(&source, std::vector<observable::Column<QList<Sample>>>{});
  source.insert(0, Sample{1});
  MessageCatcher caught;
  QAbstractItemModelTester tester(&table, QAbstractItemModelTester::FailureReportingMode::Warning);
  const std::string kind = "QList";
  KCHECK(caught.messages().isEmpty());
  source.replace(0, Sample{2});
  KCHECK(caught.saw("topLeft.isValid()"));  // what the tester says about it
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  limit_zero_columns_data_changed_has_invalid_indexes();
  return rqt_test::finish("limit_edge_protocol_shapes");
}
