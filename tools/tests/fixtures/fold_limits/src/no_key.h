// A folded template needs the Key alias to type the signals with.
#pragma once
#include <QtCore/QObject>
#include <QtCore/QVariant>

class NoKeySignals : public QObject {
  Q_OBJECT
 signals:
  void changed(const QVariant& key);
};

template <class T>
class NoKeyHolder : public NoKeySignals {
 public:
  void touch(const T& value) { emit changed(QVariant::fromValue(value)); }
};
