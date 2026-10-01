// Non-builtin types in signals and slots (QImage, QList<T>, user structs, enums)
// are written the way moc writes them, and queued connections of them across a
// QThread work through the registered argument metatypes.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QList>
#include <QtCore/QMetaMethod>
#include <QtCore/QSemaphore>
#include <QtCore/QThread>
#include <QtGui/QImage>

namespace shapes {
struct Point {
  int x = 0;
  int y = 0;
  bool operator==(Point const&) const = default;
};
}  // namespace shapes

namespace {
constexpr int kX = 3;
constexpr int kY = 4;
constexpr int kImageSide = 6;
constexpr int kListA = 10;
constexpr int kListB = 20;
}  // namespace

struct Camera : rqt::Object<> {
  Camera() { bind(); }
  [[= rqt::signal]] void frame(QImage const& image, QList<int> const& marks) { rqt::emit{this}(image, marks); }
  [[= rqt::signal]] void moved(shapes::Point const& to) { rqt::emit{this}(to); }
  [[= rqt::slot]] void shoot() {
    QImage image{kImageSide, kImageSide, QImage::Format_RGB32};
    frame(image, QList<int>{kListA, kListB});
    moved(shapes::Point{.x = kX, .y = kY});
  }
};

struct Screen : rqt::Object<> {
  Screen() { bind(); }
  QSemaphore done;
  QThread* thread_seen = nullptr;
  int image_side = 0;
  QList<int> marks;
  shapes::Point at;
  [[= rqt::slot]] void show(QImage const& image, QList<int> const& m) {
    image_side = image.width();
    marks = m;
    thread_seen = QThread::currentThread();
    done.release();
  }
  [[= rqt::slot]] void place(shapes::Point const& p) {
    at = p;
    done.release();
  }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);

  // the table: type names for what is not built in, ids for what is
  QMetaObject const& mo = rqt::static_meta_object<Camera>;
  QMetaMethod const frame = mo.method(mo.indexOfMethod("frame(QImage,QList<int>)"));
  CHECK(frame.isValid());
  CHECK(QByteArray{frame.parameterTypeName(0)} == "QImage");
  CHECK(QByteArray{frame.parameterTypeName(1)} == "QList<int>");
  CHECK(frame.parameterMetaType(1) == QMetaType::fromType<QList<int>>());
  QMetaMethod const moved = mo.method(mo.indexOfMethod("moved(shapes::Point)"));
  CHECK(moved.isValid());
  CHECK(moved.parameterMetaType(0) == QMetaType::fromType<shapes::Point>());

  Camera camera;
  Screen screen;
  QThread thread;
  screen.moveToThread(&thread);
  thread.start();

  CHECK(QObject::connect(&camera, SIGNAL(frame(QImage,QList<int>)), &screen, SLOT(show(QImage,QList<int>)),
                         Qt::QueuedConnection));
  CHECK(QObject::connect(&camera, SIGNAL(moved(shapes::Point)), &screen, SLOT(place(shapes::Point)),
                         Qt::QueuedConnection));
  camera.shoot();
  screen.done.acquire();
  screen.done.acquire();
  CHECK(screen.image_side == kImageSide);
  CHECK(screen.marks == (QList<int>{kListA, kListB}));
  CHECK(screen.thread_seen == &thread);
  CHECK(screen.at == (shapes::Point{.x = kX, .y = kY}));

  thread.quit();
  thread.wait();
  return rqt_test::finish("capability_custom_metatype_queued");
}
