// AT-9.2: a duplicated enum role is not a ValidRoleList.
#include <sel/tracker.h>

#include <array>

enum class E : int { A = Qt::UserRole + 10, B, C };

#ifdef SK_BREAK
constexpr std::array<E, 2> R{E::A, E::A};
#else
constexpr std::array<E, 2> R{E::A, E::B};
#endif

int main()
{
    sel::Tracker<sel::Mode::Multi, R> tracker;
    tracker.reset(2);
    return tracker.rowCount() == 2 ? 0 : 1;
}
