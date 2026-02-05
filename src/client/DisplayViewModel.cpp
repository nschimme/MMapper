// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "DisplayViewModel.h"
#include "../configuration/configuration.h"
#include <QApplication>

DisplayViewModel::DisplayViewModel(QObject *parent)
    : QObject(parent)
{
}

QColor DisplayViewModel::backgroundColor() const { return getConfig().integratedClient.backgroundColor; }
QColor DisplayViewModel::foregroundColor() const { return getConfig().integratedClient.foregroundColor; }
QFont DisplayViewModel::font() const { QFont f; f.fromString(getConfig().integratedClient.font); return f; }
int DisplayViewModel::lineLimit() const { return getConfig().integratedClient.linesOfScrollback; }

void DisplayViewModel::windowSizeChanged(int cols, int rows) {
    if (getConfig().integratedClient.autoResizeTerminal) {
        emit sig_windowSizeChanged(cols, rows);
    }
}

void DisplayViewModel::handleBell() {
    const auto &settings = getConfig().integratedClient;
    if (settings.audibleBell) QApplication::beep();
    if (settings.visualBell) emit sig_visualBell();
}

void DisplayViewModel::returnFocusToInput() { emit sig_returnFocusToInput(); }
void DisplayViewModel::showPreview(bool visible) { emit sig_showPreview(visible); }

void DisplayViewModel::slot_displayText(const QString &text) {
    emit sig_displayText(text);
}
