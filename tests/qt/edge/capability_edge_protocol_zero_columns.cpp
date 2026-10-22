// Model protocol, fixed gap P-67: a table with no column announces no dataChanged for a changed value, since its row
// has no cell to name (dataChanged needs valid indexes), and QAbstractItemModelTester in fatal mode stays quiet.
// Catalogue row: docs/qt-edge-cases/protocol.md (P-67).
#include "protocol_support.hpp"

namespace {
using namespace edge;

template <class C>
void zero_column_change(const std::string& kind) {
  using Fx = Fixture<C, true>;
  observable::ObservableContainer<C> source;
  observable::ContainerTableModel<C> table(&source, std::vector<observable::Column<C>>{});
  QAbstractItemModel& model = table;
  const auto key_of = [&](int n) {
    if constexpr (Fx::indexed)
      return static_cast<typename Fx::Key>(source.size());
    else
      return Fx::named(n);
  };
  for (int n = 1; n <= 2; ++n) source.insert(key_of(n), Make<C>::value(n, key_of(n)));
  QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
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
    KCHECK(Make<C>::shown(first, source.value(first)) == kEdited + 1);  // the container changed
    KCHECK(log.silent());                                               // the model announced nothing
  }
  KCHECK(model.rowCount() == 2 && model.columnCount() == 0);
}

// P-67
void zero_columns_announce_no_data_changed() {
  zero_column_change<QList<Sample>>("QList");
  zero_column_change<QMap<int, Sample>>("QMap");
  zero_column_change<QHash<unsigned, Sample>>("QHash");
  zero_column_change<QMultiMap<int, Sample>>("QMultiMap");
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  zero_columns_announce_no_data_changed();
  return rqt_test::finish("capability_edge_protocol_zero_columns");
}
