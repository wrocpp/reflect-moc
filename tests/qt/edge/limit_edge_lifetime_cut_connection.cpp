// The model's connections to the container are cut after the model has heard the "about" signal of a change and before
// the "done" signal: the model has begun a change that nothing ends. A pairing that a receiver breaks cannot be mended
// by the model, because nothing tells it that the end will not come. Nothing crashes, and the model keeps its rows.
#include "edge_support.hpp"

#include <QtCore/QCoreApplication>

namespace {
using namespace edge;
using Listed = observable::ObservableContainer<QList<Sample>>;

void a_connection_cut_after_the_model_began_the_change() {
  Fx<ListKit> fx;
  QObject::connect(&fx.src, &Listed::entryAboutToBeAdded, &fx.src, [&](const qsizetype&) {
    QObject::disconnect(&fx.src, nullptr, fx.model.get(), nullptr);
  });
  CHECK(fx.src.insert(0, Sample{kNestedValue}));
  fx.watcher->check();
  CHECK(!fx.watcher->clean());  // announced and never confirmed
  CHECK(fx.model->rowCount() == kEntries && fx.src.size() == kEntries + 1);
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  a_connection_cut_after_the_model_began_the_change();
  return rqt_test::finish("limit_edge_lifetime_cut_connection");
}
