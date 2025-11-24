// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "MapDestination.h"

#include "../global/ConfigConsts-Computed.h"
#include "filesaver.h"

#include <stdexcept>

#include <QBuffer>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QIODevice>

std::shared_ptr<MapDestination> MapDestination::alloc(const QString fileName, SaveFormatEnum format)
{
    std::shared_ptr<FileSaver> fileSaver = nullptr;
    std::shared_ptr<QBuffer> buffer = nullptr;

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        assert(format != SaveFormatEnum::WEB);
        buffer = std::make_shared<QBuffer>();
        if (!buffer->open(QIODevice::WriteOnly)) {
            throw std::runtime_error("Cannot open QBuffer for writing.");
        }

    } else if (format == SaveFormatEnum::WEB) {
        QDir destDir(fileName);
        if (!destDir.exists()) {
            if (!destDir.mkpath(fileName)) {
                throw std::runtime_error(
                    QString("Cannot create directory %1.").arg(fileName).toStdString());
            }
        }
        if (!QFileInfo(fileName).isWritable()) {
            throw std::runtime_error(
                QString("Directory %1 is not writable.").arg(fileName).toStdString());
        }
    } else {
        fileSaver = std::make_shared<FileSaver>();
        try {
            fileSaver->open(fileName);
        } catch (const std::exception &e) {
            throw std::runtime_error(
                QString("Cannot write file %1:\n%2.").arg(fileName, e.what()).toStdString());
        }
    }

    return std::make_shared<MapDestination>(Badge<MapDestination>{},
                                            std::move(fileName),
                                            std::move(fileSaver),
                                            std::move(buffer));
}

MapDestination::MapDestination(Badge<MapDestination>,
                               const QString fileName,
                               std::shared_ptr<FileSaver> fileSaver,
                               std::shared_ptr<QBuffer> buffer)
    : m_fileName(std::move(fileName))
    , m_fileSaver(std::move(fileSaver))
    , m_buffer(std::move(buffer))
{}

std::shared_ptr<QIODevice> MapDestination::getIODevice() const
{
    if (isFileNative()) {
        assert(m_fileSaver);
        return m_fileSaver->getSharedFile();
    }
    if (isFileWasm()) {
        return m_buffer;
    }
    return nullptr;
}

void MapDestination::finalize(bool success)
{
    if (isFileNative()) {
        assert(m_fileSaver);
        m_fileSaver->close();
    } else if (isFileWasm() && success) {
        assert(m_buffer);
        QFileDialog::saveFileContent(m_buffer->data(), m_fileName);
    } else {
        assert(isDirectory());
    }
}
