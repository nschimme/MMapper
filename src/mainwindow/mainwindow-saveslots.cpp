// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../configuration/configuration.h"
#include "../mapdata/mapdata.h"
#include "../mapstorage/MapDestination.h"
#include "mainwindow.h"

#include <memory>

#include <QBuffer>
#include <QFileDialog>
#include <QMessageBox>

namespace { // anonymous


namespace mwss_detail {

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

NODISCARD std::unique_ptr<QFileDialog> createCommonSaveDialog(MainWindow &mainWindow,
                                                              const QString &suggestedName)
{
    auto save = std::make_unique<QFileDialog>(&mainWindow, "Choose map file name ...");
    save->setAcceptMode(QFileDialog::AcceptSave);
    save->setDirectory(getLastMapDir());
    save->selectFile(suggestedName);
    return save;
}

NODISCARD std::unique_ptr<QFileDialog> createDirectorySaveDialog(MainWindow &mainWindow)
{
    auto save = createCommonSaveDialog(mainWindow, "");
    save->setFileMode(QFileDialog::Directory);
    save->setOption(QFileDialog::ShowDirsOnly, true);
    return save;
}

NODISCARD std::unique_ptr<QFileDialog> createFileSaveDialog(MainWindow &mainWindow,
                                                            const QString &nameFilter,
                                                            const QString &defaultSuffix,
                                                            const QString &suggestedName)
{
    auto save = createCommonSaveDialog(mainWindow, suggestedName);
    save->setFileMode(QFileDialog::AnyFile);
    save->setNameFilter(nameFilter);
    save->setDefaultSuffix(defaultSuffix);
    save->selectFile(suggestedName);
    return save;
}

NODISCARD std::unique_ptr<QFileDialog> createDefaultSaveDialog(MainWindow &mainWindow,
                                                               const QString &suggestedName)
{
    auto save = mwss_detail::createFileSaveDialog(mainWindow,
                                                  "MMapper maps (*.mm2)",
                                                  "mm2",
                                                  suggestedName);
    return save;
}

} // namespace mwss_detail
} // namespace

void MainWindow::maybeSave(std::function<void(bool)> callback)
{
    auto &mapData = deref(m_mapData);
    if (!mapData.dataChanged()) {
        callback(true);
        return;
    }

    const QString changes = mmqt::toQStringUtf8(mapData.describeChanges());

    auto *dlg = new QMessageBox(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setIcon(QMessageBox::Warning);
    dlg->setWindowTitle(tr("mmapper"));
    dlg->setText(tr("The current map has been modified:\n\n") + changes
                + tr("\nDo you want to save the changes?"));
    dlg->setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    dlg->setDefaultButton(QMessageBox::Discard);
    dlg->setEscapeButton(QMessageBox::Cancel);

    connect(dlg, &QMessageBox::finished, this, [this, callback](int result) {
        if (result == QMessageBox::Save) {
            slot_save(callback);
        } else {
            callback(result != QMessageBox::Cancel);
        }
    });
    dlg->open();
}

void MainWindow::slot_save(std::function<void(bool)> completion)
{
    if (m_mapData->getFileName().isEmpty() || m_mapData->isFileReadOnly()) {
        slot_saveAs(std::move(completion));
        return;
    }
    saveFile(m_mapData->getFileName(),
             ::SaveModeEnum::FULL,
             ::SaveFormatEnum::MM2,
             std::move(completion));
}

void MainWindow::slot_saveAs(std::function<void(bool)> completion)
{
    if (!tryStartNewAsync()) {
        if (completion) {
            completion(false);
        }
        return;
    }

    QString suggestedName = m_mapData->getFileName();
    const QFileInfo currentFile(suggestedName);
    if (currentFile.exists()) {
        suggestedName = (currentFile.suffix().contains("xml")
                             ? currentFile.baseName().append("-import.mm2")
                             : currentFile.baseName().append("-copy.mm2"));
    }

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        saveFile(suggestedName, ::SaveModeEnum::FULL, ::SaveFormatEnum::MM2, std::move(completion));
    } else {
        auto dlg = mwss_detail::createDefaultSaveDialog(*this, suggestedName);
        auto *pDlg = dlg.release();
        pDlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(pDlg, &QFileDialog::finished, this, [this, pDlg, completion](int result) {
            if (result == QDialog::Accepted) {
                const auto fileNames = pDlg->selectedFiles();
                if (!fileNames.isEmpty()) {
                    saveFile(fileNames[0], ::SaveModeEnum::FULL, ::SaveFormatEnum::MM2, completion);
                    return;
                }
            }
            showStatusShort(tr("No filename provided"));
            if (completion) {
                completion(false);
            }
        });
        pDlg->open();
    }
}

void MainWindow::slot_exportBaseMap()
{
    const QString suggestedName = QFileInfo(m_mapData->getFileName()).baseName().append("-base.mm2");

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        saveFile(suggestedName, ::SaveModeEnum::BASEMAP, ::SaveFormatEnum::MM2);
    } else {
        auto dlg = mwss_detail::createDefaultSaveDialog(*this, suggestedName);
        auto *pDlg = dlg.release();
        pDlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(pDlg, &QFileDialog::finished, this, [this, pDlg](int result) {
            if (result == QDialog::Accepted) {
                const auto fileNames = pDlg->selectedFiles();
                if (!fileNames.isEmpty()) {
                    saveFile(fileNames[0], ::SaveModeEnum::BASEMAP, ::SaveFormatEnum::MM2);
                    return;
                }
            }
            showStatusShort(tr("No filename provided"));
        });
        pDlg->open();
    }
}

void MainWindow::slot_exportMm2xmlMap()
{
    const QString suggestedName = QFileInfo(m_mapData->getFileName()).baseName().append(".xml");

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        saveFile(suggestedName, ::SaveModeEnum::FULL, ::SaveFormatEnum::MM2XML);
    } else {
        auto dlg = mwss_detail::createFileSaveDialog(*this,
                                                      "MMapper2 XML maps (*.xml)",
                                                      "xml",
                                                      suggestedName);
        auto *pDlg = dlg.release();
        pDlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(pDlg, &QFileDialog::finished, this, [this, pDlg](int result) {
            if (result == QDialog::Accepted) {
                const auto fileNames = pDlg->selectedFiles();
                if (!fileNames.isEmpty()) {
                    saveFile(fileNames[0], ::SaveModeEnum::FULL, ::SaveFormatEnum::MM2XML);
                    return;
                }
            }
            showStatusShort(tr("No filename provided"));
        });
        pDlg->open();
    }
}

void MainWindow::slot_exportWebMap()
{
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        return;
    }

    auto dlg = mwss_detail::createDirectorySaveDialog(*this);
    auto *pDlg = dlg.release();
    pDlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(pDlg, &QFileDialog::finished, this, [this, pDlg](int result) {
        if (result == QDialog::Accepted) {
            const auto fileNames = pDlg->selectedFiles();
            if (!fileNames.isEmpty()) {
                saveFile(fileNames[0], ::SaveModeEnum::BASEMAP, ::SaveFormatEnum::WEB);
                return;
            }
        }
        showStatusShort(tr("No directory name provided"));
    });
    pDlg->open();
}

void MainWindow::slot_exportMmpMap()
{
    const QString suggestedName = QFileInfo(m_mapData->getFileName()).baseName().append("-mmp.xml");

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        saveFile(suggestedName, ::SaveModeEnum::FULL, ::SaveFormatEnum::MMP);
    } else {
        auto dlg = mwss_detail::createFileSaveDialog(*this,
                                                      "MMP maps (*.xml)",
                                                      "xml",
                                                      suggestedName);
        auto *pDlg = dlg.release();
        pDlg->setAttribute(Qt::WA_DeleteOnClose);
        connect(pDlg, &QFileDialog::finished, this, [this, pDlg](int result) {
            if (result == QDialog::Accepted) {
                const auto fileNames = pDlg->selectedFiles();
                if (!fileNames.isEmpty()) {
                    saveFile(fileNames[0], ::SaveModeEnum::FULL, ::SaveFormatEnum::MMP);
                    return;
                }
            }
            showStatusShort(tr("No filename provided"));
        });
        pDlg->open();
    }
}
