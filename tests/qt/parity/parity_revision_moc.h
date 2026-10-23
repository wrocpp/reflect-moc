// The oracle: properties with a revision, processed by the real moc (it accepts only REVISION(major, minor)).
#pragma once

#include <QtCore/QObject>

namespace viamoc {

class Versioned : public QObject {
  Q_OBJECT
  Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged REVISION(2, 3))
  Q_PROPERTY(int plain READ plain)
  Q_PROPERTY(int fourth MEMBER fourth_ REVISION(4, 0))
  Q_PROPERTY(int third READ third REVISION(1, 2))

 public:
  explicit Versioned(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }
  int plain() const { return 0; }
  int third() const { return 0; }

 public slots:
  void setValue(int v) {
    value_ = v;
    emit valueChanged(v);
  }

 signals:
  void valueChanged(int value);

 private:
  int value_ = 0;
  int fourth_ = 0;
};

}  // namespace viamoc
