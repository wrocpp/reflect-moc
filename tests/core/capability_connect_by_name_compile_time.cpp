// A signal named by a string is resolved at compile time, to a generated
// NOTIFY signal or a declared one. A misspelled name is a compile error:
// see connect_by_name_typo.cpp and capability_connect_by_name_rejects_typo.
#include "check.hpp"

namespace {

constexpr int progress_value = 40;

struct Worker : rqt::Object {
  [[= rqt::property{.notify = true}]] int progress = 0;
  [[= rqt::signal]] void finished() { rqt::emit{this}(); }
};

struct Ui : rqt::Object {
  int progress_seen = 0;
  int finished_seen = 0;
  void onProgress(int value) { progress_seen = value; }
  void onFinished() { ++finished_seen; }
};

static_assert(rqt::detail::resolve_signal<Worker, "progressChanged">() == ^^Worker::progress);
static_assert(rqt::detail::resolve_signal<Worker, "finished">() == ^^Worker::finished);
static_assert(rqt::detail::resolve_signal<Worker, &Worker::progress>() == ^^Worker::progress);

}  // namespace

int main() {
  Worker worker;
  Ui ui;
  rqt::connect<"progressChanged">(worker, ui, &Ui::onProgress);
  rqt::connect<"finished">(worker, ui, &Ui::onFinished);

  worker.set<"progress">(progress_value);
  worker.finished();

  RQT_CHECK(ui.progress_seen == progress_value);
  RQT_CHECK(ui.finished_seen == 1);
  return rqt_test::result();
}
