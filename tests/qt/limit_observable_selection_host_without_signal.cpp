// Limit: a host that lacks selectedRowChanged() is not a SingleHost, so clearSelectionAndNotify does not accept it. This file must
// NOT compile with
//   ... constraints not satisfied ... SingleHost
#include <selection_tracker.hpp>

#include <QtCore/QAbstractListModel>

#include <array>

constexpr std::array<int, 1> kRoles{Qt::UserRole + 1};

class Host : public QAbstractListModel {
 public:
  int rowCount(const QModelIndex&) const override { return 1; }
  QVariant data(const QModelIndex&, int) const override { return {}; }
  void hasSelectionChanged() {}
};

int main() {
  Host host;
  observable::SelectionTracker<observable::SelectionMode::Single, kRoles> tracker;
  observable::clearSelectionAndNotify(&host, tracker);
  return 0;
}
