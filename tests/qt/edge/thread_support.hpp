// Helpers of the thread edge tests: a container and its model on the heap, bounded waits, and a fork that reports
// whether a scenario died of an assertion.
#pragma once

#include "check.hpp"

#include <container_model.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QDeadlineTimer>
#include <QtCore/QMutex>
#include <QtCore/QPointer>
#include <QtCore/QThread>
#include <QtTest/QAbstractItemModelTester>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <functional>
#include <memory>

#include <sys/wait.h>
#include <unistd.h>

namespace edge_thread {

constexpr int kWaitMs = 3000;  // every wait is bounded by this
constexpr int kPollMs = 2;

using C = QMap<int, QString>;
using Source = observable::ObservableContainer<C>;
using Model = observable::ContainerModel<C>;

inline QVariant show(const int&, const QString& v) { return QVariant(v); }

// Runs the event loop of the calling thread until `done` or the bound; true when `done` became true.
inline bool wait_for(const std::function<bool()>& done, int timeoutMs = kWaitMs) {
  QDeadlineTimer deadline(timeoutMs);
  while (!done()) {
    if (deadline.hasExpired()) return false;
    QCoreApplication::processEvents(QEventLoop::AllEvents, kPollMs);
    QThread::msleep(kPollMs);
  }
  return true;
}

// Waits without an event loop (for a flag that another thread sets).
inline bool wait_flag(const std::atomic<bool>& flag, int timeoutMs = kWaitMs) {
  QDeadlineTimer deadline(timeoutMs);
  while (!flag.load()) {
    if (deadline.hasExpired()) return false;
    QThread::msleep(kPollMs);
  }
  return true;
}

// Quits a thread and waits for it, with a bound; false (after terminating it) when it did not stop.
inline bool stop(QThread& thread) {
  thread.quit();
  if (thread.wait(kWaitMs)) return true;
  thread.terminate();
  thread.wait(kWaitMs);
  return false;
}

// Runs `scenario` in a child process and reports whether it died of an assertion (SIGABRT). Call it while the
// process has no other thread.
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

}  // namespace edge_thread
