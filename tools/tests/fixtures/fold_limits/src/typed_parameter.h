// A signal that already carries a typed parameter has no key to unbox.
#pragma once
#include <QtCore/QObject>
#include <QtCore/QVariant>

class MixedSignals : public QObject {
  Q_OBJECT
 signals:
  void resized(int size);
};

template <class T>
class MixedHolder : public MixedSignals {
 public:
  using Key = T;
};
