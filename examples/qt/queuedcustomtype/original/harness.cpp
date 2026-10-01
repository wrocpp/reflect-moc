// reflect-moc behaviour test, not part of the upstream example.
//
// Drives the Queued Custom Type window offscreen. The render thread emits
// sendBlock(const Block &) from its own thread, so every block crosses
// threads as a queued connection carrying the custom metatype. Run 1 lets
// the thread finish; run 2 presses Stop after a few blocks. The block
// positions are random, so the harness prints counts, threads and button
// states, which do not depend on them.
#include "block.h"
#include "meta_dump.hpp"
#include "renderthread.h"
#include "window.h"

#include <QApplication>
#include <QImage>
#include <QPushButton>
#include <QThread>

#include <cstdio>

namespace {

// RenderThread::run emits 400 blocks per block size, and the image's
// twentieth is the largest size: 20 px gives one size, so 400 blocks.
constexpr int image_side = 20;
constexpr int blocks_per_size = 400;
constexpr int stop_after = 25;

QImage make_image() {
  QImage image(image_side, image_side, QImage::Format_RGB32);
  for (int y = 0; y < image_side; ++y)
    for (int x = 0; x < image_side; ++x) image.setPixel(x, y, qRgb(x * 12, y * 12, 128));
  return image;
}

}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  qRegisterMetaType<Block>();
  harness::dump_meta(&RenderThread::staticMetaObject);
  harness::dump_meta(&Window::staticMetaObject);

  Window window;
  window.show();
  auto* thread = window.findChild<RenderThread*>();
  auto buttons = window.findChildren<QPushButton*>();
  if (!thread || buttons.size() != 2) {
    std::puts("FAIL: window layout not found");
    return 1;
  }
  QPushButton* load = buttons[0];
  QPushButton* stop = buttons[1];
  auto print_buttons = [&](char const* step) {
    std::printf("%-22s load=%s stop=%s\n", step, load->isEnabled() ? "enabled" : "disabled",
                stop->isEnabled() ? "enabled" : "disabled");
    std::fflush(stdout);
  };

  int blocks = 0;
  int off_thread = 0;
  int outside = 0;
  bool stop_early = false;
  QObject::connect(thread, &RenderThread::sendBlock, &app, [&](Block const& block) {
    if (QThread::currentThread() != app.thread()) ++off_thread;
    if (!QRect(0, 0, image_side + 1, image_side + 1).contains(block.rect())) ++outside;
    if (++blocks == stop_after && stop_early) stop->click();
  });
  QObject::connect(thread, &QThread::finished, &app, &QCoreApplication::quit, Qt::QueuedConnection);

  print_buttons("initial");
  window.loadImage(make_image());
  print_buttons("run 1 started");
  app.exec();
  print_buttons("run 1 finished");
  std::printf("run 1: %d blocks (expected %d), %d off the GUI thread, %d outside the image\n", blocks,
              blocks_per_size, off_thread, outside);
  int const first = blocks;

  blocks = 0;
  stop_early = true;
  window.loadImage(make_image());
  app.exec();
  print_buttons("run 2 stopped");
  std::printf("run 2: stopped early %s, %d off the GUI thread\n",
              blocks >= stop_after && blocks < blocks_per_size ? "yes" : "NO", off_thread);

  QMetaObject::invokeMethod(&window, "resetUi");
  print_buttons("invokeMethod resetUi");
  return first == blocks_per_size && off_thread == 0 && outside == 0 ? 0 : 1;
}
