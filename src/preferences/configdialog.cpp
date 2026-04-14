// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

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

    setWindowTitle(tr("Preferences"));

    auto generalPage = new GeneralPage(this);
    auto graphicsPage = new GraphicsPage(this);
    auto parserPage = new ParserPage(this);
    auto clientPage = new ClientPage(this);
    auto groupPage = new GroupPage(this);
    auto autoLogPage = new AutoLogPage(this);
    auto audioPage = new AudioPage(this);
    auto mumeProtocolPage = new MumeProtocolPage(this);
    auto pathmachinePage = new PathmachinePage(this);

    const auto makeSectionHeader = [](const QString &name, QWidget *const headerParent) -> QWidget * {
        auto *container = new QWidget(headerParent);
        auto *layout = new QHBoxLayout(container);
        layout->setContentsMargins(0, 0, 0, 4);
        layout->setSpacing(8);

        auto *label = new QLabel(name, container);
        QFont font = label->font();
        font.setBold(true);
        font.setPointSize(font.pointSize() + 1);
        label->setFont(font);
        layout->addWidget(label);

        auto *line = new QFrame(container);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);
        layout->addWidget(line, 1);

        return container;
    };

    auto addPage = [this, &makeSectionHeader](QWidget *widget,
                                              const QString &name,
                                              const QString &iconPath) {
        auto *item = new QListWidgetItem(QIcon(iconPath), name, ui->contentsWidget);

        auto *container = new QWidget(this);
        auto *containerLayout = new QVBoxLayout(container);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(8);
        containerLayout->addWidget(makeSectionHeader(name, container));
        containerLayout->addWidget(widget);

        ui->scrollLayout->addWidget(container);
        m_pages.append({name, widget, item, container});
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
    connect(ui->pagesScrollArea->verticalScrollBar(),
            &QScrollBar::valueChanged,
            this,
            &ConfigDialog::slot_onScroll);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &ConfigDialog::slot_ok);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &ConfigDialog::slot_cancel);
    connect(ui->searchBar, &QLineEdit::textChanged, this, &ConfigDialog::slot_search);

    connect(generalPage, &GeneralPage::sig_reloadConfig, this, [this]() {
        emit sig_loadConfig();
    });

    connect(this, &ConfigDialog::sig_loadConfig, generalPage, &GeneralPage::slot_loadConfig);
    connect(this, &ConfigDialog::sig_loadConfig, graphicsPage, &GraphicsPage::slot_loadConfig);
    connect(this, &ConfigDialog::sig_loadConfig, parserPage, &ParserPage::slot_loadConfig);
    connect(this, &ConfigDialog::sig_loadConfig, clientPage, &ClientPage::slot_loadConfig);
    connect(this, &ConfigDialog::sig_loadConfig, autoLogPage, &AutoLogPage::slot_loadConfig);
    connect(this, &ConfigDialog::sig_loadConfig, audioPage, &AudioPage::slot_loadConfig);
    connect(this, &ConfigDialog::sig_loadConfig, groupPage, &GroupPage::slot_loadConfig);
    connect(groupPage,
            &GroupPage::sig_groupSettingsChanged,
            this,
            &ConfigDialog::sig_groupSettingsChanged);
    connect(this,
            &ConfigDialog::sig_loadConfig,
            mumeProtocolPage,
            &MumeProtocolPage::slot_loadConfig);
    connect(this, &ConfigDialog::sig_loadConfig, pathmachinePage, &PathmachinePage::slot_loadConfig);
    connect(graphicsPage,
            &GraphicsPage::sig_graphicsSettingsChanged,
            this,
            &ConfigDialog::sig_graphicsSettingsChanged);
}

ConfigDialog::~ConfigDialog()
{
    delete ui;
}

void ConfigDialog::closeEvent(QCloseEvent *const event)
{
    getConfig().write();
    event->accept();
}

void ConfigDialog::showEvent(QShowEvent *const event)
{
    // Populate the preference pages from config each time the widget is shown
    emit sig_loadConfig();

    if (parentWidget()) {
        auto pos = parentWidget()->pos();
        pos.setX(pos.x() + (parentWidget()->width() / 2) - (width() / 2));
        move(pos);
    }

    event->accept();
}

void ConfigDialog::slot_changePage(QListWidgetItem *current, QListWidgetItem *const /*previous*/)
{
    if (current == nullptr || m_suppressScrollSync) {
        return;
    }

    for (const auto &page : m_pages) {
        if (page.item == current) {
            m_suppressScrollSync = true;
            ui->pagesScrollArea->verticalScrollBar()->setValue(page.container->y());
            m_suppressScrollSync = false;
            break;
        }
    }
}

void ConfigDialog::slot_onScroll(int value)
{
    if (m_suppressScrollSync) {
        return;
    }

    QListWidgetItem *activeItem = nullptr;
    for (const auto &page : m_pages) {
        if (!page.item->isHidden() && page.container->isVisible()) {
            if (page.container->y() <= value + 8) {
                activeItem = page.item;
            }
        }
    }

    if (activeItem && ui->contentsWidget->currentItem() != activeItem) {
        m_suppressScrollSync = true;
        ui->contentsWidget->setCurrentItem(activeItem);
        m_suppressScrollSync = false;
    }
}

void ConfigDialog::slot_ok()
{
    getConfig().write();
    accept();
}

void ConfigDialog::slot_cancel()
{
    setConfig().read();
    emit sig_loadConfig();
    accept();
}

void ConfigDialog::slot_search(const QString &text)
{
    if (text.isEmpty()) {
        for (auto &page : m_pages) {
            page.container->show();
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
            page.container->show();
            page.item->setHidden(false);
            if (pageMatches && !anyChildMatches) {
                const auto children = page.widget->findChildren<QWidget *>();
                for (auto *child : children)
                    child->show();
            }
        } else {
            page.container->hide();
            page.item->setHidden(true);
        }
    }
}
