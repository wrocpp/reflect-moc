// The oracle: Q_REVISION and Q_SCRIPTABLE on a slot, an invokable and a signal, processed by the real moc.
#pragma once

#include <QtCore/QObject>

namespace viamoc {

class Flagged : public QObject {
  Q_OBJECT

 public:
  explicit Flagged(QObject* parent = nullptr) : QObject(parent) {}
  Q_INVOKABLE Q_SCRIPTABLE void scripted() {}
  Q_INVOKABLE Q_REVISION(3, 4) int versioned(int a) { return a + 1; }
  Q_INVOKABLE Q_REVISION(5, 6) Q_SCRIPTABLE void both(int a, int b = 2) { Q_UNUSED(a) Q_UNUSED(b) }
  void fireData(int v) { emit dataSignal(v); }
  void fireFunction(int v) { emit functionSignal(v); }

 public slots:
  Q_REVISION(1, 2) Q_SCRIPTABLE void slot(int x, int y = 3) { last = x + y; }
  Q_SCRIPTABLE void scriptedSlot() {}

 signals:
  Q_REVISION(4, 5) Q_SCRIPTABLE void dataSignal(int v);
  Q_REVISION(6, 7) void functionSignal(int v);
  Q_SCRIPTABLE void scriptedSignal();

 public:
  int last = 0;
};

}  // namespace viamoc
