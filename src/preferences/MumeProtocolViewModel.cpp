// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "MumeProtocolViewModel.h"
#include "../configuration/configuration.h"

MumeProtocolViewModel::MumeProtocolViewModel(QObject *p) : QObject(p) {}

bool MumeProtocolViewModel::useInternalEditor() const { return getConfig().mumeClientProtocol.internalRemoteEditor; }
void MumeProtocolViewModel::setUseInternalEditor(bool v) { if (getConfig().mumeClientProtocol.internalRemoteEditor != v) { setConfig().mumeClientProtocol.internalRemoteEditor = v; emit settingsChanged(); } }
QString MumeProtocolViewModel::externalEditorCommand() const { return getConfig().mumeClientProtocol.externalRemoteEditorCommand; }
void MumeProtocolViewModel::setExternalEditorCommand(const QString &v) { if (getConfig().mumeClientProtocol.externalRemoteEditorCommand != v) { setConfig().mumeClientProtocol.externalRemoteEditorCommand = v; emit settingsChanged(); } }

void MumeProtocolViewModel::loadConfig() { emit settingsChanged(); }
