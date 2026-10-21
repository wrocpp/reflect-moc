// AT-9.5: batches do not exist on a sequential container.
#include <obs/container.h>

#include <QtCore/QHash>
#include <QtCore/QList>

#ifdef SK_BREAK
using C = QList<int>;
#else
using C = QHash<int, int>;
#endif

int main()
{
    obs::Container<C> c;
    return static_cast<int>(c.removeMany({1, 2}));
}
