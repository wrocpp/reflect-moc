// Model protocol, a known gap in a batch removal (docs/qt-edge-cases/protocol.md, P-25). removeMany and takeMany
// erase the entries first and the model begins its removal at the "done" signal, so at rowsAboutToBeRemoved the rows
// that are about to go read as empty, where beginRemoveRows promises the data is still there. A single remove and
// removeRange announce before they erase and read fine. The test passes while the gap exists; the fix flips it.
#include "protocol_support.hpp"

namespace {
using namespace edge;

// P-25
void limit_rows_of_a_batch_removal_read_as_empty_while_it_is_announced() {
  for_each_model(
      [](auto& fx, const std::string& kind) {
        using Fx = std::remove_cvref_t<decltype(fx)>;
        if constexpr (Fx::keyed) {
          RowProbe probe(fx.model.get());
          KCHECK(fx.remove(1));                                          // one entry: announced before it is erased
          KCHECK(probe.blank_rows_being_removed() == 0);
          KCHECK(fx.source.removeMany({Fx::named(2), Fx::named(3)}) == 2);
          KCHECK(probe.blank_rows_being_removed() > 0);                  // the gap: Qt expects to read them here
          const int after_remove_many = probe.blank_rows_being_removed();
          KCHECK(!fx.source.takeMany({Fx::named(4)}).isEmpty());
          KCHECK(probe.blank_rows_being_removed() > after_remove_many);  // the same for takeMany
          KCHECK(probe.disagreements() == 0);                            // the counts themselves are right
        }
        if constexpr (Fx::indexed) {
          RowProbe probe(fx.model.get());
          KCHECK(fx.source.removeRange(1, 2));                           // a range is announced before it is erased
          KCHECK(probe.blank_rows_being_removed() == 0);
        }
      },
      false, kSmallFill);
}

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  limit_rows_of_a_batch_removal_read_as_empty_while_it_is_announced();
  return rqt_test::finish("limit_edge_protocol_batches");
}
