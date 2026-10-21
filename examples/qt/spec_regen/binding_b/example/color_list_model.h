// The BASE model of SPEC 10.7, with the reflect-moc binding: RQT_OBJECT where Q_OBJECT was.
#pragma once

#include <reflect_moc/qt/qt.hpp>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QString>

#include <array>

struct ColorItem {
    QString name;
    QString hex;
};

class ColorListModel : public QAbstractListModel {
    RQT_OBJECT

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
    void resetWith(QList<ColorItem> items);

protected:
    // called INSIDE the begin/end pair, after the list changed
    virtual void onRowInserted(int row);
    virtual void onRowRemoved(int row);
    virtual void onReset();
    // called AFTER the end* call, when the model is consistent again
    virtual void afterRowInserted(int row);
    virtual void afterRowRemoved(int row);
    virtual void afterReset();

private:
    QList<ColorItem> items_;
};
