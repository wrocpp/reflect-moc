#pragma once

#include "color_list_model.h"

#include "sel/helpers.h"
#include "sel/tracker.h"

#include <optional>

class ColorSingleSelectionModel : public ColorListModel {
    Q_OBJECT
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY hasSelectionChanged)
    Q_PROPERTY(QString selectedName READ selectedName NOTIFY selectedRowChanged)
public:
    using Tracker = sel::Tracker<sel::Mode::Single, ColorListModel::Roles>;
    static constexpr int SelectedRole = Tracker::selectedRole();

    explicit ColorSingleSelectionModel(QObject * parent = nullptr);

    bool hasSelection() const;
    QString selectedName() const;
    Q_INVOKABLE void clearSelection();

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex & index) const override;

signals:
    void hasSelectionChanged();
    void selectedRowChanged();

protected:
    void onRowInserted(int row) override;
    void onRowRemoved(int row) override;
    void onReset() override;
    void afterRowInserted(int row) override;
    void afterRowRemoved(int row) override;
    void afterReset() override;

private:
    Tracker m_tracker;
    std::optional<int> m_rowBeforeInsert;
    sel::Delta m_removalDelta;
    bool m_hadSelection = false;
};

class ColorMultiSelectionModel : public ColorListModel {
    Q_OBJECT
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY hasSelectionChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedCountChanged)
public:
    using Tracker = sel::Tracker<sel::Mode::Multi, ColorListModel::Roles>;
    static constexpr int SelectedRole = Tracker::selectedRole();

    explicit ColorMultiSelectionModel(QObject * parent = nullptr);

    bool hasSelection() const;
    int selectedCount() const;
    Q_INVOKABLE void clearSelection();

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex & index) const override;

signals:
    void hasSelectionChanged();
    void selectedCountChanged();

protected:
    void onRowInserted(int row) override;
    void onRowRemoved(int row) override;
    void onReset() override;
    void afterRowRemoved(int row) override;
    void afterReset() override;

private:
    Tracker m_tracker;
    sel::Delta m_removalDelta;
    bool m_hadSelection = false;
};
