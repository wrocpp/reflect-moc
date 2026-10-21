// AT-9.2: a role list with a duplicated enum role is not a valid role list.
#include "sel/tracker.h"

enum class E : int { A = Qt::UserRole + 1, B, C };

#ifdef SK_BREAK
constexpr std::array<E, 3> R{E::A, E::B, E::A};
#else
constexpr std::array<E, 3> R{E::A, E::B, E::C};
#endif

int main()
{
    sel::Tracker<sel::Mode::Multi, R> tracker;
    return tracker.hasSelection() ? 1 : 0;
}
