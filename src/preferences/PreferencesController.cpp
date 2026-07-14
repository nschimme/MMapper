// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "PreferencesController.h"

#include "../configuration/configuration.h"
#include "AudioPageAdapter.h"
#include "AutoLogPageAdapter.h"
#include "GroupPageAdapter.h"
#include "MumeProtocolPageAdapter.h"
#include "PathMachinePageAdapter.h"

PreferencesController::PreferencesController(QWidget *const dialogParent, QObject *const parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
    , m_pathMachine(new PathMachinePageAdapter(this))
    , m_mumeProtocol(new MumeProtocolPageAdapter(dialogParent, this))
    , m_autoLog(new AutoLogPageAdapter(dialogParent, this))
    , m_group(new GroupPageAdapter(dialogParent, this))
    , m_audio(new AudioPageAdapter(this))
{
    connect(m_group,
            &GroupPageAdapter::sig_groupSettingsChanged,
            this,
            &PreferencesController::sig_groupSettingsChanged);
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
}
