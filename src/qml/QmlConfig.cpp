// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "QmlConfig.h"

#include "../configuration/configuration.h"

#include <QFont>

namespace { // anonymous
NODISCARD QFont resolveClientFont()
{
    QFont font;
    font.fromString(getConfig().integratedClient.font);
    return font;
}
} // namespace

QmlConfig::QmlConfig(QObject *const parent)
    : QObject(parent)
    , m_npcHide(getConfig().groupManager.npcHide)
    , m_npcSortBottom(getConfig().groupManager.npcSortBottom)
    , m_groupColor(getConfig().groupManager.color)
    , m_clientFontFamily(resolveClientFont().family())
    , m_clientFontPointSize(resolveClientFont().pointSize())
    , m_clientBgColor(getConfig().integratedClient.backgroundColor)
    , m_clientFgColor(getConfig().integratedClient.foregroundColor)
    , m_clientClearInputOnEnter(getConfig().integratedClient.clearInputOnEnter)
    , m_clientPreviewLines(getConfig().integratedClient.linesOfPeekPreview)
{}

bool QmlConfig::getNpcHide() const
{
    return getConfig().groupManager.npcHide;
}

void QmlConfig::setNpcHide(const bool value)
{
    if (getConfig().groupManager.npcHide == value) {
        return;
    }
    setConfig().groupManager.npcHide = value;
    m_npcHide = value;
    emit npcHideChanged();
}

bool QmlConfig::getNpcSortBottom() const
{
    return getConfig().groupManager.npcSortBottom;
}

void QmlConfig::setNpcSortBottom(const bool value)
{
    if (getConfig().groupManager.npcSortBottom == value) {
        return;
    }
    setConfig().groupManager.npcSortBottom = value;
    m_npcSortBottom = value;
    emit npcSortBottomChanged();
}

QColor QmlConfig::getGroupColor() const
{
    return getConfig().groupManager.color;
}

void QmlConfig::setGroupColor(const QColor &value)
{
    if (getConfig().groupManager.color == value) {
        return;
    }
    setConfig().groupManager.color = value;
    m_groupColor = value;
    emit groupColorChanged();
}

QString QmlConfig::getClientFontFamily() const
{
    return resolveClientFont().family();
}

int QmlConfig::getClientFontPointSize() const
{
    return resolveClientFont().pointSize();
}

QColor QmlConfig::getClientBgColor() const
{
    return getConfig().integratedClient.backgroundColor;
}

QColor QmlConfig::getClientFgColor() const
{
    return getConfig().integratedClient.foregroundColor;
}

bool QmlConfig::getClientClearInputOnEnter() const
{
    return getConfig().integratedClient.clearInputOnEnter;
}

int QmlConfig::getClientPreviewLines() const
{
    return getConfig().integratedClient.linesOfPeekPreview;
}

void QmlConfig::reload()
{
    if (const bool value = getConfig().groupManager.npcHide; value != m_npcHide) {
        m_npcHide = value;
        emit npcHideChanged();
    }
    if (const bool value = getConfig().groupManager.npcSortBottom; value != m_npcSortBottom) {
        m_npcSortBottom = value;
        emit npcSortBottomChanged();
    }
    if (const QColor value = getConfig().groupManager.color; value != m_groupColor) {
        m_groupColor = value;
        emit groupColorChanged();
    }
    {
        const QFont font = resolveClientFont();
        if (const QString family = font.family(); family != m_clientFontFamily) {
            m_clientFontFamily = family;
            emit clientFontChanged();
        }
        if (const int pointSize = font.pointSize(); pointSize != m_clientFontPointSize) {
            m_clientFontPointSize = pointSize;
            emit clientFontChanged();
        }
    }
    if (const QColor value = getConfig().integratedClient.backgroundColor;
        value != m_clientBgColor) {
        m_clientBgColor = value;
        emit clientColorsChanged();
    }
    if (const QColor value = getConfig().integratedClient.foregroundColor;
        value != m_clientFgColor) {
        m_clientFgColor = value;
        emit clientColorsChanged();
    }
    if (const bool value = getConfig().integratedClient.clearInputOnEnter;
        value != m_clientClearInputOnEnter) {
        m_clientClearInputOnEnter = value;
        emit clientClearInputOnEnterChanged();
    }
    if (const int value = getConfig().integratedClient.linesOfPeekPreview;
        value != m_clientPreviewLines) {
        m_clientPreviewLines = value;
        emit clientPreviewLinesChanged();
    }
}
