// A model follows the destruction of its container: it holds the container by QPointer and, when the container is
// deleted first (by delete, or by deleteLater at the next turn of the event loop), resets to empty with one
// begin/endResetModel, so rowCount is 0 and no later call reads the container that is gone. Before the fix the model
// kept its old rows and announced nothing (pinned as capability_edge_thread_stale_model).
// Catalogue: docs/qt-edge-cases/threads.md (T-16, T-17, T-18).
#include "thread_support.hpp"

namespace {
using namespace edge_thread;

const auto kFormat = [](const int&, const QString& v) { return QVariant(v); };

void a_model_resets_when_its_container_is_deleted() {
  auto* source = new Source();
  Model model(source, kFormat);
  CHECK(source->insertMany(C{{1, "a"}, {2, "b"}, {3, "c"}}) == 3);
  int resets = 0, removals = 0;
  QObject::connect(&model, &QAbstractItemModel::modelReset, &model, [&] { ++resets; });
  QObject::connect(&model, &QAbstractItemModel::rowsRemoved, &model, [&] { ++removals; });
  delete source;
  CHECK(model.rowCount() == 0);  // the container is gone: one reset to empty, no row-by-row removal
  CHECK(resets == 1 && removals == 0);
}

void deleteLater_resets_the_model_at_the_next_turn() {
  auto* source = new Source();
  QPointer<Source> guard(source);
  Model model(source, kFormat);
  CHECK(source->insert(1, "a"));
  source->deleteLater();
  CHECK(source->insert(2, "b") && model.rowCount() == 2);  // until the loop turns the model follows
  QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  CHECK(!guard);
  CHECK(model.rowCount() == 0);  // and after it the model is empty
}

// The signals queued for a model on another thread are heard after the container is gone (they carry the key by
// value). The model has already reset to empty when the container was destroyed and ignores what arrives for a
// container it no longer has, so it ends empty and does not hold rows the container never held.
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
  CHECK(model->rowCount() == 0);
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
  a_model_resets_when_its_container_is_deleted();
  deleteLater_resets_the_model_at_the_next_turn();
  a_queued_model_hears_the_container_after_its_death();
  return rqt_test::finish("capability_edge_thread_stale_model");
}
