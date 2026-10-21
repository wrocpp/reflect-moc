// AT-9.4: a Single host must declare hasSelectionChanged() and selectedRowChanged().
#include "sel/helpers.h"

#include <QAbstractListModel>

constexpr std::array<int, 1> R{Qt::UserRole + 1};

class Host : public QAbstractListModel {
public:
    int rowCount(const QModelIndex & = {}) const override { return 0; }
    QVariant data(const QModelIndex &, int = Qt::DisplayRole) const override { return {}; }
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
