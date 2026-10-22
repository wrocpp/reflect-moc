// Helpers of the data and type edge tests: read a model back through its key role, and build the models.
#pragma once

#include "check.hpp"

#include <container_model.hpp>

#include <QtCore/QList>
#include <QtCore/QVariant>
#include <QtTest/QAbstractItemModelTester>

#include <csignal>
#include <cstdio>
#include <memory>

#include <sys/wait.h>
#include <unistd.h>

namespace edge_data {

constexpr auto kFatal = QAbstractItemModelTester::FailureReportingMode::Fatal;

// A model of a container with a formatter that shows the value as text; the tester runs in fatal mode.
template <class C>
struct Rig {
  using Source = observable::ObservableContainer<C>;
  using Key = typename Source::Key;
  using Value = typename Source::Value;

  explicit Rig(C content = {}) : source(std::move(content)), model(&source, [](const Key&, const Value&) { return QVariant(); }),
                                  tester(&model, kFatal) {}

  QList<Key> keys() const {
    QList<Key> out;
    for (int row = 0; row < model.rowCount(); ++row) out.append(model.index(row, 0).data(Qt::UserRole).template value<Key>());
    return out;
  }

  Source source;
  observable::ContainerModel<C> model;
  QAbstractItemModelTester tester;
};

// Runs `scenario` in a child process and reports whether it died of an assertion (SIGABRT).
template <class F>
bool aborts(F&& scenario) {
  std::fflush(nullptr);
  const pid_t child = fork();
  if (child == 0) {
    if (!std::freopen("/dev/null", "w", stderr)) _exit(2);
    scenario();
    _exit(0);
  }
  int status = 0;
  waitpid(child, &status, 0);
  return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

}  // namespace edge_data
