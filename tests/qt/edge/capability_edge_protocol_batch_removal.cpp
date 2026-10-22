// Model protocol, fixed gap P-25: a batch removal (removeMany, takeMany) erases the entries before the model announces
// its runs, yet Qt expects the data of a row to be readable at rowsAboutToBeRemoved. The model keeps a snapshot of
// the doomed (key, value) pairs while the runs are announced, so every receiver reads the real value in every run,
// and no snapshot value is served afterwards. Catalogue row: docs/qt-edge-cases/protocol.md (P-25).
#include "protocol_support.hpp"

namespace {
using namespace edge;

constexpr int kBatchRows = 10;

// Reads rows 0..last at every rowsAboutToBeRemoved and compares them with what the model showed before the batch.
class RunReader : public QObject {
 public:
  RunReader(QAbstractItemModel* model, std::vector<int> before) : QObject(model), m_before(std::move(before)) {
    connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this, model](const QModelIndex&, int first, int last) {
      ++m_runs;
      // the runs go from the bottom up, so rows 0..last still stand where they stood before the batch
      for (int row = 0; row <= last; ++row)
        if (model->data(model->index(row, 0)).toInt() != m_before[static_cast<std::size_t>(row)] || !model->data(model->index(row, 0)).isValid())
          ++m_wrong;
      m_doomedRead += last - first + 1;
    });
  }
  int runs() const { return m_runs; }
  int wrong() const { return m_wrong; }
  int doomed_read() const { return m_doomedRead; }

 private:
  std::vector<int> m_before;
  int m_runs = 0;
  int m_wrong = 0;
  int m_doomedRead = 0;
};

// P-25
void rows_of_a_batch_removal_read_their_values_while_it_is_announced() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        if constexpr (Fx::keyed) {
          RowProbe probe(fx.model.get());
          KCHECK(fx.remove(1));  // a single remove was always announced before it erased
          RunReader reader(fx.model.get(), fx.shown());
          KCHECK(fx.source.removeMany({Fx::named(2), Fx::named(3), Fx::named(5), Fx::named(8), Fx::named(9), Fx::named(kUnknownRole)}) == 5);
          KCHECK(reader.runs() >= 2 && reader.doomed_read() == 5 && reader.wrong() == 0);  // scattered rows, several runs, all readable
          KCHECK(probe.blank_rows_being_removed() == 0 && probe.disagreements() == 0);
          RunReader taker(fx.model.get(), fx.shown());
          KCHECK(fx.source.takeMany({Fx::named(4), Fx::named(6), Fx::named(7)}).size() == 3);
          KCHECK(taker.runs() >= 1 && taker.doomed_read() == 3 && taker.wrong() == 0);
          KCHECK(probe.blank_rows_being_removed() == 0 && probe.disagreements() == 0);
          KCHECK(fx.rows() == 1 && fx.source.size() == 1);
        }
        if constexpr (Fx::indexed) {
          RowProbe probe(fx.model.get());
          KCHECK(fx.source.removeRange(1, 2));  // a range was always announced before it erased
          KCHECK(probe.blank_rows_being_removed() == 0);
        }
      },
      false, kBatchRows);
}

// P-25: the snapshot is gone after the batch, and a key that returns with another value shows that value
void no_snapshot_value_is_served_after_the_batch() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        if constexpr (Fx::keyed && Fx::assignable) {
          KCHECK(fx.source.removeMany({Fx::named(2), Fx::named(3)}) == 2);
          KCHECK(fx.source.insert(Fx::named(2), Fx::from_int(kEdited)));  // the key is back with another value
          const std::vector<int> back = fx.shown();
          KCHECK(std::count(back.begin(), back.end(), kEdited) == 1 && std::count(back.begin(), back.end(), 2) == 0);
          RunReader reader(fx.model.get(), back);
          KCHECK(fx.source.removeMany({Fx::named(2), Fx::named(1)}) == 2);  // doomed again: its new value is read, not the old one
          KCHECK(reader.runs() >= 1 && reader.doomed_read() == 2 && reader.wrong() == 0);
          const std::vector<int> after = fx.shown();
          KCHECK(std::count(after.begin(), after.end(), kEdited) == 0 && fx.rows() == kBatchRows - 3);
          for (int row = 0; row < fx.rows(); ++row) KCHECK(fx.model->data(fx.index(row)).isValid());
        }
      },
      false, kBatchRows);
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  rows_of_a_batch_removal_read_their_values_while_it_is_announced();
  no_snapshot_value_is_served_after_the_batch();
  return rqt_test::finish("capability_edge_protocol_batch_removal");
}
