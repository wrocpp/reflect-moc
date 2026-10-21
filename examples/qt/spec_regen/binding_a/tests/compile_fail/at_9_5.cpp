// AT-9.5: batches do not exist on a sequential container.
#include "obs/container.h"

#ifdef SK_BREAK
using C = QList<int>;
#else
using C = QHash<int, int>;
#endif

int main()
{
    obs::Container<C> container;
    return static_cast<int>(container.removeMany({1}));
}
