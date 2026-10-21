// AT-9.3: selectedValue needs a Single tracker.
#include "sel/helpers.h"

#include <QList>
#include <QString>

constexpr std::array<int, 1> R{Qt::UserRole + 1};

#ifdef SK_BREAK
constexpr sel::Mode kMode = sel::Mode::Multi;
#else
constexpr sel::Mode kMode = sel::Mode::Single;
#endif

int main()
{
    const sel::Tracker<kMode, R> tracker;
    const QList<QString> names{QStringLiteral("a")};
    const QString value = sel::selectedValue<[](const QString & s) { return s; }>(tracker, names);
    return value.isEmpty() ? 0 : 1;
}
