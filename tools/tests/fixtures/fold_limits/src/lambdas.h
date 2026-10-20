// The shapes of a lambda parameter that carried a QVariant key.
#pragma once
#include <QtCore/QObject>
#include <QtCore/QVariant>

class LambdaSignals : public QObject {
  Q_OBJECT
 signals:
  void keyed(const QVariant& key);
};

template <class T>
class LambdaHolder : public LambdaSignals {
 public:
  using Key = T;
};

class KeyedListener : public QObject {
 public:
  using Key = int;

  template <class T>
  void listen(LambdaHolder<T>* holder) {
    using Holder = LambdaHolder<T>;
    connect(holder, &Holder::keyed, this, [this](const QVariant& k) { use(k.value<Key>()); use(k.value<Key>() + 1); });
    connect(holder, &Holder::keyed, this, [this](const QVariant& ignored) { use(0); });
    connect(holder, &Holder::keyed, this, [this](const QVariant&) mutable { use(1); });
    connect(holder, &Holder::keyed, this, [this](const QVariant& k) { use(k.value<int>() + k.toInt()); });
    connect(holder, &Holder::keyed, this, [this](const QVariant& k) {
      const long n = k.value<int>();
      use(n);
    });
  }

  void use(int);
};

class BareListener : public QObject {
 public:
  template <class T>
  void listen(LambdaHolder<T>* holder) {
    using Holder = LambdaHolder<T>;
    connect(holder, &Holder::keyed, this, [this](const QVariant&) { use(2); });
  }

  void use(int);
};
