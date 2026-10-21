// AT-9.4: a host that lacks selectedRowChanged() is not a SingleHost. The member functions are plain
// functions: only the expression check matters.
#include <sel/helpers.h>

#include <QtCore/QAbstractListModel>

#include <array>

constexpr std::array<int, 1> R{Qt::UserRole + 1};

class Host : public QAbstractListModel {
public:
    int rowCount(const QModelIndex &) const override { return 1; }
    QVariant data(const QModelIndex &, int) const override { return {}; }
    void hasSelectionChanged() {}
#ifndef SK_BREAK
    void selectedRowChanged() {}
#endif
};

int main()
{
    Host host;
    sel::Tracker<sel::Mode::Single, R> tracker;
    sel::clearSelectionAndNotify(&host, tracker);
    return 0;
}
