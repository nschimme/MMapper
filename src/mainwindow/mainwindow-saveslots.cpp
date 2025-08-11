// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../mapdata/mapdata.h"
#include "mainwindow.h"

#include <memory>

#include <QFileDialog>
#include <QMessageBox>

namespace { // anonymous
NODISCARD QDir getLastMapDir()
{
    const QString &path = getConfig().autoLoad.lastMapDirectory;
    QDir dir;
    if (dir.mkpath(path)) {
        dir.setPath(path);
    } else {
        dir.setPath(QDir::homePath());
    }
    return dir;
}
} // namespace

bool MainWindow::maybeSave()
{
    auto &mapData = deref(m_mapData);
    if (!mapData.dataChanged()) {
        return true;
    }

    const QString changes = mmqt::toQStringUtf8(mapData.describeChanges());

    QMessageBox dlg(this);
    dlg.setIcon(QMessageBox::Warning);
    dlg.setWindowTitle(tr("mmapper"));
    dlg.setText(tr("The current map has been modified:\n\n") + changes
                + tr("\nDo you want to save the changes?"));
    dlg.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    dlg.setDefaultButton(QMessageBox::Discard);
    dlg.setEscapeButton(QMessageBox::Cancel);
    const int ret = dlg.exec();
    if (ret == QMessageBox::Save) {
        return slot_save();
    }

    // REVISIT: is it a bug if this returns true? (Shouldn't this always be false?)
    return ret != QMessageBox::Cancel;
}

bool MainWindow::slot_save()
{
    if (m_mapData->getFileName().isEmpty() || m_mapData->isFileReadOnly()) {
        return slot_saveAs();
    }
    return saveFile(m_mapData->getFileName(), SaveModeEnum::FULL, SaveFormatEnum::MM2);
}

bool MainWindow::slot_saveAs()
{
    saveMap(SaveModeEnum::FULL, SaveFormatEnum::MM2);
    return true;
}

bool MainWindow::slot_exportBaseMap()
{
    saveMap(SaveModeEnum::BASEMAP, SaveFormatEnum::MM2);
    return true;
}

bool MainWindow::slot_exportMm2xmlMap()
{
    saveMap(SaveModeEnum::FULL, SaveFormatEnum::MM2XML);
    return true;
}

bool MainWindow::slot_exportWebMap()
{
    const QString dir = QFileDialog::getExistingDirectory(this,
                                                          tr("Choose a directory to save the web map"),
                                                          getLastMapDir().path());
    if (dir.isEmpty()) {
        showStatusShort(tr("No directory provided"));
        return false;
    }

    return saveFile(dir, SaveModeEnum::BASEMAP, SaveFormatEnum::WEB);
}

bool MainWindow::slot_exportMmpMap()
{
    saveMap(SaveModeEnum::FULL, SaveFormatEnum::MMP);
    return true;
}
