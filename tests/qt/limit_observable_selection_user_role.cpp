// Limit: Qt::UserRole itself is not a custom role of a tracker: the container models answer their key there. This file must NOT
// compile with
//   ... constraints not satisfied ... ValidRoleList
#include <selection_tracker.hpp>

#include <array>

constexpr std::array<int, 1> kRoles{Qt::UserRole};

observable::SelectionTracker<observable::SelectionMode::Multi, kRoles> refused;
