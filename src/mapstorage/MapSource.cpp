// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "MapSource.h"

#include "../global/TextUtils.h"

#include <stdexcept>

#include <QBuffer>
#include <QFile>

std::shared_ptr<MapSource> MapSource::alloc(const QUrl &url,
                                            const std::optional<QByteArray> &fileContent)
{
    if (fileContent.has_value()) {
        auto pBuffer = std::make_shared<QBuffer>();
        pBuffer->setData(fileContent.value());
        if (!pBuffer->open(QIODevice::ReadOnly)) {
            throw std::runtime_error(mmqt::toStdStringUtf8(pBuffer->errorString()));
        }
        return std::make_shared<MapSource>(Badge<MapSource>{}, url, std::move(pBuffer));
    } else if (url.isLocalFile()) {
        auto pFile = std::make_shared<QFile>(url.toLocalFile());
        if (!pFile->open(QFile::ReadOnly)) {
            throw std::runtime_error(mmqt::toStdStringUtf8(pFile->errorString()));
        }
        return std::make_shared<MapSource>(Badge<MapSource>{}, url, std::move(pFile));
    } else if (url.scheme() == QLatin1String("qrc")) {
        auto pFile = std::make_shared<QFile>(":" + url.path());
        if (!pFile->open(QFile::ReadOnly)) {
            throw std::runtime_error(mmqt::toStdStringUtf8(pFile->errorString()));
        }
        return std::make_shared<MapSource>(Badge<MapSource>{}, url, std::move(pFile));
    } else {
        throw std::runtime_error("Cannot immediately allocate MapSource for remote URL");
    }
}

static QString getFileNameFromUrl(const QUrl &url)
{
    if (url.isLocalFile()) {
        return url.toLocalFile();
    } else if (url.scheme() == QLatin1String("qrc")) {
        return ":" + url.path();
    } else {
        return url.fileName();
    }
}

MapSource::MapSource(Badge<MapSource>, const QUrl &url, std::shared_ptr<QIODevice> device)
    : m_url(url)
    , m_fileName(getFileNameFromUrl(url))
    , m_device(std::move(device))
{}
