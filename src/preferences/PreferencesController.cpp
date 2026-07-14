// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "PreferencesController.h"

#include "../configuration/configuration.h"
#include "AudioPageAdapter.h"
#include "AutoLogPageAdapter.h"
#include "ClientPageAdapter.h"
#include "GeneralPageAdapter.h"
#include "GraphicsPageAdapter.h"
#include "GroupPageAdapter.h"
#include "MumeProtocolPageAdapter.h"
#include "ParserPageAdapter.h"
#include "PathMachinePageAdapter.h"

PreferencesController::PreferencesController(QWidget *const dialogParent, QObject *const parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
    , m_pathMachine(new PathMachinePageAdapter(this))
    , m_mumeProtocol(new MumeProtocolPageAdapter(dialogParent, this))
    , m_autoLog(new AutoLogPageAdapter(dialogParent, this))
    , m_group(new GroupPageAdapter(dialogParent, this))
    , m_audio(new AudioPageAdapter(this))
    , m_graphics(new GraphicsPageAdapter(dialogParent, this))
    , m_parser(new ParserPageAdapter(dialogParent, this))
    , m_general(new GeneralPageAdapter(dialogParent, this))
    , m_client(new ClientPageAdapter(dialogParent, this))
{
    connect(m_group,
            &GroupPageAdapter::sig_groupSettingsChanged,
            this,
            &PreferencesController::sig_groupSettingsChanged);
    connect(m_graphics,
            &GraphicsPageAdapter::sig_graphicsSettingsChanged,
            this,
            &PreferencesController::sig_graphicsSettingsChanged);
    connect(m_general, &GeneralPageAdapter::sig_reloadConfig, this, [this]() {
        reloadAll();
        emit sig_generalReloadRequested();
    });
}

void PreferencesController::ok()
{
    getConfig().write();
    emit sig_accepted();
}

void PreferencesController::cancel()
{
    setConfig().read();
    reloadAll();
}

void PreferencesController::reloadAll()
{
    m_pathMachine->reload();
    m_mumeProtocol->reload();
    m_autoLog->reload();
    m_group->reload();
    m_audio->reload();
    m_graphics->reload();
    m_parser->reload();
    m_general->reload();
    m_client->reload();
}
