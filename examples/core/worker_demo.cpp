// A Worker on a background thread reports progress to a Ui on the main
// thread, with no moc and no Qt: C++26 reflection finds the signals.
//
//   Ui::startRequested ---queued--> Worker::start          (runs on the worker)
//   Worker.progress (NOTIFY) ---queued--> Ui::onProgress   (runs on main)
//   Worker::finished ---queued--> Ui::onFinished           (runs on main)
//
// Build: g++-16 -std=c++26 -freflection -pthread -Iinclude examples/core/worker_demo.cpp
// Compiler Explorer (GCC 16.2, g162): paste the generated single file
// <build>/examples/core/worker_demo_single_file.cpp, flags -std=c++26 -freflection -pthread.
#include <reflect_moc/core.hpp>

#include <print>
#include <string_view>
#include <thread>

namespace {

constexpr int not_started = -1;
constexpr int progress_step = 25;
constexpr int progress_done = 100;

std::thread::id main_thread;

std::string_view thread_label() { return std::this_thread::get_id() == main_thread ? "main" : "worker"; }

struct Worker : rqt::Object {
  [[= rqt::property{.notify = true}]] int progress = not_started;
  [[= rqt::signal]] void finished() { rqt::emit{this}(); }

  [[= rqt::slot]] void start() {
    std::println("start on {} thread", thread_label());
    for (int p = 0; p <= progress_done; p += progress_step) set<"progress">(p);
    finished();
  }
};

struct Ui : rqt::Object {
  [[= rqt::signal]] void startRequested() { rqt::emit{this}(); }

  [[= rqt::slot]] void onProgress(int value) { std::println("progress {} on {} thread", value, thread_label()); }
  [[= rqt::slot]] void onFinished() {
    std::println("finished on {} thread", thread_label());
    rqt::EventLoop::current()->quit();
  }
};

}  // namespace

int main() {
  main_thread = std::this_thread::get_id();
  rqt::EventLoop loop;
  Ui ui;
  Worker worker;
  std::jthread background{[] {
    rqt::EventLoop worker_loop;
    worker_loop.run();
  }};
  worker.move_to_thread(background.get_id());

  ui.connect<&Ui::startRequested>(worker, &Worker::start);
  rqt::connect<"progressChanged">(worker, ui, &Ui::onProgress);
  rqt::connect<"finished">(worker, ui, &Ui::onFinished);

  ui.startRequested();
  loop.run();
  rqt::post(background.get_id(), [] { rqt::EventLoop::current()->quit(); });
}
