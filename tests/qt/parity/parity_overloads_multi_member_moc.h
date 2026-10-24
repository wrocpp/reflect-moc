// The oracle for a signal member with several signatures: two overloaded signals processed by the real moc.
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

namespace viamoc {

class MultiSignal : public QObject {
  Q_OBJECT
  Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)

 public:
  explicit MultiSignal(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }
  QString label() const { return label_; }

 public slots:
  void setValue(int v) {
    value_ = v;
    emit valueChanged(v);
  }
  void setLabel(const QString& text) {
    label_ = text;
    emit valueChanged(text);
  }

 signals:
  void valueChanged(int value);
  void valueChanged(const QString& text);

 private:
  int value_ = 0;
  QString label_;
};

}  // namespace viamoc
