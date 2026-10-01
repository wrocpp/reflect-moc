// A property's NOTIFY signal is linked in the meta-object, and a write through
// Qt emits it for both property styles, once and only when the value changed.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaMethod>
#include <QtCore/QMetaProperty>
#include <QtCore/QString>

namespace {
constexpr int kFirst = 3;
constexpr int kSecond = 9;
}  // namespace

struct Meter : rqt::Object<> {
  Meter() { bind(); }

  [[= rqt::property{.write = "setLevel", .notify = "levelChanged"}]] int level() const { return level_; }
  [[= rqt::property{.notify = "labelChanged"}]] QString label;

  [[= rqt::signal_function]] void levelChanged(int level) { rqt::emit{this}(level); }
  [[= rqt::signal_function]] void labelChanged(QString const& label) { rqt::emit{this}(label); }
  [[= rqt::slot]] void setLevel(int l) {
    if (l == level_) return;
    level_ = l;
    levelChanged(l);
  }

  int level_ = 0;
};

struct Counter : rqt::Object<> {
  Counter() { bind(); }
  int levels = 0;
  int labels = 0;
  int last_level = 0;
  QString last_label;
  [[= rqt::slot]] void onLevel(int l) {
    ++levels;
    last_level = l;
  }
  [[= rqt::slot]] void onLabel(QString const& l) {
    ++labels;
    last_label = l;
  }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  Meter meter;
  Counter counter;
  QMetaObject const* mo = meter.metaObject();

  QMetaProperty const level = mo->property(mo->indexOfProperty("level"));
  QMetaProperty const label = mo->property(mo->indexOfProperty("label"));
  CHECK(level.hasNotifySignal() && level.notifySignal().name() == "levelChanged");
  CHECK(label.hasNotifySignal() && label.notifySignal().name() == "labelChanged");
  CHECK(label.notifySignal().methodSignature() == "labelChanged(QString)");

  CHECK(QObject::connect(&meter, SIGNAL(levelChanged(int)), &counter, SLOT(onLevel(int))));
  CHECK(QObject::connect(&meter, SIGNAL(labelChanged(QString)), &counter, SLOT(onLabel(QString))));

  // accessor style: the setter emits
  CHECK(level.write(&meter, kFirst));
  CHECK(counter.levels == 1 && counter.last_level == kFirst);
  CHECK(level.write(&meter, kFirst));
  CHECK(counter.levels == 1);

  // MEMBER style: the write emits the NOTIFY signal with the new value
  CHECK(label.write(&meter, QString{"warm"}));
  CHECK(counter.labels == 1 && counter.last_label == "warm");
  CHECK(label.write(&meter, QString{"warm"}));
  CHECK(counter.labels == 1);
  CHECK(label.write(&meter, QString{"cold"}) && counter.labels == 2);

  CHECK(meter.setProperty("level", kSecond) && counter.levels == 2 && counter.last_level == kSecond);
  return rqt_test::finish("capability_notify_signal_linked");
}
