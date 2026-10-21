// A receiver of a signal of a container, or of its model, mutates the same container while the announcement is still
// going out. Every mutator is tried from every phase (the "about" and the "done" signal) and from a receiver
// connected before and after the model's own connection, for a list and for a map.
//
// The rule: the nested call is refused. It returns false (0, an empty container), changes nothing, emits nothing, and
// a debug build asserts. The same file is built twice. In a debug build every case runs in a child process and must
// end in SIGABRT (a case in which the call was not possible in that state may end quietly). In the build that
// defines QT_NO_DEBUG (capability_edge_reentrancy_release) the calls return, and the content and the model's
// announcements must equal those of the same change made without a receiver.
#include "edge_support.hpp"

#include <QtCore/QCoreApplication>

#include <algorithm>
#include <string>
#include <vector>

namespace {
using namespace edge;

template <class Kit>
struct Nested {
  using F = Fx<Kit>;
  std::string name;
  std::function<bool(F&)> valid;  // can the call succeed at all in the state the receiver sees
  std::function<bool(F&)> call;   // true: the container took the change
};

// What a mutator returns, as "did it take": a count above zero, a non-empty container, or content that changed.
template <class Kit>
std::function<bool(Fx<Kit>&)> setDataNested() {
  return [](Fx<Kit>& f) { return f.model->setData(f.model->index(0), kEditedValue); };
}
// The model's first row names a key; setData is possible when the container still holds it.
template <class Kit>
bool setDataValid(Fx<Kit>& f) {
  if (f.model->rowCount() == 0 || f.src.size() == 0) return false;
  if constexpr (Kit::kStableKeys) return f.src.contains(f.model->index(0).data(Qt::UserRole).toInt());
  return true;
}

std::vector<Nested<ListKit>> listNesteds() {
  using F = Fx<ListKit>;
  const auto some = [](F& f) { return f.src.size() >= 1; };
  const auto always = [](F&) { return true; };
  std::vector<Nested<ListKit>> v;
  v.push_back({"insert", always, [](F& f) { return f.src.insert(0, Sample{kNestedValue}); }});
  v.push_back({"remove", some, [](F& f) { return f.src.remove(0); }});
  v.push_back({"replace", some, [](F& f) { return f.src.replace(0, Sample{kNestedValue}); }});
  v.push_back({"editValue", some, [](F& f) { return f.src.editValue(0, [](Sample& s) { s.value = kNestedValue; }); }});
  v.push_back({"move", [](F& f) { return f.src.size() >= 2; }, [](F& f) { return f.src.move(0, 1); }});
  v.push_back({"insertRange", always, [](F& f) { return f.src.insertRange(0, {Sample{kNestedValue}, Sample{kNestedValue + 1}}); }});
  v.push_back({"removeRange", some, [](F& f) { return f.src.removeRange(0, 1); }});
  v.push_back({"clear", some, [](F& f) { return f.src.clear(); }});
  v.push_back({"resetWith", always, [](F& f) {
                 const auto before = f.contents();
                 f.src.resetWith({Sample{kNestedValue}});
                 return f.contents() != before;
               }});
  v.push_back({"setData", setDataValid<ListKit>, setDataNested<ListKit>()});
  return v;
}

std::vector<Nested<MapKit>> mapNesteds() {
  using F = Fx<MapKit>;
  const auto always = [](F&) { return true; };
  std::vector<Nested<MapKit>> v;
  v.push_back({"insert", [](F& f) { return !f.src.contains(kNestedNewKey); }, [](F& f) { return f.src.insert(kNestedNewKey, Sample{kNestedValue}); }});
  v.push_back({"remove", [](F& f) { return f.src.contains(kNestedRemoved); }, [](F& f) { return f.src.remove(kNestedRemoved); }});
  v.push_back({"replace", [](F& f) { return f.src.contains(kNestedKey); }, [](F& f) { return f.src.replace(kNestedKey, Sample{kNestedValue}); }});
  v.push_back({"editValue", [](F& f) { return f.src.contains(kNestedKey); },
               [](F& f) { return f.src.editValue(kNestedKey, [](Sample& s) { s.value = kNestedValue; }); }});
  v.push_back({"insertMany", [](F& f) { return !f.src.contains(kNestedBatchA); }, [](F& f) {
                 QMap<int, Sample> batch;
                 batch.insert(kNestedBatchA, Sample{kNestedValue});
                 batch.insert(kNestedBatchB, Sample{kNestedValue + 1});
                 return f.src.insertMany(batch) > 0;
               }});
  v.push_back({"removeMany", [](F& f) { return f.src.contains(kNestedRemoved); }, [](F& f) { return f.src.removeMany({kNestedRemoved}) > 0; }});
  v.push_back({"takeMany", [](F& f) { return f.src.contains(kNestedRemoved); }, [](F& f) { return !f.src.takeMany({kNestedRemoved}).isEmpty(); }});
  v.push_back({"clear", [](F& f) { return f.src.size() >= 1; }, [](F& f) { return f.src.clear(); }});
  v.push_back({"resetWith", always, [](F& f) {
                 const auto before = f.contents();
                 f.src.resetWith({{kNestedNewKey, Sample{kNestedValue}}});
                 return f.contents() != before;
               }});
  v.push_back({"setData", setDataValid<MapKit>, setDataNested<MapKit>()});
  return v;
}

template <class Kit>
int scenario(const Outer<Kit>& out, const Nested<Kit>& nested, Where where, Phase phase) {
  Fx<Kit> baseline;
  out.run(baseline);
  const std::vector<int> expectedContents = baseline.contents();
  const QStringList expectedLog = baseline.log;

  const auto& containerHook = phase == Phase::about ? out.containerAbout : out.containerDone;
  const auto& modelHook = phase == Phase::about ? out.modelAbout : out.modelDone;
  if (where == Where::model ? !modelHook : !containerHook) return kNoHook;

  Fx<Kit>* fx = nullptr;
  bool fired = false, valid = false, accepted = false;
  const Receiver receiver = [&] {
    if (fired || !fx) return;
    fired = true;
    valid = nested.valid(*fx);
    accepted = nested.call(*fx);
  };
  std::unique_ptr<Fx<Kit>> owned;
  if (where == Where::containerEarly) {
    owned = std::make_unique<Fx<Kit>>([&](typename Fx<Kit>::Src& s) { containerHook(s, receiver); });
  } else {
    owned = std::make_unique<Fx<Kit>>();
  }
  fx = owned.get();
  if (where == Where::containerLate) containerHook(fx->src, receiver);
  if (where == Where::model) modelHook(*fx->model, receiver);

  out.run(*fx);
  fx->watcher->check();

  int code = 0;
  if (!fx->watcher->clean() || fx->model->rowCount() != fx->src.size()) code |= kDirty;
  if (!valid) code |= kSkipped;
  else if (!accepted) code |= kRefused;
  if (fx->contents() != expectedContents || fx->log != expectedLog) code |= kDiffers;
  return code;
}

struct Tally {
  int total = 0, aborted = 0, refused = 0, failed = 0;
};

// What a case must come to. A debug build asserts, so the child aborts; a call that was not possible in the state it was
// made in may return before it reaches the assertion. Without the assertion the call is refused and nothing else happens.
bool holds(int code) {
#ifndef QT_NO_DEBUG
  return code == -SIGABRT || (code >= 0 && (code & kSkipped));
#else
  return code >= 0 && !(code & (kDirty | kDiffers)) && (code & (kRefused | kSkipped));
#endif
}

template <class Kit>
Tally sweep(const std::vector<Outer<Kit>>& outers, const std::vector<Nested<Kit>>& nesteds, Where where,
            const std::vector<std::string>& only) {
  Tally t;
  for (const auto& out : outers) {
    if (std::find(only.begin(), only.end(), out.name) == only.end()) continue;
    for (Phase phase : {Phase::about, Phase::done})
      for (const auto& nested : nesteds) {
        const int code = forked([&] { return scenario<Kit>(out, nested, where, phase); });
        if (code == kNoHook) continue;
        ++t.total;
        if (code == -SIGABRT) ++t.aborted;
        if (code >= 0 && (code & kRefused)) ++t.refused;
        if (!holds(code)) {
          ++t.failed;
          std::printf("  %s %s phase %d nested %s: code %d\n", Kit::kName, out.name.c_str(), static_cast<int>(phase), nested.name.c_str(), code);
        }
      }
  }
  return t;
}

void report(const char* what, const Tally& t) {
  std::printf("%-34s %4d cases: %d aborted, %d refused, %d failed\n", what, t.total, t.aborted, t.refused, t.failed);
}

// Every nested mutator, from the given outer changes, for a list and for a map. Nothing refuses a nested change,
// and some of them leave the model out of step with its container or end the process.
void sweep_both(const char* what, Where where, const std::vector<std::string>& only) {
  const Tally l = sweep<ListKit>(listOuters(), listNesteds(), where, only);
  const Tally m = sweep<MapKit>(mapOuters(), mapNesteds(), where, only);
  report((std::string("list, ") + what).c_str(), l);
  report((std::string("map, ") + what).c_str(), m);
  CHECK(l.total + m.total > 0);
#ifndef QT_NO_DEBUG
  CHECK(l.aborted + m.aborted > 0);
  CHECK(l.failed == 0 && m.failed == 0);
#else
  CHECK(l.refused + m.refused > 0);
  CHECK(l.failed == 0 && m.failed == 0);
#endif
}

const std::vector<std::string> kSingle{"insert", "remove", "replace", "editValue", "move"};
const std::vector<std::string> kBatch{"insertRange", "removeRange", "insertMany", "removeMany", "takeMany"};
const std::vector<std::string> kWhole{"clear", "resetWith"};
const std::vector<std::string> kRows{"insert", "remove", "replace", "editValue", "move", "insertRange", "removeRange", "insertMany", "removeMany", "takeMany"};
const std::vector<std::string> kAll = [] {
  auto all = kRows;
  all.insert(all.end(), kWhole.begin(), kWhole.end());
  return all;
}();

void container_single_entry_signals() { sweep_both("single-entry signals", Where::containerLate, kSingle); }
void container_batch_signals() { sweep_both("range and batch signals", Where::containerLate, kBatch); }
void container_clear_and_reset_signals() { sweep_both("cleared and reset signals", Where::containerLate, kWhole); }
void receiver_before_the_model() { sweep_both("receiver before the model", Where::containerEarly, kAll); }
void model_row_and_data_signals() { sweep_both("model row and data signals", Where::model, kRows); }
void model_reset_signals() { sweep_both("model reset signals", Where::model, kWhole); }
}  // namespace

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  container_single_entry_signals();
  container_batch_signals();
  container_clear_and_reset_signals();
  receiver_before_the_model();
  model_row_and_data_signals();
  model_reset_signals();
#ifndef EDGE_TEST_NAME
#define EDGE_TEST_NAME "capability_edge_reentrancy"
#endif
  return rqt_test::finish(EDGE_TEST_NAME);
}
