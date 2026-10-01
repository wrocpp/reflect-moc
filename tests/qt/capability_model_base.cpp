// rqt::Object<QAbstractListModel>: a reflected model whose own slots and
// properties sit on top of the model's meta-object, with Qt's model signals
// (dataChanged, rowsInserted) still working.
#define QT_NO_KEYWORDS
#include "check.hpp"

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QAbstractListModel>
#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>

namespace {
constexpr int kRows = 3;
}  // namespace

struct NameModel : rqt::Object<QAbstractListModel> {
  NameModel() { bind(); }

  [[= rqt::property{.notify = "countChanged"}]] int count() const { return static_cast<int>(names_.size()); }
  [[= rqt::signal]] void countChanged(int count) { rqt::emit{this}(count); }
  [[= rqt::slot]] void add(QString const& name) {
    beginInsertRows({}, count(), count());
    names_.append(name);
    endInsertRows();
    countChanged(count());
  }

  int rowCount(QModelIndex const& = {}) const override { return count(); }
  QVariant data(QModelIndex const& index, int role) const override {
    return role == Qt::DisplayRole ? QVariant{names_.value(index.row())} : QVariant{};
  }

  QStringList names_;
};

struct Spy : rqt::Object<> {
  Spy() { bind(); }
  int inserted = 0;
  int counts = 0;
  [[= rqt::slot]] void onInserted() { ++inserted; }
  [[= rqt::slot]] void onCount(int) { ++counts; }
};

int main(int argc, char** argv) {
  QCoreApplication app(argc, argv);
  NameModel model;
  Spy spy;

  CHECK(QObject::connect(&model, SIGNAL(rowsInserted(QModelIndex,int,int)), &spy, SLOT(onInserted())));
  CHECK(QObject::connect(&model, SIGNAL(countChanged(int)), &spy, SLOT(onCount(int))));
  for (int i = 0; i < kRows; ++i)
    CHECK(QMetaObject::invokeMethod(&model, "add", Q_ARG(QString, QString::number(i))));

  CHECK(spy.inserted == kRows && spy.counts == kRows);
  CHECK(model.rowCount() == kRows && model.property("count").toInt() == kRows);
  CHECK(model.data(model.index(1), Qt::DisplayRole).toString() == "1");
  CHECK(model.inherits("QAbstractListModel") && model.inherits("QAbstractItemModel"));
  CHECK(QByteArray{model.metaObject()->superClass()->className()} == "QAbstractListModel");
  return rqt_test::finish("capability_model_base");
}
