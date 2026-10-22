// Limit: a role list that names the same role twice (here an enum) is not a ValidRoleList. This file must NOT compile with
//   ... constraints not satisfied ... ValidRoleList
#include <selection_tracker.hpp>

#include <array>

enum class Role : int { A = Qt::UserRole + 10, B, C };

constexpr std::array<Role, 2> kRoles{Role::A, Role::A};

observable::SelectionTracker<observable::SelectionMode::Multi, kRoles> refused;
