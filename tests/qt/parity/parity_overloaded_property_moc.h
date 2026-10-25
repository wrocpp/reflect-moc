// The oracle: properties whose WRITE, RESET and NOTIFY name overloaded functions. moc binds each by C++ overload resolution
// against the property's type (WRITE), by the empty argument list (RESET) and to the first signal of the name (NOTIFY).
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace viamoc {

class Overloaded : public QObject {
  Q_OBJECT
  Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
  Q_PROPERTY(int count READ count WRITE setCount)
  Q_PROPERTY(QString label READ label WRITE setLabel RESET resetLabel)

 public:
  explicit Overloaded(QObject* parent = nullptr) : QObject(parent) {}
  double value() const { return value_; }
  int count() const { return count_; }
  QString label() const { return label_; }
  QByteArray used() const { return used_; }

  // the int overload first: a double property must still be written by the double one
  void setValue(int v) { store(v, "setValue(int)"); }
  void setValue(double v) { store(v, "setValue(double)"); }
  // the double overload first: an int property is written by the int one, which takes it exactly
  void setCount(double v) { count_ = static_cast<int>(v) + 1000, used_ = "setCount(double)"; }
  void setCount(int v) { count_ = v, used_ = "setCount(int)"; }
  void setLabel(const QString& text) {
    label_ = text;
    used_ = "setLabel";
  }
  void resetLabel(int) { used_ = "resetLabel(int)"; }  // takes an argument: not a RESET
  void resetLabel() {
    label_ = QStringLiteral("reset");
    used_ = "resetLabel()";
  }

 signals:
  void valueChanged(int value);
  void valueChanged(double value);

 private:
  void store(double v, char const* by) {
    value_ = v;
    used_ = by;
    emit valueChanged(v);
  }
  double value_ = 0;
  int count_ = 0;
  QString label_;
  QByteArray used_;
};

}  // namespace viamoc
