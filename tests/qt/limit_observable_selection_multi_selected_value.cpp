// Limit: selectedValue has no meaning on a Multi tracker, which holds many rows. This file must NOT compile with
//   static assertion failed: selectedValue: a Multi tracker holds multiple selection, so there is no single value to return
#include <selection_tracker.hpp>

#include <QtCore/QString>

#include <array>
#include <vector>

constexpr std::array<int, 1> kRoles{Qt::UserRole + 1};

int main() {
  observable::SelectionTracker<observable::SelectionMode::Multi, kRoles> tracker;
  tracker.reset(1);
  const std::vector<QString> names{QStringLiteral("a")};
  const QString name = observable::selectedValue<[](const QString& s) -> QString { return s; }>(tracker, names);
  return name.isEmpty() ? 0 : 1;
}
