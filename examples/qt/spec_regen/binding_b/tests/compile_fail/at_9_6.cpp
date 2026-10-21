// AT-9.6: a multi-container is not a SupportedContainer.
#include <obs/container.h>

#include <QtCore/QHash>

#ifdef SK_BREAK
using C = QMultiHash<int, int>;
#else
using C = QHash<int, int>;
#endif

int main()
{
    obs::Container<C> c;
    return static_cast<int>(c.size());
}
