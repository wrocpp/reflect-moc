// AT-9.1: a role list with a built-in role is not a ValidRoleList.
#include <sel/tracker.h>

#include <array>

#ifdef SK_BREAK
constexpr std::array<int, 2> R{Qt::DisplayRole, Qt::UserRole + 1};
#else
constexpr std::array<int, 2> R{Qt::UserRole + 1, Qt::UserRole + 2};
#endif

int main()
{
    sel::Tracker<sel::Mode::Single, R> tracker;
    tracker.set(1);
    return tracker.count() == 1 ? 0 : 1;
}
