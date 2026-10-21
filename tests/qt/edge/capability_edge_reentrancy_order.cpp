// A receiver connected to the container before the model's own connection hears every signal first. At a "done"
// signal it sees the container already changed while the model still has the old rows. The model's own row count stays
// consistent with what it has announced, reading every row of it never goes out of range, and once the emission has
// finished the model and the container agree.
#include "edge_support.hpp"

#include <QtCore/QCoreApplication>

namespace {
using namespace edge;

template <class Kit>
int earlyReader(const Outer<Kit>& out, Phase phase) {
  using F = Fx<Kit>;
  const auto& hook = phase == Phase::about ? out.containerAbout : out.containerDone;
  if (!hook) return kNoHook;
  F* fxp = nullptr;
  bool fired = false;
  int rowsSeen = -1;
  int reads = 0;
  const Receiver receiver = [&] {
    if (fired || !fxp) return;
    fired = true;
    rowsSeen = fxp->model->rowCount();
    for (int row = 0; row < rowsSeen; ++row) {
      (void)fxp->model->index(row, 0).data();
      (void)fxp->model->index(row, 0).data(Qt::UserRole);
      ++reads;
    }
  };
  F fx([&](typename F::Src& s) { hook(s, receiver); });
  fxp = &fx;
  out.run(fx);
  fx.watcher->check();
  int code = 0;
  if (rowsSeen != kEntries || reads != kEntries) code |= kDirty;  // the model had not heard yet: its old rows
  if (!fx.watcher->clean() || !agrees(*fx.model, fx.src.items())) code |= kDirty;
  return code;
}

template <class Kit>
int sweep(const std::vector<Outer<Kit>>& outers) {
  int cases = 0;
  for (const auto& out : outers)
    for (Phase phase : {Phase::about, Phase::done}) {
      const int code = forked([&] { return earlyReader<Kit>(out, phase); });
      if (code == kNoHook) continue;
      ++cases;
      CHECK(code == 0);
      if (code != 0) std::fprintf(stderr, "%s %s phase %d: code %d\n", Kit::kName, out.name.c_str(), static_cast<int>(phase), code);
    }
  return cases;
}

void an_early_receiver_reads_a_model_that_has_not_heard_yet() {
  CHECK(sweep<ListKit>(listOuters()) > 0);
  CHECK(sweep<MapKit>(mapOuters()) > 0);
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  an_early_receiver_reads_a_model_that_has_not_heard_yet();
  return rqt_test::finish("capability_edge_reentrancy_order");
}
