// reflect-moc behaviour test, not part of the upstream example.
//
// Drives the Sliders window offscreen through its own controls and prints
// every SlidersGroup::valueChanged emission and the resulting widget state.
// It covers the custom signal, the public slots reached by pointer-to-member
// connect, invokeMethod by name, string-based SIGNAL/SLOT connect, and the
// resize handler that calls a slot directly.
#include "meta_dump.hpp"
#include "slidersgroup.h"
#include "window.h"

#include <QAbstractSlider>
#include <QApplication>
#include <QCheckBox>
#include <QDial>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>

#include <cstdio>

namespace {

struct Probe {
  QSlider* slider;
  QScrollBar* scroll_bar;
  QDial* dial;
  QSpinBox* minimum;
  QSpinBox* maximum;
  QSpinBox* value;

  void print(char const* step) const {
    std::printf("%-28s slider=%d scroll=%d dial=%d range=[%d,%d] spin=%d inverted=%d keys=%d vertical=%d\n", step,
                slider->value(), scroll_bar->value(), dial->value(), slider->minimum(), slider->maximum(),
                value->value(), slider->invertedAppearance(), slider->invertedControls(),
                slider->orientation() == Qt::Vertical);
    std::fflush(stdout);
  }
};

}  // namespace

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  harness::dump_meta(&SlidersGroup::staticMetaObject);
  harness::dump_meta(&Window::staticMetaObject);

  Window window;
  window.resize(640, 320);
  window.show();

  auto* group = window.findChild<SlidersGroup*>();
  auto spins = window.findChildren<QSpinBox*>();
  auto boxes = window.findChildren<QCheckBox*>();
  if (!group || spins.size() != 3 || boxes.size() != 2) {
    std::puts("FAIL: window layout not found");
    return 1;
  }
  Probe probe{group->findChild<QSlider*>(), group->findChild<QScrollBar*>(), group->findChild<QDial*>(),
              spins[0], spins[1], spins[2]};
  QObject::connect(group, &SlidersGroup::valueChanged, [](int v) { std::printf("  valueChanged(%d)\n", v); });

  probe.print("initial");
  probe.value->setValue(12);
  probe.print("spin value 12");
  probe.dial->setValue(3);
  probe.print("dial 3");
  probe.minimum->setValue(5);
  probe.print("minimum 5");
  probe.maximum->setValue(8);
  probe.print("maximum 8");
  boxes[0]->setChecked(true);
  boxes[1]->setChecked(true);
  probe.print("both boxes checked");

  bool ok = QMetaObject::invokeMethod(group, "setValue", Q_ARG(int, 7));
  probe.print(ok ? "invokeMethod setValue(7)" : "invokeMethod FAILED");

  QSpinBox mirror;
  mirror.setRange(-100, 100);
  ok = QObject::connect(group, SIGNAL(valueChanged(int)), &mirror, SLOT(setValue(int)));
  probe.slider->setValue(6);
  std::printf("string connect %s, mirror=%d\n", ok ? "ok" : "FAILED", mirror.value());

  window.resize(320, 640);
  probe.print("resized to portrait");
  window.resize(640, 320);
  probe.print("resized to landscape");

  std::printf("qobject_cast<SlidersGroup*> %s\n",
              qobject_cast<SlidersGroup*>(static_cast<QObject*>(group)) == group ? "ok" : "FAILED");
  return 0;
}
