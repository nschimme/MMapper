// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "MainViewModel.h"
#include <QFileInfo>

MainViewModel::MainViewModel(QObject *p) : QObject(p) {}

QString MainViewModel::windowTitle() const {
    QString title = "MMapper";
    if (!m_currentFilePath.isEmpty()) title += " - " + QFileInfo(m_currentFilePath).fileName();
    if (m_isModified) title += "*";
    return title;
}

void MainViewModel::setModified(bool m) {
    if (m_isModified != m) { m_isModified = m; emit isModifiedChanged(); emit windowTitleChanged(); }
}

void MainViewModel::setMapMode(MapModeEnum m) {
    if (m_mapMode != m) { m_mapMode = m; emit mapModeChanged(); }
}

void MainViewModel::newFile() { m_currentFilePath.clear(); setModified(false); emit windowTitleChanged(); }
void MainViewModel::openFile(const QString &p) { m_currentFilePath = p; setModified(false); emit windowTitleChanged(); }
void MainViewModel::saveFile() { setModified(false); }
void MainViewModel::saveFileAs(const QString &p) { m_currentFilePath = p; setModified(false); emit windowTitleChanged(); }
