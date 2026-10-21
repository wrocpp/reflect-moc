#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QVariant>

#include <array>

struct ColorItem {
    QString name;
    QString hex;
};

// The base model: a plain list model that calls protected hooks inside and
// after each begin/end pair, so a subclass can keep a selection in step.
class ColorListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role : int { NameRole = Qt::UserRole + 1, HexRole };
    static constexpr std::array<Role, 2> Roles{NameRole, HexRole};

    explicit ColorListModel(QObject * parent = nullptr);

    int rowCount(const QModelIndex & parent = {}) const override;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    const QList<ColorItem> & items() const;
    void insertItem(int row, const ColorItem & item);
    void removeItem(int row);
    void resetWith(const QList<ColorItem> & items);

protected:
    virtual void onRowInserted(int row);
    virtual void onRowRemoved(int row);
    virtual void onReset();
    virtual void afterRowInserted(int row);
    virtual void afterRowRemoved(int row);
    virtual void afterReset();

private:
    QList<ColorItem> m_items;
};
