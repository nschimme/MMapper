// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "UiCommand.h"

#include <utility>

UiCommand::UiCommand(QString id, QObject *const parent)
    : QObject(parent)
    , m_id{std::move(id)}
{}

UiCommand::~UiCommand() = default;

void UiCommand::setText(QString text)
{
    if (m_text == text) {
        return;
    }
    m_text = std::move(text);
    emit sig_textChanged();
}

void UiCommand::setShortcut(QString shortcut)
{
    if (m_shortcut == shortcut) {
        return;
    }
    m_shortcut = std::move(shortcut);
    emit sig_shortcutChanged();
}

void UiCommand::setIconSource(QUrl url)
{
    if (m_iconSource == url) {
        return;
    }
    m_iconSource = std::move(url);
    emit sig_iconSourceChanged();
}

void UiCommand::setEnabled(const bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    const bool wasEffective = isEffectiveEnabled();
    m_enabled = enabled;
    emit sig_enabledChanged(m_enabled);
    if (isEffectiveEnabled() != wasEffective) {
        emit sig_effectiveEnabledChanged(isEffectiveEnabled());
    }
}

void UiCommand::setBulkDisabled(const bool bulkDisabled)
{
    if (m_bulkDisabled == bulkDisabled) {
        return;
    }
    const bool wasEffective = isEffectiveEnabled();
    m_bulkDisabled = bulkDisabled;
    if (isEffectiveEnabled() != wasEffective) {
        emit sig_effectiveEnabledChanged(isEffectiveEnabled());
    }
}

void UiCommand::setCheckable(const bool checkable)
{
    if (m_checkable == checkable) {
        return;
    }
    m_checkable = checkable;
    emit sig_checkableChanged(m_checkable);
}

void UiCommand::setChecked(const bool checked)
{
    if (m_checked == checked) {
        return;
    }
    m_checked = checked;
    emit sig_checkedChanged(m_checked);
}
