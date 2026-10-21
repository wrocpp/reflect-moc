// The two hosting models of SPEC 10.7. Their host notifications are reflect-moc data-member signals, so the
// classes carry no Q_OBJECT and no moc runs.
#pragma once

#include "color_list_model.h"

#include <sel/helpers.h>
#include <sel/tracker.h>

class ColorSingleSelectionModel : public ColorListModel {
    RQT_OBJECT
    RQT_PROPERTY(bool hasSelection READ hasSelection NOTIFY hasSelectionChanged)
    RQT_PROPERTY(QString selectedName READ selectedName NOTIFY selectedRowChanged)

public:
    using Tracker = sel::Tracker<sel::Mode::Single, ColorListModel::Roles>;
    static constexpr int SelectedRole = Tracker::selectedRole();

    explicit ColorSingleSelectionModel(QObject * parent = nullptr);

signals:
    rqt::signal<void()> hasSelectionChanged;
    rqt::signal<void()> selectedRowChanged;

public:
    bool hasSelection() const;
    QString selectedName() const;
    [[= rqt::invokable]] void clearSelection();

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex & index) const override;

protected:
    void onRowInserted(int row) override;
    void onRowRemoved(int row) override;
    void onReset() override;
    void afterRowInserted(int row) override;
    void afterRowRemoved(int row) override;
    void afterReset() override;

private:
    Tracker tracker_;
    bool shifted_ = false;
    bool hadSelection_ = false;
    sel::Delta removal_;
};

class ColorMultiSelectionModel : public ColorListModel {
    RQT_OBJECT
    RQT_PROPERTY(bool hasSelection READ hasSelection NOTIFY hasSelectionChanged)
    RQT_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedCountChanged)

public:
    using Tracker = sel::Tracker<sel::Mode::Multi, ColorListModel::Roles>;
    static constexpr int SelectedRole = Tracker::selectedRole();

    explicit ColorMultiSelectionModel(QObject * parent = nullptr);

signals:
    rqt::signal<void()> hasSelectionChanged;
    rqt::signal<void()> selectedCountChanged;

public:
    bool hasSelection() const;
    int selectedCount() const;
    [[= rqt::invokable]] void clearSelection();

    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex & index, const QVariant & value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex & index) const override;

protected:
    void onRowInserted(int row) override;
    void onRowRemoved(int row) override;
    void onReset() override;
    void afterRowInserted(int row) override;
    void afterRowRemoved(int row) override;
    void afterReset() override;

private:
    Tracker tracker_;
    bool hadSelection_ = false;
    sel::Delta removal_;
};
