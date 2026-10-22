// Helpers of the data and type edge tests: read a model back through its key role, and build the models.
#pragma once

#include "check.hpp"

#include <container_model.hpp>

#include <QtCore/QList>
#include <QtCore/QVariant>
#include <QtTest/QAbstractItemModelTester>

#include <memory>

namespace edge_data {

constexpr auto kFatal = QAbstractItemModelTester::FailureReportingMode::Fatal;

// A model of a container with a formatter that shows the value as text; the tester runs in fatal mode.
template <class C>
struct Rig {
  using Source = observable::ObservableContainer<C>;
  using Key = typename Source::Key;
  using Value = typename Source::Value;

  explicit Rig(C content = {}) : source(std::move(content)), model(&source, [](const Key&, const Value&) { return QVariant(); }),
                                  tester(&model, kFatal) {}

  QList<Key> keys() const {
    QList<Key> out;
    for (int row = 0; row < model.rowCount(); ++row) out.append(model.index(row, 0).data(Qt::UserRole).template value<Key>());
    return out;
  }

  Source source;
  observable::ContainerModel<C> model;
  QAbstractItemModelTester tester;
};

}  // namespace edge_data
