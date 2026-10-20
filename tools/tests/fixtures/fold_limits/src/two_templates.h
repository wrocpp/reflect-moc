// Two templates derive from the base: the signals cannot move into both.
#pragma once
#include <QtCore/QObject>
#include <QtCore/QVariant>

class SharedSignals : public QObject {
  Q_OBJECT
 signals:
  void changed(const QVariant& key);
};

template <class T>
class FirstHolder : public SharedSignals {
 public:
  using Key = T;
};

template <class T>
class SecondHolder : public SharedSignals {
 public:
  using Key = T;
};
