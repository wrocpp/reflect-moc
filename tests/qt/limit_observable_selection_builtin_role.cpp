// Limit: a role list that holds a built-in role (here Qt::DisplayRole) is not a ValidRoleList: the tracker's roles must be
// custom roles, strictly greater than Qt::UserRole. This file must NOT compile with
//   ... constraints not satisfied ... ValidRoleList
#include <selection_tracker.hpp>

#include <array>

constexpr std::array<int, 2> kRoles{Qt::DisplayRole, Qt::UserRole + 1};

observable::SelectionTracker<observable::SelectionMode::Single, kRoles> refused;
