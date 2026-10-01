// reflect-moc behaviour test, not part of the upstream example.
//
// Drives RenderThread headless: two render requests, each rendered in every
// pass, delivered to the GUI thread through the queued renderedImage(QImage,
// double) signal. Prints each image's size, scale, pass text (without its
// timing) and a checksum of its pixels, so the moc and reflect-moc builds can
// be compared byte for byte.
#include "meta_dump.hpp"
#include "renderthread.h"

#include <QGuiApplication>
#include <QImage>
#include <QThread>

#include <cstdint>
#include <cstdio>

namespace {

constexpr int passes = 4;
constexpr QSize image_size{160, 120};

// The widget's defaults (mandelbrotwidget.cpp), then a 2x zoom.
constexpr double center_x = -0.637011;
constexpr double center_y = -0.0395159;
constexpr double scale = 0.00403897;
constexpr double zoom = 0.5;

std::uint64_t fnv1a(QImage const& image) {
  std::uint64_t h = 14695981039346656037ull;
  for (qsizetype i = 0; i < image.sizeInBytes(); ++i) {
    h ^= image.constBits()[i];
    h *= 1099511628211ull;
  }
  return h;
}

}  // namespace

int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  harness::dump_meta(&RenderThread::staticMetaObject);

  RenderThread::setNumPasses(passes);
  RenderThread thread;
  int received = 0;
  int off_thread = 0;
  QObject::connect(&thread, &RenderThread::renderedImage, &app, [&](QImage const& image, double scaleFactor) {
    if (QThread::currentThread() != app.thread()) ++off_thread;
    QString info = image.text(RenderThread::infoKey());
    info.truncate(info.indexOf(u", time:"));
    std::printf("image %d: %dx%d scale=%.8g%s checksum=%016llx\n", received, image.width(), image.height(),
                scaleFactor, info.toUtf8().constData(), static_cast<unsigned long long>(fnv1a(image)));
    std::fflush(stdout);
    ++received;
    if (received == passes) thread.render(center_x, center_y, scale * zoom, image_size, 1.0);
    if (received == 2 * passes) app.quit();
  });

  thread.render(center_x, center_y, scale, image_size, 1.0);
  app.exec();
  std::printf("received %d images, %d delivered off the GUI thread\n", received, off_thread);
  return received == 2 * passes && off_thread == 0 ? 0 : 1;
}
