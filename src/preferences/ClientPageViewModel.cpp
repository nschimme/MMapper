// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "ClientPageViewModel.h"
#include "../configuration/configuration.h"

ClientPageViewModel::ClientPageViewModel(QObject *p) : QObject(p) {}
QFont ClientPageViewModel::font() const { QFont f; f.fromString(getConfig().integratedClient.font); return f; }
void ClientPageViewModel::setFont(const QFont &v) { if (getConfig().integratedClient.font != v.toString()) { setConfig().integratedClient.font = v.toString(); emit settingsChanged(); } }
QColor ClientPageViewModel::backgroundColor() const { return getConfig().integratedClient.backgroundColor; }
void ClientPageViewModel::setBackgroundColor(const QColor &v) { if (getConfig().integratedClient.backgroundColor != v) { setConfig().integratedClient.backgroundColor = v; emit settingsChanged(); } }
QColor ClientPageViewModel::foregroundColor() const { return getConfig().integratedClient.foregroundColor; }
void ClientPageViewModel::setForegroundColor(const QColor &v) { if (getConfig().integratedClient.foregroundColor != v) { setConfig().integratedClient.foregroundColor = v; emit settingsChanged(); } }
int ClientPageViewModel::columns() const { return getConfig().integratedClient.columns; }
void ClientPageViewModel::setColumns(int v) { if (getConfig().integratedClient.columns != v) { setConfig().integratedClient.columns = v; emit settingsChanged(); } }
int ClientPageViewModel::rows() const { return getConfig().integratedClient.rows; }
void ClientPageViewModel::setRows(int v) { if (getConfig().integratedClient.rows != v) { setConfig().integratedClient.rows = v; emit settingsChanged(); } }
void ClientPageViewModel::loadConfig() { emit settingsChanged(); }
