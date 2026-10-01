// Input for `moc`, used ONLY to read the shape of the Qt 6.10 output.
// Nothing in reflect-moc compiles this header's moc output.
#pragma once
#include <QObject>

class Counter : public QObject {
  Q_OBJECT
  Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)

 public:
  int value() const { return value_; }
  void setValue(int v);
  Q_INVOKABLE int add(int n);

 public slots:
  void reset();

 signals:
  void valueChanged(int value);

 private:
  int value_ = 0;
};
