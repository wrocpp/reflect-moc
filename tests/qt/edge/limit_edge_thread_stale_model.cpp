// Pinned: a model keeps a raw pointer to its container and does not hear of the container's destruction. When the container
// is deleted first (by delete, or by deleteLater at the next turn of the event loop) the model neither resets nor
// empties: it keeps its old row count and rows, announces nothing, and every data() call reads the container that is
// gone. The test looks only at what is safe to look at: the row count and the signals (data() would read freed memory).
// A fix connects to the container's destroyed signal and resets the model to empty; this file then becomes
// capability_edge_thread_stale_model.
// Catalogue: docs/qt-edge-cases/threads.md.
#include "thread_support.hpp"

namespace {
using namespace edge_thread;

const auto kFormat = [](const int&, const QString& v) { return QVariant(v); };

void a_model_outlives_its_container() {
  auto* source = new Source();
  Model model(source, kFormat);
  CHECK(source->insertMany(C{{1, "a"}, {2, "b"}, {3, "c"}}) == 3);
  int resets = 0, removals = 0;
  QObject::connect(&model, &QAbstractItemModel::modelReset, &model, [&] { ++resets; });
  QObject::connect(&model, &QAbstractItemModel::rowsRemoved, &model, [&] { ++removals; });
  delete source;
  CHECK(model.rowCount() == 3);  // rows of a container that is gone
  CHECK(resets == 0 && removals == 0);
}

void deleteLater_leaves_the_same_model_behind() {
  auto* source = new Source();
  QPointer<Source> guard(source);
  Model model(source, kFormat);
  CHECK(source->insert(1, "a"));
  source->deleteLater();
  CHECK(source->insert(2, "b") && model.rowCount() == 2);  // until the loop turns the model follows
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  CHECK(!guard);
  CHECK(model.rowCount() == 2);  // and after it, it holds on to the two rows
}

// The signals a model queued for a container are heard after the container is gone (they carry the key by value), so
// a model on another thread can end with rows the container never held at the moment it hears them.
void a_queued_model_hears_the_container_after_its_death() {
  auto* source = new Source();
  auto* model = new Model(source, kFormat);
  QThread thread;
  QObject anchor;
  model->moveToThread(&thread);
  anchor.moveToThread(&thread);
  CHECK(source->insert(1, "a") && source->insert(2, "b") && source->remove(1));
  delete source;
  thread.start();
  QMetaObject::invokeMethod(&anchor, [] {}, Qt::BlockingQueuedConnection);
  CHECK(model->rowCount() == 1);
  QMetaObject::invokeMethod(&anchor, [&] {
    model->moveToThread(QCoreApplication::instance()->thread());
    anchor.moveToThread(QCoreApplication::instance()->thread());
  }, Qt::BlockingQueuedConnection);
  CHECK(stop(thread));
  delete model;
}
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  a_model_outlives_its_container();
  deleteLater_leaves_the_same_model_behind();
  a_queued_model_hears_the_container_after_its_death();
  return rqt_test::finish("limit_edge_thread_stale_model");
}
