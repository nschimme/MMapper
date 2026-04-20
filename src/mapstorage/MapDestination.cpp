// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "MapDestination.h"

#include "../global/ConfigConsts-Computed.h"
#include "filesaver.h"

#include <stdexcept>

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QIODevice>

std::shared_ptr<MapDestination> MapDestination::alloc(const QString fileName, SaveFormatEnum format)
{
    std::shared_ptr<FileSaver> fileSaver = nullptr;
    std::shared_ptr<QBuffer> buffer = nullptr;

    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        assert(format != SaveFormatEnum::WEB);
        buffer = std::make_shared<QBuffer>();

    } else if (format == SaveFormatEnum::WEB) {
        QDir destDir(fileName);
        if (!destDir.exists()) {
            if (!QDir().mkpath(fileName)) {
                throw std::runtime_error("Cannot create directory");
            }
        }
        if (!QFileInfo(fileName).isWritable()) {
            throw std::runtime_error("Directory is not writable");
        }
    } else {
        fileSaver = std::make_shared<FileSaver>();
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

const QByteArray MapDestination::getWasmBufferData() const
{
    if (isFileWasm()) {
        assert(m_buffer);
        return m_buffer->data();
    }
    return {};
}

void MapDestination::open()
{
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        assert(isFileWasm());
        assert(m_buffer);
        if (!m_buffer->open(QIODevice::WriteOnly)) {
            throw std::runtime_error("Cannot open QBuffer for writing");
        }
    } else if (isFileNative()) {
        assert(m_fileSaver);
        m_fileSaver->open(m_fileName);
    }
}

void MapDestination::finalize()
{
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        assert(isFileWasm());
        assert(m_buffer);
    } else if (isFileNative()) {
        assert(m_fileSaver);
        m_fileSaver->close();
    } else {
        assert(isDirectory());
    }
}
