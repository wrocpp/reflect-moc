// Helpers of the view and QML edge tests: models over string-valued containers with a tester in fatal mode, and a
// bounded wait that processes events.
#pragma once

#include "check.hpp"

#include <container_model.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QDeadlineTimer>
#include <QtCore/QStringList>
#include <QtTest/QAbstractItemModelTester>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace edge_view {

constexpr auto kFatal = QAbstractItemModelTester::FailureReportingMode::Fatal;
constexpr int kKeyRole = Qt::UserRole;      // ContainerModel::KeyRole
constexpr int kWaitMs = 3000;               // every wait is bounded by this
constexpr int kPollMs = 5;

// Runs the event loop until `done` is true or the bound passes; returns whether it became true.
inline bool wait_for(const std::function<bool()>& done, int timeoutMs = kWaitMs) {
  QDeadlineTimer deadline(timeoutMs);
  while (!done()) {
    if (deadline.hasExpired()) return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, kPollMs);
  }
  return true;
}

// Lets timers and queued work of the moment run: a short bounded spin.
inline void settle(int ms = 30) {
  QDeadlineTimer deadline(ms);
  while (!deadline.hasExpired()) QCoreApplication::processEvents(QEventLoop::AllEvents, kPollMs);
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
