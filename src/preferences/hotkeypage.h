#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../client/Hotkey.h"
#include "../global/macros.h"

#include <QAbstractTableModel>
#include <QDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QWidget>

/**
 * HotkeyModel manages the list of hotkeys and their commands for the QTableView.
 */
class HotkeyModel final : public QAbstractTableModel
{
public:
    explicit HotkeyModel(QObject *parent = nullptr);
    ~HotkeyModel() override;

    void refresh();
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    const Hotkey &hotkeyAt(int row) const { return m_hotkeys[row].first; }
    QString commandAt(int row) const { return m_hotkeys[row].second; }

private:
    QVector<QPair<Hotkey, QString>> m_hotkeys;
};

/**
 * HotkeyRecorderDialog is a modal dialog that listens for a key combination.
 */
class HotkeyRecorderDialog final : public QDialog
{
public:
    explicit HotkeyRecorderDialog(QWidget *parent = nullptr);
    ~HotkeyRecorderDialog() override;

    Hotkey hotkey() const { return m_hotkey; }

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    Hotkey m_hotkey;
    QLabel *m_label;
};

/**
 * EditHotkeyDialog allows entering a command for a new/existing hotkey.
 */
class EditHotkeyDialog final : public QDialog
{
public:
    explicit EditHotkeyDialog(QWidget *parent = nullptr);
    ~EditHotkeyDialog() override;

    std::optional<Hotkey> hotkey() const { return m_hotkey; }
    QString command() const { return m_commandEdit->text(); }

private:
    std::optional<Hotkey> m_hotkey = std::nullopt;
    QPushButton *m_recordButton;
    QLineEdit *m_commandEdit;
};

namespace Ui {
class HotkeyPage;
}

class NODISCARD_QOBJECT HotkeyPage final : public QWidget
{
    Q_OBJECT

private:
    Ui::HotkeyPage *const ui;
    HotkeyModel *m_model;

public:
    explicit HotkeyPage(QWidget *parent = nullptr);
    ~HotkeyPage() final;

public slots:
    void slot_loadConfig();

private slots:
    void slot_onAdd();
    void slot_onRemove();
    void slot_onChangeKey();
};
