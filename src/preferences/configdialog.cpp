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

ConfigDialog::ConfigDialog(QWidget *const parent)
    : QDialog(parent)
    , ui(new Ui::ConfigDialog)
{
    ui->setupUi(this);

    setWindowTitle(tr("Config Dialog"));

    createIcons();

    auto generalPage = new GeneralPage(this, m_workingConfig);
    auto graphicsPage = new GraphicsPage(this, m_workingConfig);
    auto parserPage = new ParserPage(this, m_workingConfig);
    auto clientPage = new ClientPage(this, m_workingConfig);
    auto groupPage = new GroupPage(this, m_workingConfig);
    auto autoLogPage = new AutoLogPage(this, m_workingConfig);
    auto audioPage = new AudioPage(this, m_workingConfig);
    auto mumeProtocolPage = new MumeProtocolPage(this, m_workingConfig);
    auto pathmachinePage = new PathmachinePage(this, m_workingConfig);

    m_pagesWidget = new QStackedWidget(this);

    auto *const pagesWidget = m_pagesWidget;
    pagesWidget->addWidget(generalPage);
    pagesWidget->addWidget(graphicsPage);
    pagesWidget->addWidget(parserPage);
    pagesWidget->addWidget(clientPage);
    pagesWidget->addWidget(groupPage);
    pagesWidget->addWidget(autoLogPage);
    pagesWidget->addWidget(audioPage);
    pagesWidget->addWidget(mumeProtocolPage);
    pagesWidget->addWidget(pathmachinePage);
    pagesWidget->setCurrentIndex(0);

    ui->pagesScrollArea->setWidget(pagesWidget);

    ui->contentsWidget->setCurrentItem(ui->contentsWidget->item(0));
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

    connect(generalPage, &GeneralPage::sig_reloadConfig, this, [this]() {
        // sig_reloadConfig is emitted after a Factory Reset or Import.
        // The global getConfig() has already been updated in GeneralPage.
        // We need to update our originalConfig to match the new global state
        // to properly track future changes, and ensure workingConfig is in sync.
        m_originalConfig = getConfig();
        m_workingConfig = getConfig();
        slot_updateApplyButton();
        emit sig_loadConfig();
    });

    connect(this, &ConfigDialog::sig_loadConfig, [this]() { slot_updateApplyButton(); });

    for (auto *page : std::initializer_list<QWidget *>{generalPage,
                                                       graphicsPage,
                                                       parserPage,
                                                       clientPage,
                                                       groupPage,
                                                       autoLogPage,
                                                       audioPage,
                                                       mumeProtocolPage,
                                                       pathmachinePage}) {
        connect(page, SIGNAL(sig_changed()), this, SLOT(slot_updateApplyButton()));
    }

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

    // Populate the preference pages from config each time the widget is shown
    emit sig_loadConfig();

    // Move widget to center of parent's location
    auto pos = parentWidget()->pos();
    pos.setX(pos.x() + (parentWidget()->width() / 2) - (width() / 2));
    move(pos);

    event->accept();
}

void ConfigDialog::createIcons()
{
    const QSize iconTargetSize = ui->contentsWidget->iconSize();

    auto addItem = [this, iconTargetSize](const QString &iconPath, const QString &label) {
        QPixmap pixmap(iconPath);
        QPixmap scaled = pixmap.scaled(iconTargetSize,
                                       Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation);

        auto *item = new QListWidgetItem(QIcon(scaled), label, ui->contentsWidget);
        item->setTextAlignment(Qt::AlignHCenter);
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    };

    addItem(":/icons/generalcfg.png", tr("General"));
    addItem(":/icons/graphicscfg.png", tr("Graphics"));
    addItem(":/icons/parsercfg.png", tr("Parser"));
    addItem(":/icons/terminal.png", tr("Integrated\nMud Client"));
    addItem(":/icons/group-recolor.png", tr("Group Panel"));
    addItem(":/icons/autologgercfg.png", tr("Auto\nLogger"));
    addItem(":/icons/audiocfg.png", tr("Audio"));
    addItem(":/icons/mumeprotocolcfg.png", tr("Mume\nProtocol"));
    addItem(":/icons/pathmachinecfg.png", tr("Path\nMachine"));
}

void ConfigDialog::slot_changePage(QListWidgetItem *current, QListWidgetItem *const previous)
{
    if (current == nullptr) {
        current = previous;
    }
    ui->pagesScrollArea->verticalScrollBar()->setSliderPosition(0);
    m_pagesWidget->setCurrentIndex(ui->contentsWidget->row(current));
}

void ConfigDialog::slot_apply()
{
    if (m_workingConfig == m_originalConfig) {
        return;
    }

    const auto oldConfig = getConfig();

    // 1. Trigger notifications for settings that use ChangeMonitor by using setters
    if (m_workingConfig.general.getTheme() != oldConfig.general.getTheme()) {
        setConfig().general.setTheme(m_workingConfig.general.getTheme());
    }
    if (m_workingConfig.adventurePanel.getDisplayXPStatus()
        != oldConfig.adventurePanel.getDisplayXPStatus()) {
        setConfig().adventurePanel.setDisplayXPStatus(
            m_workingConfig.adventurePanel.getDisplayXPStatus());
    }
    if (m_workingConfig.audio.getMusicVolume() != oldConfig.audio.getMusicVolume()) {
        setConfig().audio.setMusicVolume(m_workingConfig.audio.getMusicVolume());
    }
    if (m_workingConfig.audio.getSoundVolume() != oldConfig.audio.getSoundVolume()) {
        setConfig().audio.setSoundVolume(m_workingConfig.audio.getSoundVolume());
    }
    if (m_workingConfig.audio.getOutputDeviceId() != oldConfig.audio.getOutputDeviceId()) {
        setConfig().audio.setOutputDeviceId(m_workingConfig.audio.getOutputDeviceId());
    }

    // NamedConfigs in canvas.advanced also need notifications
    auto &adv = setConfig().canvas.advanced;
    const auto &wadv = m_workingConfig.canvas.advanced;
    adv.use3D.set(wadv.use3D.get());
    adv.autoTilt.set(wadv.autoTilt.get());
    adv.printPerfStats.set(wadv.printPerfStats.get());
    adv.maximumFps.set(wadv.maximumFps.get());
    adv.fov.set(wadv.fov.get());
    adv.verticalAngle.set(wadv.verticalAngle.get());
    adv.horizontalAngle.set(wadv.horizontalAngle.get());
    adv.layerHeight.set(wadv.layerHeight.get());

    // And other canvas settings
    auto &can = setConfig().canvas;
    const auto &wcan = m_workingConfig.canvas;
    can.antialiasingSamples.set(wcan.antialiasingSamples.get());
    can.trilinearFiltering.set(wcan.trilinearFiltering.get());
    can.showMissingMapId.set(wcan.showMissingMapId.get());
    can.showUnsavedChanges.set(wcan.showUnsavedChanges.get());
    can.showUnmappedExits.set(wcan.showUnmappedExits.get());

    can.weatherAtmosphereIntensity.set(wcan.weatherAtmosphereIntensity.get());
    can.weatherPrecipitationIntensity.set(wcan.weatherPrecipitationIntensity.get());
    can.weatherTimeOfDayIntensity.set(wcan.weatherTimeOfDayIntensity.get());

    // Hotkeys
    if (m_workingConfig.hotkeys.data() != oldConfig.hotkeys.data()) {
        setConfig().hotkeys.setData(m_workingConfig.hotkeys.data());
    }

    // 2. Perform global assignment to catch all other fields
    setConfig() = m_workingConfig;

    // 3. Emit dialog-level signals for broader updates
    if (m_workingConfig.canvas != oldConfig.canvas) {
        emit sig_graphicsSettingsChanged();
    }

    if (m_workingConfig.groupManager != oldConfig.groupManager) {
        emit sig_groupSettingsChanged();
    }

    // Colors
    bool colorChanged = false;
#define X_COLOR_SYNC(_id, _name) \
    if (m_workingConfig.colorSettings._id != oldConfig.colorSettings._id) { \
        XNamedColor(NamedColorEnum::_id).setColor(m_workingConfig.colorSettings._id); \
        colorChanged = true; \
    }
    XFOREACH_NAMED_COLOR_OPTIONS(X_COLOR_SYNC)
#undef X_COLOR_SYNC

    if (colorChanged) {
        emit sig_graphicsSettingsChanged();
    }

    // 4. Persist changes to disk
    getConfig().write();

    // 5. Update state for tracking
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
