// AT-9.1: a role list holding a built-in role is not a valid role list.
#include "sel/tracker.h"

#ifdef SK_BREAK
constexpr std::array<int, 2> R{Qt::DisplayRole, Qt::UserRole + 1};
#else
constexpr std::array<int, 2> R{Qt::UserRole + 1, Qt::UserRole + 2};
#endif

int main()
{
    sel::Tracker<sel::Mode::Single, R> tracker;
    return tracker.hasSelection() ? 1 : 0;
}
