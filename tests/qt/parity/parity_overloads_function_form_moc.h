// The oracle: overloaded signals, slots and invokables, processed by the real moc.
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

namespace viamoc {

class Overloaded : public QObject {
  Q_OBJECT
  Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged)

 public:
  explicit Overloaded(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }
  QString label() const { return label_; }
  Q_INVOKABLE int twice(int n) const { return 2 * n; }
  Q_INVOKABLE QString twice(const QString& text) const { return text + text; }

 public slots:
  void setValue(int v) {
    value_ = v;
    emit valueChanged(v);
  }
  void setValue(const QString& text) {
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
