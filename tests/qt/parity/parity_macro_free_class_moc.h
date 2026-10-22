// The oracle: a class with properties, slots, an invokable, function signals, an enum and class info, and a derived
// class, processed by the real moc.
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

namespace viamoc {

class Gauge : public QObject {
  Q_OBJECT
  Q_CLASSINFO("author", "wro.cpp")
  Q_PROPERTY(int level READ level WRITE setLevel NOTIFY levelChanged)
  Q_PROPERTY(Mode mode READ mode WRITE setMode)

 public:
  enum class Mode { off, on };
  Q_ENUM(Mode)
  explicit Gauge(QObject* parent = nullptr) : QObject(parent) {}
  int level() const { return level_; }
  Mode mode() const { return mode_; }
  void setMode(Mode m) { mode_ = m; }
  Q_INVOKABLE int twice(int n) const { return 2 * n; }

 public slots:
  void setLevel(int l) {
    if (l == level_) return;
    level_ = l;
    emit levelChanged(l);
  }

 signals:
  void levelChanged(int level);

 private:
  int level_ = 0;
  Mode mode_ = Mode::off;
};

class FineGauge : public Gauge {
  Q_OBJECT
  Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY unitChanged)

 public:
  explicit FineGauge(QObject* parent = nullptr) : Gauge(parent) {}
  QString unit() const { return unit_; }

 public slots:
  void setUnit(const QString& u) {
    unit_ = u;
    emit unitChanged(u);
  }
  void nudge() { setLevel(level() + 1); }

 signals:
  void unitChanged(const QString& unit);

 private:
  QString unit_;
};

}  // namespace viamoc
