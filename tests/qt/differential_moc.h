// The oracle: the classes written the way Qt wants them, processed by the real moc (AUTOMOC
// is on for this one test target only). differential_rqt.hpp declares the same classes for
// reflect-moc, member for member.
#pragma once

#include "differential_interface.hpp"

#include <QtCore/QObject>
#include <QtCore/QString>

namespace viamoc {

class Plugin : public QObject, public DiffGreeter {
  Q_OBJECT
  Q_INTERFACES(DiffGreeter)

 public:
  explicit Plugin(QObject* parent = nullptr) : QObject(parent) {}
  QString greet() const override { return "hi"; }
};

class Base : public QObject {
  Q_OBJECT
  Q_CLASSINFO("author", "me")
  Q_CLASSINFO("version", "2")
  Q_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged RESET reset FINAL)
  Q_PROPERTY(QString label MEMBER label_ NOTIFY labelChanged)
  Q_PROPERTY(Mode mode READ mode WRITE setMode)
  Q_PROPERTY(int fixed READ fixed CONSTANT)
  Q_PROPERTY(QObject* peer READ peer WRITE adopt)
  Q_PROPERTY(int hiddenFlags READ value DESIGNABLE false SCRIPTABLE false STORED false USER true REQUIRED)

 public:
  enum class Mode { Linear, Log };
  Q_ENUM(Mode)
  enum Edge { Left = 1, Right = 2 };
  Q_DECLARE_FLAGS(Edges, Edge)
  Q_FLAG(Edges)

  explicit Base(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }
  Mode mode() const { return mode_; }
  int fixed() const { return 3; }
  QObject* peer() const { return peer_; }
  Q_INVOKABLE int twice(int n) const { return 2 * n; }
  Q_INVOKABLE void scaled(double factor, const QString& name = QString()) {
    value_ = static_cast<int>(factor) + name.size();
  }

 public slots:
  void setValue(int v) {
    if (v == value_) return;
    value_ = v;
    emit valueChanged(v);
  }
  void reset() { setValue(0); }
  void setLabel(const QString& l) {
    label_ = l;
    emit labelChanged(l);
  }
  void setMode(Mode m) { mode_ = m; }
  void adopt(QObject* o) { peer_ = o; }
  void step(int by, int times = 1) { value_ += by * times; }

 signals:
  void valueChanged(int value);
  void labelChanged(const QString& label);
  void moved(int from, int to);
  void done();

 private:
  int value_ = 0;
  QString label_;
  Mode mode_ = Mode::Linear;
  QObject* peer_ = nullptr;
};

class Derived : public Base {
  Q_OBJECT
  Q_PROPERTY(int extra READ extra WRITE setExtra NOTIFY extraChanged)

 public:
  explicit Derived(QObject* parent = nullptr) : Base(parent) {}
  int extra() const { return extra_; }
  Q_INVOKABLE QString describe() const { return QString{"derived"}; }

 public slots:
  void setExtra(int e) {
    extra_ = e;
    emit extraChanged(e);
  }

 signals:
  void extraChanged(int extra);

 private slots:
  void hiddenSlot() {}

 private:
  int extra_ = 0;
};

}  // namespace viamoc
