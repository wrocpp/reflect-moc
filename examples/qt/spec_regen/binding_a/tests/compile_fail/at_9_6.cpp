// AT-9.6: a multi-container is not a supported container.
#include "obs/container.h"

#ifdef SK_BREAK
using C = QMultiHash<int, int>;
#else
using C = QHash<int, int>;
#endif

int main()
{
    obs::Container<C> container;
    return static_cast<int>(container.size());
}
