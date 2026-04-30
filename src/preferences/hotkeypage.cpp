// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "hotkeypage.h"

#include "../client/Hotkey.h"
#include "../configuration/configuration.h"
#include "../global/TextUtils.h"
#include "ui_hotkeypage.h"

#include <algorithm>
#include <optional>

#include <QAbstractTableModel>
#include <QDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHeaderView>

/**
 * HotkeyModel manages the list of hotkeys and their commands for the QTableView.
 */
class HotkeyModel final : public QAbstractTableModel
{
public:
    explicit HotkeyModel(QObject *parent = nullptr)
        : QAbstractTableModel(parent)
    {
        refresh();
    }

    void refresh()
    {
        beginResetModel();
        m_hotkeys.clear();
        const QVariantMap &data = getConfig().hotkeys.data();
        for (auto it = data.begin(); it != data.end(); ++it) {
            m_hotkeys.append({Hotkey(mmqt::toStdStringUtf8(it.key())), it.value().toString()});
        }
        // Sort by key string for consistency
        std::sort(m_hotkeys.begin(), m_hotkeys.end(), [](const auto &a, const auto &b) {
            return a.first.to_string() < b.first.to_string();
        });
        endResetModel();
    }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_hotkeys.size();
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 2;
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() >= m_hotkeys.size())
            return {};

        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            if (index.column() == 0)
                return mmqt::toQStringUtf8(m_hotkeys[index.row()].first.to_string());
            if (index.column() == 1)
                return m_hotkeys[index.row()].second;
        }
        return {};
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override
    {
        if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
            if (section == 0)
                return tr("Hotkey");
            if (section == 1)
                return tr("Command");
        }
        return {};
    }

    Qt::ItemFlags flags(const QModelIndex &index) const override
    {
        if (!index.isValid())
            return Qt::NoItemFlags;
        Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (index.column() == 1)
            f |= Qt::ItemIsEditable;
        return f;
    }

    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override
    {
        if (index.isValid() && index.column() == 1 && role == Qt::EditRole) {
            QString newCommand = value.toString();
            m_hotkeys[index.row()].second = newCommand;

            // Sync back to config
            QVariantMap data = getConfig().hotkeys.data();
            data[mmqt::toQStringUtf8(m_hotkeys[index.row()].first.to_string())] = newCommand;
            setConfig().hotkeys.setData(std::move(data));

            emit dataChanged(index, index, {role});
            return true;
        }
        return false;
    }

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
    explicit HotkeyRecorderDialog(QWidget *parent = nullptr)
        : QDialog(parent)
        , m_hotkey(Qt::Key_unknown, Qt::NoModifier)
    {
        setWindowTitle(tr("Record Hotkey"));
        setModal(true);

        auto *layout = new QVBoxLayout(this);
        m_label = new QLabel(tr("Press the key combination..."), this);
        m_label->setAlignment(Qt::AlignCenter);
        m_label->setMinimumSize(300, 100);
        layout->addWidget(m_label);

        auto *hintLabel = new QLabel(tr("Esc to cancel"), this);
        hintLabel->setAlignment(Qt::AlignCenter);
        hintLabel->setStyleSheet("color: gray;");
        layout->addWidget(hintLabel);
    }

    Hotkey hotkey() const { return m_hotkey; }

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Escape) {
            reject();
            return;
        }

        // We want to ignore modifier-only presses
        if (event->key() == Qt::Key_Control || event->key() == Qt::Key_Shift ||
            event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) {
            return;
        }

        Hotkey hk(static_cast<Qt::Key>(event->key()), event->modifiers());
        if (hk.isValid()) {
            m_hotkey = hk;
            accept();
        } else if (hk.isRecognized()) {
             m_label->setText(tr("Invalid combination for this key.\n(Requires modifier)"));
        } else {
             m_label->setText(tr("Unrecognized key."));
        }
    }

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
    explicit EditHotkeyDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(tr("Add Hotkey"));
        auto *layout = new QVBoxLayout(this);

        layout->addWidget(new QLabel(tr("1. Record Key:"), this));
        m_recordButton = new QPushButton(tr("Click to Record"), this);
        layout->addWidget(m_recordButton);

        layout->addWidget(new QLabel(tr("2. Enter Command:"), this));
        m_commandEdit = new QLineEdit(this);
        layout->addWidget(m_commandEdit);

        auto *buttons = new QHBoxLayout();
        auto *okButton = new QPushButton(tr("OK"), this);
        auto *cancelButton = new QPushButton(tr("Cancel"), this);
        buttons->addWidget(okButton);
        buttons->addWidget(cancelButton);
        layout->addLayout(buttons);

        connect(m_recordButton, &QPushButton::clicked, this, [this]() {
            HotkeyRecorderDialog recorder(this);
            if (recorder.exec() == QDialog::Accepted) {
                m_hotkey = recorder.hotkey();
                m_recordButton->setText(mmqt::toQStringUtf8(m_hotkey->to_string()));
            }
        });

        connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    }

    std::optional<Hotkey> hotkey() const { return m_hotkey; }
    QString command() const { return m_commandEdit->text(); }

private:
    std::optional<Hotkey> m_hotkey = std::nullopt;
    QPushButton *m_recordButton;
    QLineEdit *m_commandEdit;
};

HotkeyPage::HotkeyPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::HotkeyPage)
    , m_model(new HotkeyModel(this))
{
    ui->setupUi(this);

    ui->hotkeyTableView->setModel(m_model);
    ui->hotkeyTableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->hotkeyTableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

    ui->addButton->setIcon(QIcon(":/icons/new.png"));
    ui->addButton->setAccessibleName(tr("Add Hotkey"));
    ui->removeButton->setIcon(QIcon(":/icons/connectiondelete.png"));
    ui->removeButton->setAccessibleName(tr("Remove Hotkey"));
    ui->editButton->setIcon(QIcon(":/icons/reload.png"));
    ui->editButton->setAccessibleName(tr("Change Key"));

    connect(ui->addButton, &QToolButton::clicked, this, &HotkeyPage::slot_onAdd);
    connect(ui->removeButton, &QToolButton::clicked, this, &HotkeyPage::slot_onRemove);
    connect(ui->editButton, &QToolButton::clicked, this, &HotkeyPage::slot_onChangeKey);
}

HotkeyPage::~HotkeyPage()
{
    delete ui;
}

void HotkeyPage::slot_loadConfig()
{
    m_model->refresh();
}

void HotkeyPage::slot_onAdd()
{
    EditHotkeyDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        auto hk = dlg.hotkey();
        if (!hk || !hk->isValid()) {
            QMessageBox::warning(this, tr("Invalid Hotkey"), tr("No valid hotkey was recorded."));
            return;
        }

        QString command = dlg.command();
        QString hkStr = mmqt::toQStringUtf8(hk->to_string());

        QVariantMap data = getConfig().hotkeys.data();
        if (data.contains(hkStr)) {
            auto result = QMessageBox::question(this, tr("Conflict"),
                tr("Hotkey '%1' is already assigned to '%2'.\nDo you want to reassign it?")
                .arg(hkStr, data[hkStr].toString()),
                QMessageBox::Yes | QMessageBox::No);

            if (result != QMessageBox::Yes) {
                return;
            }
        }

        data[hkStr] = command;
        setConfig().hotkeys.setData(std::move(data));
        m_model->refresh();
    }
}

void HotkeyPage::slot_onRemove()
{
    auto selection = ui->hotkeyTableView->selectionModel()->selectedRows();
    if (selection.isEmpty())
        return;

    int row = selection.first().row();
    QString hkStr = mmqt::toQStringUtf8(m_model->hotkeyAt(row).to_string());

    QVariantMap data = getConfig().hotkeys.data();
    data.remove(hkStr);
    setConfig().hotkeys.setData(std::move(data));
    m_model->refresh();
}

void HotkeyPage::slot_onChangeKey()
{
    auto selection = ui->hotkeyTableView->selectionModel()->selectedRows();
    if (selection.isEmpty())
        return;

    int row = selection.first().row();
    Hotkey oldHk = m_model->hotkeyAt(row);
    QString command = m_model->commandAt(row);

    HotkeyRecorderDialog recorder(this);
    if (recorder.exec() == QDialog::Accepted) {
        Hotkey newHk = recorder.hotkey();
        if (newHk == oldHk) return;

        QString newHkStr = mmqt::toQStringUtf8(newHk.to_string());
        QVariantMap data = getConfig().hotkeys.data();

        if (data.contains(newHkStr)) {
             auto result = QMessageBox::question(this, tr("Conflict"),
                tr("Hotkey '%1' is already assigned to '%2'.\nDo you want to reassign it?")
                .arg(newHkStr, data[newHkStr].toString()),
                QMessageBox::Yes | QMessageBox::No);

            if (result != QMessageBox::Yes) {
                return;
            }
        }

        data.remove(mmqt::toQStringUtf8(oldHk.to_string()));
        data[newHkStr] = command;
        setConfig().hotkeys.setData(std::move(data));
        m_model->refresh();
    }
}
