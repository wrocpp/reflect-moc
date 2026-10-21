// Shared by the edge tests of re-entrancy and lifetime: a list and a map of five entries, a fixture with a model, a
// view mirror and a log of what the model announced, and a way to run a case in a child process.
#pragma once

#include "check.hpp"
#include "observable_models_support.hpp"

#include <container_model.hpp>

#include <QtCore/QMap>

#include <sys/resource.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <functional>
#include <string>
#include <memory>
#include <optional>
#include <vector>

namespace edge {
using observable_test::ModelWatcher;
using observable_test::Sample;

constexpr int kEntries = 5;           // values 1..kEntries; a map holds them under the keys 10, 20, ...
constexpr int kKeyStep = 10;
constexpr int kOuterValue = 100;      // what the outer change adds or writes
constexpr int kNestedValue = 200;     // what the receiver adds or writes
constexpr int kEditedValue = 77;      // what setData writes
constexpr int kChildSeconds = 20;     // a corrupted model may loop; a child gets this long
// Keys of a map: the outer change and the nested one never name the same key.
constexpr int kOuterNewKey = 25, kOuterKeyA = 20, kOuterKeyB = 30, kOuterBatchA = 12, kOuterBatchB = 14, kOuterRemoveA = 10;
constexpr int kNestedNewKey = 15, kNestedBatchA = 16, kNestedBatchB = 18, kNestedKey = 40, kNestedRemoved = 50;

inline std::optional<Sample> parse(const Sample&, const QVariant& edited) {
  bool ok = false;
  const int value = edited.toInt(&ok);
  if (!ok) return std::nullopt;
  return Sample{value};
}

struct ListKit {
  using C = QList<Sample>;
  static constexpr bool kStableKeys = false;
  static constexpr const char* kName = "list";
  static C initial() {
    C c;
    for (int v = 1; v <= kEntries; ++v) c.append(Sample{v});
    return c;
  }
  static QVariant show(const qsizetype&, const Sample& s) { return s.value; }
  static std::vector<int> contents(const C& c) {
    std::vector<int> out;
    for (const Sample& s : c) out.push_back(s.value);
    return out;
  }
};

struct MapKit {
  using C = QMap<int, Sample>;
  static constexpr bool kStableKeys = true;
  static constexpr const char* kName = "map";
  static C initial() {
    C c;
    for (int v = 1; v <= kEntries; ++v) c.insert(v * kKeyStep, Sample{v});
    return c;
  }
  static QVariant show(const int&, const Sample& s) { return s.value; }
  static std::vector<int> contents(const C& c) {
    std::vector<int> out;
    for (auto it = c.begin(); it != c.end(); ++it) {
      out.push_back(it.key());
      out.push_back(it.value().value);
    }
    return out;
  }
};

template <class Kit>
struct Fx {
  using C = typename Kit::C;
  using Src = observable::ObservableContainer<C>;
  using Mdl = observable::ContainerModel<C>;

  Src src{Kit::initial()};
  std::unique_ptr<Mdl> model;
  std::unique_ptr<ModelWatcher> watcher;
  QStringList log;  // what the model announced, in order

  explicit Fx(const std::function<void(Src&)>& beforeModel = {}) {
    if (beforeModel) beforeModel(src);
    model = std::make_unique<Mdl>(&src, Kit::show, parse);
    watcher = std::make_unique<ModelWatcher>(model.get(), Kit::kStableKeys);
    using M = QAbstractItemModel;
    Mdl* m = model.get();
    QObject::connect(m, &M::rowsInserted, m, [this](const QModelIndex&, int f, int l) { log << QString("ins %1-%2").arg(f).arg(l); });
    QObject::connect(m, &M::rowsRemoved, m, [this](const QModelIndex&, int f, int l) { log << QString("rem %1-%2").arg(f).arg(l); });
    QObject::connect(m, &M::rowsMoved, m, [this](const QModelIndex&, int f, int l, const QModelIndex&, int d) {
      log << QString("mov %1-%2 to %3").arg(f).arg(l).arg(d);
    });
    QObject::connect(m, &M::dataChanged, m, [this](const QModelIndex& t, const QModelIndex& b) {
      log << QString("chg %1-%2").arg(t.row()).arg(b.row());
    });
    QObject::connect(m, &M::modelReset, m, [this] { log << "rst"; });
  }
  std::vector<int> contents() const { return Kit::contents(src.items()); }
};

// Runs `scenario` in a child: -signal for a crash, otherwise the exit code.
inline int forked(const std::function<int()>& run) {
  std::fflush(nullptr);
  const pid_t child = fork();
  if (child == 0) {
    const rlimit noCore{0, 0};
    setrlimit(RLIMIT_CORE, &noCore);
#if defined(__linux__)
    prctl(PR_SET_DUMPABLE, 0);  // RLIMIT_CORE 0 does not stop a dump to a piped handler (systemd-coredump on a runner)
#endif
    alarm(kChildSeconds);
    if (!std::freopen("/dev/null", "w", stderr)) _exit(2);
    _exit(run());
  }
  int status = 0;
  waitpid(child, &status, 0);
  return WIFSIGNALED(status) ? -WTERMSIG(status) : WEXITSTATUS(status);
}

using Receiver = std::function<void()>;

template <class Kit>
struct Outer {
  using F = Fx<Kit>;
  std::string name;
  std::function<void(typename F::Src&, Receiver)> containerAbout, containerDone;  // connect a receiver to a container signal
  std::function<void(typename F::Mdl&, Receiver)> modelAbout, modelDone;          // and to a signal of its model
  std::function<void(F&)> run;
};

template <class Obj, class Sig>
void connectTo(Obj& obj, Sig sig, Receiver receiver) {
  QObject::connect(&obj, sig, &obj, [receiver](auto&&...) { receiver(); });
}

template <class Kit, class SrcAbout, class SrcDone, class MdlAbout, class MdlDone>
Outer<Kit> outer(std::string name, SrcAbout sa, SrcDone sd, MdlAbout ma, MdlDone md, std::function<void(Fx<Kit>&)> run) {
  using F = Fx<Kit>;
  Outer<Kit> o;
  o.name = std::move(name);
  o.run = std::move(run);
  if constexpr (!std::is_null_pointer_v<SrcAbout>) o.containerAbout = [sa](typename F::Src& s, Receiver r) { connectTo(s, sa, std::move(r)); };
  if constexpr (!std::is_null_pointer_v<SrcDone>) o.containerDone = [sd](typename F::Src& s, Receiver r) { connectTo(s, sd, std::move(r)); };
  if constexpr (!std::is_null_pointer_v<MdlAbout>) o.modelAbout = [ma](typename F::Mdl& m, Receiver r) { connectTo(m, ma, std::move(r)); };
  if constexpr (!std::is_null_pointer_v<MdlDone>) o.modelDone = [md](typename F::Mdl& m, Receiver r) { connectTo(m, md, std::move(r)); };
  return o;
}

using M = QAbstractItemModel;

std::vector<Outer<ListKit>> listOuters() {
  using S = observable::ObservableContainer<QList<Sample>>;
  using F = Fx<ListKit>;
  std::vector<Outer<ListKit>> v;
  v.push_back(outer<ListKit>("insert", &S::entryAboutToBeAdded, &S::entryAdded, &M::rowsAboutToBeInserted, &M::rowsInserted,
                             [](F& f) { f.src.insert(2, Sample{kOuterValue}); }));
  v.push_back(outer<ListKit>("remove", &S::entryAboutToBeRemoved, &S::entryRemoved, &M::rowsAboutToBeRemoved, &M::rowsRemoved,
                             [](F& f) { f.src.remove(2); }));
  v.push_back(outer<ListKit>("replace", &S::entryAboutToBeChanged, &S::entryChanged, nullptr, &M::dataChanged,
                             [](F& f) { f.src.replace(2, Sample{kOuterValue}); }));
  v.push_back(outer<ListKit>("editValue", &S::entryAboutToBeChanged, &S::entryChanged, nullptr, &M::dataChanged,
                             [](F& f) { f.src.editValue(2, [](Sample& s) { s.value = kOuterValue; }); }));
  v.push_back(outer<ListKit>("move", &S::entryAboutToBeMoved, &S::entryMoved, &M::rowsAboutToBeMoved, &M::rowsMoved,
                             [](F& f) { f.src.move(1, 3); }));
  v.push_back(outer<ListKit>("insertRange", &S::rangeAboutToBeAdded, &S::rangeAdded, &M::rowsAboutToBeInserted, &M::rowsInserted,
                             [](F& f) { f.src.insertRange(2, {Sample{kOuterValue}, Sample{kOuterValue + 1}}); }));
  v.push_back(outer<ListKit>("removeRange", &S::rangeAboutToBeRemoved, &S::rangeRemoved, &M::rowsAboutToBeRemoved, &M::rowsRemoved,
                             [](F& f) { f.src.removeRange(1, 2); }));
  v.push_back(outer<ListKit>("clear", &S::aboutToBeCleared, &S::cleared, &M::modelAboutToBeReset, &M::modelReset,
                             [](F& f) { f.src.clear(); }));
  v.push_back(outer<ListKit>("resetWith", &S::aboutToBeReset, &S::resetCompleted, &M::modelAboutToBeReset, &M::modelReset,
                             [](F& f) { f.src.resetWith({Sample{kOuterValue}, Sample{kOuterValue + 1}}); }));
  return v;
}

std::vector<Outer<MapKit>> mapOuters() {
  using S = observable::ObservableContainer<QMap<int, Sample>>;
  using F = Fx<MapKit>;
  std::vector<Outer<MapKit>> v;
  v.push_back(outer<MapKit>("insert", &S::entryAboutToBeAdded, &S::entryAdded, &M::rowsAboutToBeInserted, &M::rowsInserted,
                            [](F& f) { f.src.insert(kOuterNewKey, Sample{kOuterValue}); }));
  v.push_back(outer<MapKit>("remove", &S::entryAboutToBeRemoved, &S::entryRemoved, &M::rowsAboutToBeRemoved, &M::rowsRemoved,
                            [](F& f) { f.src.remove(kOuterKeyB); }));
  v.push_back(outer<MapKit>("replace", &S::entryAboutToBeChanged, &S::entryChanged, nullptr, &M::dataChanged,
                            [](F& f) { f.src.replace(kOuterKeyA, Sample{kOuterValue}); }));
  v.push_back(outer<MapKit>("editValue", &S::entryAboutToBeChanged, &S::entryChanged, nullptr, &M::dataChanged,
                            [](F& f) { f.src.editValue(kOuterKeyA, [](Sample& s) { s.value = kOuterValue; }); }));
  v.push_back(outer<MapKit>("insertMany", &S::entriesAboutToBeAdded, &S::entriesAdded, &M::rowsAboutToBeInserted, &M::rowsInserted,
                            [](F& f) {
                              QMap<int, Sample> batch;
                              batch.insert(kOuterBatchA, Sample{kOuterValue});
                              batch.insert(kOuterBatchB, Sample{kOuterValue + 1});
                              f.src.insertMany(batch);
                            }));
  v.push_back(outer<MapKit>("removeMany", &S::entriesAboutToBeRemoved, &S::entriesRemoved, &M::rowsAboutToBeRemoved, &M::rowsRemoved,
                            [](F& f) { f.src.removeMany({kOuterRemoveA, kOuterKeyA}); }));
  v.push_back(outer<MapKit>("takeMany", &S::entriesAboutToBeRemoved, &S::entriesRemoved, &M::rowsAboutToBeRemoved, &M::rowsRemoved,
                            [](F& f) { f.src.takeMany({kOuterKeyA, kOuterKeyB}); }));
  v.push_back(outer<MapKit>("clear", &S::aboutToBeCleared, &S::cleared, &M::modelAboutToBeReset, &M::modelReset,
                            [](F& f) { f.src.clear(); }));
  v.push_back(outer<MapKit>("resetWith", &S::aboutToBeReset, &S::resetCompleted, &M::modelAboutToBeReset, &M::modelReset,
                            [](F& f) { f.src.resetWith({{kOuterNewKey, Sample{kOuterValue}}}); }));
  return v;
}

enum class Where { containerEarly, containerLate, model };
enum class Phase { about, done };

// What one case came to, as bits of a child's exit code.
constexpr int kDirty = 1;     // the model and the container, or a view's mirror, disagree afterwards
constexpr int kRefused = 2;   // the call was possible and the container did not take it
constexpr int kSkipped = 4;   // the call was not possible in that state, so it says nothing about the rule
constexpr int kDiffers = 8;   // the content or the model's announcements differ from the same change without a receiver
constexpr int kNoHook = 16;   // this outer change has no signal in that place

// Does a model show the container: the same rows, in the container's order.
inline bool agrees(const QAbstractItemModel& model, const QList<Sample>& items) {
  if (model.rowCount() != items.size()) return false;
  for (int row = 0; row < model.rowCount(); ++row)
    if (model.index(row, 0).data().toInt() != items[row].value) return false;
  return true;
}

inline bool agrees(const QAbstractItemModel& model, const QMap<int, Sample>& items) {
  if (model.rowCount() != items.size()) return false;
  int row = 0;
  for (auto it = items.begin(); it != items.end(); ++it, ++row)
    if (model.index(row, 0).data().toInt() != it.value().value) return false;
  return true;
}

}  // namespace edge
