// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "configdialog.h"

#include "../configuration/configuration.h"
#include "audiopage.h"
#include "autologpage.h"
#include "clientpage.h"
#include "generalpage.h"
#include "graphicspage.h"
#include "grouppage.h"
#include "mumeprotocolpage.h"
#include "parserpage.h"
#include "pathmachinepage.h"
#include "ui_configdialog.h"

#include <QIcon>
#include <QListWidget>
#include <QtWidgets>

namespace {
bool matches(const QWidget *widget, const QString &text)
{
    if (auto *cb = qobject_cast<const QCheckBox *>(widget))
        return cb->text().contains(text, Qt::CaseInsensitive);
    if (auto *rb = qobject_cast<const QRadioButton *>(widget))
        return rb->text().contains(text, Qt::CaseInsensitive);
    if (auto *lbl = qobject_cast<const QLabel *>(widget))
        return lbl->text().contains(text, Qt::CaseInsensitive);
    if (auto *pb = qobject_cast<const QPushButton *>(widget))
        return pb->text().contains(text, Qt::CaseInsensitive);
    if (auto *gb = qobject_cast<const QGroupBox *>(widget))
        return gb->title().contains(text, Qt::CaseInsensitive);
    return false;
}

void showWithBuddy(QWidget *widget)
{
    widget->show();
    if (auto *lbl = qobject_cast<QLabel *>(widget)) {
        if (lbl->buddy())
            lbl->buddy()->show();
    }
}
} // namespace

ConfigDialog::ConfigDialog(QWidget *const parent)
    : QDialog(parent)
    , ui(new Ui::ConfigDialog)
{
    ui->setupUi(this);

    setWindowTitle(tr("Config Dialog"));

    auto generalPage = new GeneralPage(this, m_workingConfig);
    auto graphicsPage = new GraphicsPage(this, m_workingConfig);
    auto parserPage = new ParserPage(this, m_workingConfig);
    auto clientPage = new ClientPage(this, m_workingConfig);
    auto groupPage = new GroupPage(this, m_workingConfig);
    auto autoLogPage = new AutoLogPage(this, m_workingConfig);
    auto audioPage = new AudioPage(this, m_workingConfig);
    auto mumeProtocolPage = new MumeProtocolPage(this, m_workingConfig);
    auto pathmachinePage = new PathmachinePage(this, m_workingConfig);

    auto addPage = [this](QWidget *widget, const QString &name, const QString &iconPath) {
        auto *item = new QListWidgetItem(QIcon(iconPath), name, ui->contentsWidget);
        ui->scrollLayout->addWidget(widget);
        m_pages.append({name, widget, item});
    };

    addPage(generalPage, tr("General"), ":/icons/generalcfg.png");
    addPage(graphicsPage, tr("Graphics"), ":/icons/graphicscfg.png");
    addPage(parserPage, tr("Parser"), ":/icons/parsercfg.png");
    addPage(clientPage, tr("Integrated Mud Client"), ":/icons/terminal.png");
    addPage(groupPage, tr("Group Panel"), ":/icons/group-recolor.png");
    addPage(autoLogPage, tr("Auto Logger"), ":/icons/autologgercfg.png");
    addPage(audioPage, tr("Audio"), ":/icons/audiocfg.png");
    addPage(mumeProtocolPage, tr("Mume Protocol"), ":/icons/mumeprotocolcfg.png");
    addPage(pathmachinePage, tr("Path Machine"), ":/icons/pathmachinecfg.png");

    ui->scrollLayout->addStretch();

    connect(ui->contentsWidget,
            &QListWidget::currentItemChanged,
            this,
            &ConfigDialog::slot_changePage);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ConfigDialog::slot_ok);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ConfigDialog::slot_cancel);
    connect(ui->buttonBox->button(QDialogButtonBox::Apply),
            &QPushButton::clicked,
            this,
            &ConfigDialog::slot_apply);
    connect(ui->searchBar, &QLineEdit::textChanged, this, &ConfigDialog::slot_search);

    connect(generalPage, &GeneralPage::sig_reloadConfig, this, [this]() {
        m_originalConfig = getConfig();
        m_workingConfig = getConfig();
        slot_updateApplyButton();
        emit sig_loadConfig();
    });

    m_workingConfig.registerChangeCallback(m_lifetime, [this]() { slot_updateApplyButton(); });

    connect(this, &ConfigDialog::sig_loadConfig, generalPage, &GeneralPage::slot_loadConfig);
    connect(this, &ConfigDialog::sig_loadConfig, graphicsPage, &GraphicsPage::slot_loadConfig);
    connect(this, &ConfigDialog::sig_loadConfig, parserPage, &ParserPage::slot_loadConfig);
    connect(this, &ConfigDialog::sig_loadConfig, clientPage, &ClientPage::slot_loadConfig);
    connect(this, &ConfigDialog::sig_loadConfig, autoLogPage, &AutoLogPage::slot_loadConfig);
    connect(this, &ConfigDialog::sig_loadConfig, audioPage, &AudioPage::slot_loadConfig);
    connect(this, &ConfigDialog::sig_loadConfig, groupPage, &GroupPage::slot_loadConfig);
    connect(this,
            &ConfigDialog::sig_loadConfig,
            mumeProtocolPage,
            &MumeProtocolPage::slot_loadConfig);
    connect(this, &ConfigDialog::sig_loadConfig, pathmachinePage, &PathmachinePage::slot_loadConfig);
}

ConfigDialog::~ConfigDialog()
{
    delete ui;
}

void ConfigDialog::closeEvent(QCloseEvent *const event)
{
    if (m_workingConfig != m_originalConfig) {
        QMessageBox box(this);
        box.setWindowTitle(tr("Unapplied Changes"));
        box.setText(tr("You have unapplied changes. What would you like to do?"));
        auto *applyButton = box.addButton(tr("Apply"), QMessageBox::AcceptRole);
        auto *discardButton = box.addButton(tr("Discard"), QMessageBox::DestructiveRole);
        box.addButton(tr("Cancel"), QMessageBox::RejectRole);

        box.exec();

        if (box.clickedButton() == applyButton) {
            slot_apply();
            event->accept();
        } else if (box.clickedButton() == discardButton) {
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}

void ConfigDialog::showEvent(QShowEvent *const event)
{
    m_workingConfig = getConfig();
    m_originalConfig = getConfig();

    emit sig_loadConfig();

    if (parentWidget()) {
        auto pos = parentWidget()->pos();
        pos.setX(pos.x() + (parentWidget()->width() / 2) - (width() / 2));
        move(pos);
    }

    event->accept();
}

void ConfigDialog::slot_changePage(QListWidgetItem *current, QListWidgetItem *const previous)
{
    if (current == nullptr) {
        return;
    }

    for (const auto &page : m_pages) {
        if (page.item == current) {
            ui->pagesScrollArea->ensureWidgetVisible(page.widget);
            break;
        }
    }
}

void ConfigDialog::slot_apply()
{
    if (m_workingConfig == m_originalConfig) {
        return;
    }

    setConfig() = m_workingConfig;
    getConfig().write();
    m_originalConfig = m_workingConfig;
    slot_updateApplyButton();
}

void ConfigDialog::slot_ok()
{
    slot_apply();
    accept();
}

void ConfigDialog::slot_cancel()
{
    close();
}

void ConfigDialog::slot_updateApplyButton()
{
    ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(m_workingConfig != m_originalConfig);
}

void ConfigDialog::slot_search(const QString &text)
{
    if (text.isEmpty()) {
        for (auto &page : m_pages) {
            page.widget->show();
            page.item->setHidden(false);

            const auto children = page.widget->findChildren<QWidget *>();
            for (auto *child : children) {
                child->show();
            }
        }
        return;
    }

    for (auto &page : m_pages) {
        bool pageMatches = page.name.contains(text, Qt::CaseInsensitive);
        bool anyChildMatches = false;

        const auto groupBoxes = page.widget->findChildren<QGroupBox *>();
        for (auto *gb : groupBoxes) {
            if (matches(gb, text)) {
                gb->show();
                const auto children = gb->findChildren<QWidget *>();
                for (auto *child : children)
                    child->show();
                anyChildMatches = true;
            } else {
                bool anyGbChildMatches = false;
                const auto children = gb->findChildren<QWidget *>();
                for (auto *child : children) {
                    if (matches(child, text)) {
                        showWithBuddy(child);
                        anyGbChildMatches = true;
                    } else if (qobject_cast<QLabel *>(child) || qobject_cast<QCheckBox *>(child)
                               || qobject_cast<QRadioButton *>(child)
                               || qobject_cast<QPushButton *>(child)) {
                        child->hide();
                    }
                }

                if (anyGbChildMatches) {
                    gb->show();
                    anyChildMatches = true;
                } else {
                    gb->hide();
                }
            }
        }

        const auto directChildren = page.widget->findChildren<QWidget *>(QString(),
                                                                         Qt::FindDirectChildrenOnly);
        for (auto *child : directChildren) {
            if (qobject_cast<QGroupBox *>(child))
                continue;

            if (matches(child, text)) {
                showWithBuddy(child);
                anyChildMatches = true;
            } else if (qobject_cast<QLabel *>(child) || qobject_cast<QCheckBox *>(child)
                       || qobject_cast<QRadioButton *>(child)
                       || qobject_cast<QPushButton *>(child)) {
                child->hide();
            }
        }

        if (pageMatches || anyChildMatches) {
            page.widget->show();
            page.item->setHidden(false);
            if (pageMatches && !anyChildMatches) {
                const auto children = page.widget->findChildren<QWidget *>();
                for (auto *child : children)
                    child->show();
            }
        } else {
            page.widget->hide();
            page.item->setHidden(true);
        }
    }
}
