// A class with no macro at all, against the same class processed by moc: the meta-object must equal moc's row for row
// (class info, enum, properties with their notify signals, slots, an invokable, function signals), for a class and for
// a derived class, and behave the same. docs/moc-parity.md: A-01, A-02.
#include "parity.hpp"
#include "parity_macro_free_class_moc.h"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaEnum>
#include <QtCore/QMetaProperty>
#include <QtCore/QObject>
#include <QtCore/QString>

namespace viarqt {

class Gauge : public rqt::ObjectOf<Gauge> {
 public:
  enum class Mode { off, on };
  static constexpr rqt::enum_decl mode_enum{^^Mode, false};
  static constexpr rqt::classinfo author{"author", "wro.cpp"};

  explicit Gauge(QObject* parent = nullptr) : rqt::ObjectOf<Gauge>(parent) {}
  [[= rqt::property{.write = "setLevel", .notify = "levelChanged"}]] int level() const { return level_; }
  [[= rqt::property{.write = "setMode"}]] Mode mode() const { return mode_; }
  void setMode(Mode m) { mode_ = m; }
  [[= rqt::invokable]] int twice(int n) const { return 2 * n; }

  [[= rqt::slot]] void setLevel(int l) {
    if (l == level_) return;
    level_ = l;
    levelChanged(l);
  }

  [[= rqt::signal_function]] void levelChanged(int level) { rqt::activate{this}(level); }

 private:
  int level_ = 0;
  Mode mode_ = Mode::off;
};

class FineGauge : public rqt::ObjectOf<FineGauge, Gauge> {
 public:
  explicit FineGauge(QObject* parent = nullptr) : rqt::ObjectOf<FineGauge, Gauge>(parent) {}
  [[= rqt::property{.write = "setUnit", .notify = "unitChanged"}]] QString unit() const { return unit_; }

  [[= rqt::slot]] void setUnit(const QString& u) {
    unit_ = u;
    unitChanged(u);
  }
  [[= rqt::slot]] void nudge() { setLevel(level() + 1); }

  [[= rqt::signal_function]] void unitChanged(const QString& unit) { rqt::activate{this}(unit); }

 private:
  QString unit_;
};

}  // namespace viarqt

template <class G, class F>
void behaviour(char const* what) {
  F fine;
  int levels = 0;
  QString unit;
  QObject::connect(&fine, &G::levelChanged, [&](int) { ++levels; });
  QObject::connect(&fine, &F::unitChanged, [&](const QString& u) { unit = u; });
  fine.nudge();
  rqt_test::report(levels == 1 && fine.level() == 1, what, __FILE__, __LINE__);
  rqt_test::report(fine.setProperty("unit", QString{"cm"}) && unit == "cm", what, __FILE__, __LINE__);
  rqt_test::report(fine.setProperty("level", 5) && levels == 2, what, __FILE__, __LINE__);
  int twice = 0;
  rqt_test::report(QMetaObject::invokeMethod(&fine, "twice", Q_RETURN_ARG(int, twice), Q_ARG(int, 4)) && twice == 8, what, __FILE__, __LINE__);
  QObject* as_object = &fine;
  rqt_test::report(qobject_cast<G*>(as_object) != nullptr && qobject_cast<F*>(as_object) == &fine, what, __FILE__, __LINE__);
  QMetaEnum const e = fine.metaObject()->enumerator(fine.metaObject()->indexOfEnumerator("Mode"));
  rqt_test::report(e.isValid() && e.keyCount() == 2 && QByteArray{e.valueToKey(1)} == "on", what, __FILE__, __LINE__);
}

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  viamoc::Gauge moc_gauge;
  viarqt::Gauge rqt_gauge;
  PARITY_SAME("Gauge", moc_gauge.metaObject(), rqt_gauge.metaObject());
  CHECK(parity::same_totals(moc_gauge.metaObject(), rqt_gauge.metaObject()));
  viamoc::FineGauge moc_fine;
  viarqt::FineGauge rqt_fine;
  PARITY_SAME("FineGauge", moc_fine.metaObject(), rqt_fine.metaObject());
  CHECK(parity::same_totals(moc_fine.metaObject(), rqt_fine.metaObject()));
  behaviour<viamoc::Gauge, viamoc::FineGauge>("moc behaviour");
  behaviour<viarqt::Gauge, viarqt::FineGauge>("rqt behaviour");
  return rqt_test::finish("parity_macro_free_class");
}
