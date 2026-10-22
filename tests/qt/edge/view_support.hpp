// Helpers of the view and QML edge tests: models over string-valued containers with a tester in fatal mode, and a
// bounded wait that processes events.
#pragma once

#include "check.hpp"

#include <container_model.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QDeadlineTimer>
#include <QtCore/QElapsedTimer>
#include <QtCore/QStringList>
#include <QtGui/QWindow>
#include <QtTest/QAbstractItemModelTester>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QWidget>

#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace edge_view {

constexpr auto kFatal = QAbstractItemModelTester::FailureReportingMode::Fatal;
constexpr int kKeyRole = Qt::UserRole;      // ContainerModel::KeyRole
constexpr int kWaitMs = 3000;               // every wait is bounded by this
constexpr int kPollMs = 5;
constexpr int kSettleMinMs = 30;            // settle() never returns sooner than this, as before it was condition based
constexpr int kSettleBoundMs = 10000;       // a scene that is still changing after this long is reported, not waited for
constexpr int kStableTurns = 3;             // event loop turns over which nothing may have changed

// Runs the event loop until `done` is true or the bound passes; returns whether it became true.
inline bool wait_for(const std::function<bool()>& done, int timeoutMs = kWaitMs) {
  QDeadlineTimer deadline(timeoutMs);
  while (!done()) {
    if (deadline.hasExpired()) return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, kPollMs);
  }
  return true;
}

// A hook that returns a fingerprint of everything a scene shows that can still change while the event loop runs: set by
// the QML helpers, which also force the polish and the synchronisation of their windows each time it is called.
inline std::function<std::uint64_t()>& scene_probe() {
  static std::function<std::uint64_t()> probe;
  return probe;
}

inline void mix(std::uint64_t& hash, std::uint64_t value) { hash = (hash ^ value) * 1099511628211ULL + 0x9e3779b97f4a7c15ULL; }

// Geometry, visibility and scroll position of every widget under the visible top-level widgets, and the current
// row of every item view, so that a layout that is still moving or a delayed relayout changes the number.
inline std::uint64_t widget_fingerprint() {
  std::uint64_t hash = 14695981039346656037ULL;
  for (QWidget* top : QApplication::topLevelWidgets()) {
    if (!top->isVisible()) continue;
    const auto children = top->findChildren<QWidget*>();
    mix(hash, static_cast<std::uint64_t>(children.size()));
    for (QWidget* w : children) {
      mix(hash, static_cast<std::uint64_t>(w->x() * 31 + w->y() * 17 + w->width() * 7 + w->height() + (w->isVisible() ? 1 : 0)));
      if (auto* view = qobject_cast<QAbstractItemView*>(w)) {
        mix(hash, static_cast<std::uint64_t>(view->verticalScrollBar()->value() * 13 + view->verticalScrollBar()->maximum()));
        mix(hash, static_cast<std::uint64_t>(view->horizontalScrollBar()->value() * 13 + view->horizontalScrollBar()->maximum()));
        mix(hash, static_cast<std::uint64_t>(view->currentIndex().row() + 2));
      }
    }
  }
  return hash;
}

// Lets timers and queued work of the moment run, for at least `minMs`, and until the scene has stopped changing:
// the same fingerprint over kStableTurns event loop turns, with the deferred deletes delivered and the windows
// polished. A scene that never stops is reported once, after kSettleBoundMs, instead of being waited for forever.
inline void settle(int minMs = kSettleMinMs) {
  QElapsedTimer clock;
  clock.start();
  const auto look = [] {
    std::uint64_t hash = widget_fingerprint();
    if (scene_probe()) mix(hash, scene_probe()());
    return hash;
  };
  std::uint64_t last = look();
  int stable = 0;
  while (stable < kStableTurns || clock.elapsed() < minMs) {
    if (clock.elapsed() > kSettleBoundMs) {
      ::rqt_test::report(false, "settle: the scene did not stop changing", __FILE__, __LINE__);
      return;
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, kPollMs);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const std::uint64_t now = look();
    stable = now == last ? stable + 1 : 0;
    last = now;
  }
}

template <class C>
inline QVariant show(const typename observable::ContainerTraits<C>::Key&, const typename observable::ContainerTraits<C>::Value& v) {
  return QVariant(v);
}

// A list model over a container of strings, with an optional editor (empty text is refused).
template <class C>
struct Rig {
  using Source = observable::ObservableContainer<C>;
  using Model = observable::ContainerModel<C>;
  using Value = typename Source::Value;

  explicit Rig(bool editable = false)
      : model(editable ? std::make_unique<Model>(&source, show<C>,
                                                 [](const Value&, const QVariant& edited) -> std::optional<Value> {
                                                   if (edited.toString().isEmpty()) return std::nullopt;
                                                   return edited.toString();
                                                 })
                       : std::make_unique<Model>(&source, show<C>)),
        tester(model.get(), kFatal) {}

  Source source;
  std::unique_ptr<Model> model;
  QAbstractItemModelTester tester;
};

inline QStringList column(const QAbstractItemModel& model, int role = Qt::DisplayRole) {
  QStringList out;
  for (int row = 0; row < model.rowCount(); ++row) out.append(model.index(row, 0).data(role).toString());
  return out;
}

}  // namespace edge_view
