// A folded pair whose sites are not all rewritable.
#pragma once
#include <QtCore/QObject>
#include <QtCore/QVariant>

class SiteSignals : public QObject {
  Q_OBJECT
 signals:
  void added(const QVariant& key);
  void removed(const QVariant& key);
  void moved(const QVariant& from, const QVariant& to);
};

template <class T>
class SiteHolder : public SiteSignals {
 public:
  using Key = T;
  using Self = SiteHolder<T>;

  void add(const Key& key) { emit added(QVariant::fromValue(key)); }
  void remove(const Key& key) { emit removed(QVariant(5)); }
};

class Watcher : public QObject {
 public:
  void onAdded(const QVariant& key);

  template <class T>
  void watch(SiteHolder<T>* holder) {
    using Holder = SiteHolder<T>;
    connect(holder, &Holder::added, this, &Watcher::onAdded);
    connect(holder, SIGNAL(removed(QVariant)), this, SLOT(onAdded(QVariant)));
    connect(holder, &Holder::moved, this, [this](const QVariant& from, const QVariant& to) {
      use(from.toInt() + to.toInt());
    });
  }

  template <class T>
  void watchBase(SiteHolder<T>* holder) {
    connect(holder, &SiteSignals::removed, this, [this](const QVariant&) {});
  }

  void use(int);
};
