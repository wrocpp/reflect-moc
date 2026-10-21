// AT-9.3: selectedValue has no meaning on a Multi tracker.
#include <sel/helpers.h>

#include <QtCore/QString>

#include <array>
#include <vector>

constexpr std::array<int, 1> R{Qt::UserRole + 1};

#ifdef SK_BREAK
constexpr sel::Mode kMode = sel::Mode::Multi;
#else
constexpr sel::Mode kMode = sel::Mode::Single;
#endif

int main()
{
    sel::Tracker<kMode, R> tracker;
    tracker.reset(1);
    const std::vector<QString> names{QStringLiteral("a")};
    const QString name = sel::selectedValue<[](const QString & s) -> QString { return s; }>(tracker, names);
    return name.isEmpty() ? 0 : 1;
}
