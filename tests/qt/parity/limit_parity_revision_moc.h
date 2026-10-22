// The oracle: a property with a revision, processed by the real moc.
#pragma once

#include <QtCore/QObject>

namespace viamoc {

class Versioned : public QObject {
  Q_OBJECT
  Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged REVISION(2, 3))
  Q_PROPERTY(int plain READ plain)

 public:
  explicit Versioned(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }
  int plain() const { return 0; }
  void setValue(int v) {
    value_ = v;
    emit valueChanged(v);
  }

 signals:
  void valueChanged(int value);

 private:
  int value_ = 0;
};

}  // namespace viamoc
