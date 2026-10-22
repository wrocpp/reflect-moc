// Helpers of the proxy and selection edge tests: models over string-valued containers, with a tester in fatal mode,
// and readers that compare a proxy with the model under it.
#pragma once

#include "check.hpp"

#include <container_model.hpp>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QIdentityProxyModel>
#include <QtCore/QItemSelectionModel>
#include <QtCore/QPersistentModelIndex>
#include <QtCore/QSortFilterProxyModel>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtTest/QAbstractItemModelTester>

#include <memory>
#include <vector>

namespace edge_proxy {

constexpr auto kFatal = QAbstractItemModelTester::FailureReportingMode::Fatal;
constexpr int kKeyRole = Qt::UserRole;  // ContainerModel::KeyRole

// A container of strings, whatever its shape: a list and a map or hash hold a QString per entry, a set holds the
// key itself. add() puts an entry in with a key that suits the container.
template <class C>
struct Setup {
  using Traits = observable::ContainerTraits<C>;
  using Key = typename Traits::Key;
  using Value = typename Traits::Value;
  static constexpr bool isList = Traits::order == observable::Order::indexed;
  static constexpr bool hasValue = Traits::assignable;

  static QVariant show(const Key& key, const Value& value) {
    if constexpr (hasValue)
      return QVariant(value);
    else
      return QVariant(QString::number(key));
  }
  static Value value(int n, const Key& key) {
    if constexpr (hasValue)
      return QString::number(n);
    else
      return static_cast<Value>(key);
  }
  // `n` doubles as the key for a map, hash or set, and as the text shown for the others.
  template <class Source>
  static bool add(Source& source, int n, const QString& text) {
    if constexpr (isList)
      return source.insert(source.size(), text);
    else if constexpr (hasValue)
      return source.insert(static_cast<Key>(n), text);
    else
      return source.insert(static_cast<Key>(n), static_cast<Value>(n));
  }
};

template <class C, bool Table = false>
struct Rig {
  using Source = observable::ObservableContainer<C>;
  using Model = std::conditional_t<Table, observable::ContainerTableModel<C>, observable::ContainerModel<C>>;

  static std::unique_ptr<Model> make(Source* source) {
    if constexpr (Table) {
      using Column = observable::Column<C>;
      const auto show = [](const auto& key, const auto& value) { return Setup<C>::show(key, value); };
      return std::make_unique<Model>(source, std::vector<Column>{{"first", show}, {"second", show}});
    } else {
      return std::make_unique<Model>(source, [](const auto& key, const auto& value) { return Setup<C>::show(key, value); });
    }
  }

  Rig() : model(make(&source)), tester(model.get(), kFatal) {}

  Source source;
  std::unique_ptr<Model> model;
  QAbstractItemModelTester tester;
};

inline QStringList column(const QAbstractItemModel& model, int col = 0, int role = Qt::DisplayRole) {
  QStringList out;
  for (int row = 0; row < model.rowCount(); ++row) out.append(model.index(row, col).data(role).toString());
  return out;
}

// A proxy over `model`, with its own tester in fatal mode.
struct ProxyRig {
  explicit ProxyRig(QAbstractItemModel* model, std::unique_ptr<QSortFilterProxyModel> p = std::make_unique<QSortFilterProxyModel>())
      : proxy(std::move(p)) {
    proxy->setSourceModel(model);
    tester = std::make_unique<QAbstractItemModelTester>(proxy.get(), kFatal);
  }
  std::unique_ptr<QSortFilterProxyModel> proxy;
  std::unique_ptr<QAbstractItemModelTester> tester;
};

// Every proxy row maps to a source row and back, and shows what the source row shows; the rows of the proxy are
// the rows of the source that pass, once each.
inline bool mapping_is_consistent(const QAbstractProxyModel& proxy, int role = Qt::DisplayRole) {
  const QAbstractItemModel& source = *proxy.sourceModel();
  bool ok = true;
  QSet<int> seen;
  for (int row = 0; row < proxy.rowCount(); ++row) {
    for (int col = 0; col < proxy.columnCount(); ++col) {
      const QModelIndex p = proxy.index(row, col);
      const QModelIndex s = proxy.mapToSource(p);
      ok = ok && s.isValid() && proxy.mapFromSource(s) == p;
      ok = ok && p.data(role) == s.data(role);
      if (col == 0) ok = ok && !seen.contains(s.row()) && s.row() >= 0 && s.row() < source.rowCount();
      if (col == 0) seen.insert(s.row());
    }
  }
  return ok;
}

}  // namespace edge_proxy
