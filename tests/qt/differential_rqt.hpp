// The same classes as differential_moc.h, written for reflect-moc: RQT_OBJECT, RQT_PROPERTY with the
// same text, [[=rqt::slot]] and [[=rqt::invokable]] annotations, and signals as data members.
#pragma once

#include "differential_interface.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QObject>
#include <QtCore/QString>

namespace viarqt {

class Plugin : public QObject, public DiffGreeter {
  RQT_OBJECT
  Q_INTERFACES(DiffGreeter)  // an empty macro without moc: the interface needs no line

 public:
  explicit Plugin(QObject* parent = nullptr) : QObject(parent) {}
  QString greet() const override { return "hi"; }
};

class Base : public QObject {
  RQT_OBJECT
  RQT_CLASSINFO("author", "me")
  RQT_CLASSINFO("version", "2")
  RQT_PROPERTY(int value READ value WRITE setValue NOTIFY valueChanged RESET reset FINAL)
  RQT_PROPERTY(QString label MEMBER label_ NOTIFY labelChanged)
  RQT_PROPERTY(Mode mode READ mode WRITE setMode)
  RQT_PROPERTY(int fixed READ fixed CONSTANT)
  RQT_PROPERTY(QObject* peer READ peer WRITE adopt)
  RQT_PROPERTY(int hiddenFlags READ value DESIGNABLE false SCRIPTABLE false STORED false USER true REQUIRED)

 public:
  enum class Mode { Linear, Log };
  RQT_ENUM(Mode)
  enum Edge { Left = 1, Right = 2 };
  Q_DECLARE_FLAGS(Edges, Edge)
  RQT_FLAG(Edges)

  explicit Base(QObject* parent = nullptr) : QObject(parent) {}
  int value() const { return value_; }
  Mode mode() const { return mode_; }
  int fixed() const { return 3; }
  QObject* peer() const { return peer_; }
  [[= rqt::invokable]] int twice(int n) const { return 2 * n; }
  [[= rqt::invokable]] void scaled(double factor, const QString& name = QString()) {
    value_ = static_cast<int>(factor) + name.size();
  }

 public slots:
  [[= rqt::slot]] void setValue(int v) {
    if (v == value_) return;
    value_ = v;
    emit valueChanged(v);
  }
  [[= rqt::slot]] void reset() { setValue(0); }
  [[= rqt::slot]] void setLabel(const QString& l) {
    label_ = l;
    emit labelChanged(l);
  }
  [[= rqt::slot]] void setMode(Mode m) { mode_ = m; }
  [[= rqt::slot]] void adopt(QObject* o) { peer_ = o; }
  [[= rqt::slot]] void step(int by, int times = 1) { value_ += by * times; }

 signals:
  [[= rqt::names("value")]] rqt::signal<void(int)> valueChanged;
  [[= rqt::names("label")]] rqt::signal<void(const QString&)> labelChanged;
  [[= rqt::names("from, to")]] rqt::signal<void(int, int)> moved;
  rqt::signal<void()> done;

 private:
  int value_ = 0;
  QString label_;
  Mode mode_ = Mode::Linear;
  QObject* peer_ = nullptr;
};

class Derived : public Base {
  RQT_OBJECT
  RQT_PROPERTY(int extra READ extra WRITE setExtra NOTIFY extraChanged)

 public:
  explicit Derived(QObject* parent = nullptr) : Base(parent) {}
  int extra() const { return extra_; }
  [[= rqt::invokable]] QString describe() const { return QString{"derived"}; }

 public slots:
  [[= rqt::slot]] void setExtra(int e) {
    extra_ = e;
    emit extraChanged(e);
  }

 signals:
  [[= rqt::names("extra")]] rqt::signal<void(int)> extraChanged;

 private slots:
  [[= rqt::slot]] void hiddenSlot() {}

 private:
  int extra_ = 0;
};

// signals as static members: they take no space, and RQT_OBJECT_STATIC leaves out the anchor
class StaticSignals : public QObject {
  RQT_OBJECT_STATIC
  RQT_PROPERTY(int level READ level WRITE setLevel NOTIFY levelChanged)

 public:
  explicit StaticSignals(QObject* parent = nullptr) : QObject(parent) {}
  int level() const { return level_; }

 public slots:
  [[= rqt::slot]] void setLevel(int v) {
    level_ = v;
    levelChanged(v).from(this);
  }
  [[= rqt::slot]] void reset() { setLevel(0); }

 signals:
  [[= rqt::names("level")]] static inline rqt::static_signal<void(int)> levelChanged{};
  [[= rqt::names("from, to")]] static inline rqt::static_signal<void(int, int)> moved{};
  static inline rqt::static_signal<void()> done{};
  [[= rqt::names("label")]] static inline rqt::static_signal<void(const QString&)> labelled{};

 private:
  int level_ = 0;
};

class DerivedStaticSignals : public StaticSignals {
  RQT_OBJECT_STATIC

 public:
  explicit DerivedStaticSignals(QObject* parent = nullptr) : StaticSignals(parent) {}
  [[= rqt::invokable]] int twice(int n) const { return 2 * n; }

 signals:
  [[= rqt::names("extra")]] static inline rqt::static_signal<void(int)> extraChanged{};
};

}  // namespace viarqt
