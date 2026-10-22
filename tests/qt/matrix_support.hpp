// Helpers of the matrix tests: a matrix of strings and doubles, and a recorder connected to every signal of it.
#pragma once

#include "check.hpp"

#include <matrix_table_model.hpp>
#include <observable_matrix.hpp>

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtTest/QAbstractItemModelTester>
#include <QtTest/QSignalSpy>

#include <cmath>
#include <limits>

namespace matrix_test {

constexpr auto kTesterMode = QAbstractItemModelTester::FailureReportingMode::Fatal;
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

using Mat = observable::ObservableMatrix<QString, QString, double>;
using Cells = QList<std::tuple<QString, QString, double>>;

inline QString keysText(const QList<QString> & keys)
{
  return QLatin1Char('[') + keys.join(QLatin1Char(',')) + QLatin1Char(']');
}

// One object connected to EVERY signal of the matrix, appending the name and the payload.
class Recorder {
public:
  explicit Recorder(Mat & m)
  {
    const auto keyIndex = [&](const char * name, auto signal) {
      QObject::connect(&m, signal, &context, [this, name](const QString & k, qsizetype i) { log << QStringLiteral("%1(%2,%3)").arg(QLatin1String(name), k).arg(i); });
    };
    const auto range = [&](const char * name, auto signal) {
      QObject::connect(&m, signal, &context, [this, name](qsizetype f, qsizetype l, const QList<QString> & ks) {
        log << QStringLiteral("%1(%2,%3,%4)").arg(QLatin1String(name)).arg(f).arg(l).arg(keysText(ks));
      });
    };
    const auto rename = [&](const char * name, auto signal) {
      QObject::connect(&m, signal, &context, [this, name](const QString & a, const QString & b, qsizetype i) {
        log << QStringLiteral("%1(%2,%3,%4)").arg(QLatin1String(name), a, b).arg(i);
      });
    };
    const auto cell = [&](const char * name, auto signal) {
      QObject::connect(&m, signal, &context, [this, name](const QString & r, const QString & c, qsizetype i, qsizetype j) {
        log << QStringLiteral("%1(%2,%3,%4,%5)").arg(QLatin1String(name), r, c).arg(i).arg(j);
      });
    };
    const auto rect = [&](const char * name, auto signal) {
      QObject::connect(&m, signal, &context, [this, name](qsizetype t, qsizetype l, qsizetype b, qsizetype r) {
        log << QStringLiteral("%1(%2,%3,%4,%5)").arg(QLatin1String(name)).arg(t).arg(l).arg(b).arg(r);
      });
    };
    const auto none = [&](const char * name, auto signal) { QObject::connect(&m, signal, &context, [this, name] { log << QLatin1String(name); }); };

    keyIndex("rowAboutToBeAdded", &Mat::rowAboutToBeAdded);
    keyIndex("rowAdded", &Mat::rowAdded);
    keyIndex("columnAboutToBeAdded", &Mat::columnAboutToBeAdded);
    keyIndex("columnAdded", &Mat::columnAdded);
    keyIndex("rowAboutToBeRemoved", &Mat::rowAboutToBeRemoved);
    keyIndex("rowRemoved", &Mat::rowRemoved);
    keyIndex("columnAboutToBeRemoved", &Mat::columnAboutToBeRemoved);
    keyIndex("columnRemoved", &Mat::columnRemoved);
    range("rowsAboutToBeAdded", &Mat::rowsAboutToBeAdded);
    range("rowsAdded", &Mat::rowsAdded);
    range("columnsAboutToBeAdded", &Mat::columnsAboutToBeAdded);
    range("columnsAdded", &Mat::columnsAdded);
    range("rowsAboutToBeRemoved", &Mat::rowsAboutToBeRemoved);
    range("rowsRemoved", &Mat::rowsRemoved);
    range("columnsAboutToBeRemoved", &Mat::columnsAboutToBeRemoved);
    range("columnsRemoved", &Mat::columnsRemoved);
    rename("rowAboutToBeRenamed", &Mat::rowAboutToBeRenamed);
    rename("rowRenamed", &Mat::rowRenamed);
    rename("columnAboutToBeRenamed", &Mat::columnAboutToBeRenamed);
    rename("columnRenamed", &Mat::columnRenamed);
    cell("cellAboutToChange", &Mat::cellAboutToChange);
    cell("cellChanged", &Mat::cellChanged);
    rect("cellsAboutToChange", &Mat::cellsAboutToChange);
    rect("cellsChanged", &Mat::cellsChanged);
    none("aboutToBeCleared", &Mat::aboutToBeCleared);
    none("cleared", &Mat::cleared);
  }

  QObject context;
  QStringList log;
};

inline QList<QString> keys(const char * prefix, int n)
{
  QList<QString> out;
  for (int i = 0; i < n; ++i)
    out << QStringLiteral("%1%2").arg(QLatin1String(prefix)).arg(i);
  return out;
}

// the table model mirrors the matrix: counts, cells and headers
inline bool mirrors(const observable::MatrixTableModel<Mat> & model, const Mat & m)
{
  if (model.rowCount() != m.rowCount() || model.columnCount() != m.columnCount())
    return false;
  for (int r = 0; r < model.rowCount(); ++r) {
    if (model.headerData(r, Qt::Vertical).toString() != m.rowKeys()[r])
      return false;
    for (int c = 0; c < model.columnCount(); ++c) {
      const QVariant shown = model.data(model.index(r, c), Qt::DisplayRole);
      const double actual = m.valueAt(r, c);
      if (m.isUninitialized(actual) ? shown.isValid() : shown.toDouble() != actual)
        return false;
    }
  }
  for (int c = 0; c < model.columnCount(); ++c)
    if (model.headerData(c, Qt::Horizontal).toString() != m.columnKeys()[c])
      return false;
  return true;
}

}  // namespace matrix_test
