// The oracle: enums and flags registered with Q_ENUM and Q_FLAG, and properties of those types, processed by the real moc.
#pragma once

#include <QtCore/QObject>

namespace viamoc {

class Colored : public QObject {
  Q_OBJECT
  Q_PROPERTY(Mode mode READ mode WRITE setMode)
  Q_PROPERTY(Options options READ options WRITE setOptions)
  Q_PROPERTY(Level level READ level)

 public:
  enum class Level : short { Low, High };
  Q_ENUM(Level)
  enum Mode { Day, Night = 5 };
  Q_ENUM(Mode)
  enum Option { Bold = 1, Italic = 2, Underline = 4 };
  Q_DECLARE_FLAGS(Options, Option)
  Q_FLAG(Options)

  explicit Colored(QObject* parent = nullptr) : QObject(parent) {}
  Mode mode() const { return mode_; }
  void setMode(Mode m) { mode_ = m; }
  Options options() const { return options_; }
  void setOptions(Options o) { options_ = o; }
  Level level() const { return Level::High; }

 private:
  Mode mode_ = Day;
  Options options_;
};

}  // namespace viamoc
