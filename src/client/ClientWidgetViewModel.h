#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "DisplayViewModel.h"
#include "StackedInputViewModel.h"
#include "HotkeyManager.h"
#include "../global/macros.h"
#include <QObject>

class ClientTelnet;
class ConnectionListener;

class NODISCARD_QOBJECT ClientWidgetViewModel final : public QObject
{
    Q_OBJECT
public:
    explicit ClientWidgetViewModel(ConnectionListener &listener, HotkeyManager &hm, DisplayViewModel *dvm, StackedInputViewModel *ivm, QObject *parent = nullptr);
    ~ClientWidgetViewModel() final;

    void sendUserInput(const QString &msg);
    void handleDisplayMessage(const QString &msg);
    void displayReconnectHint();

signals:
    void sig_relayMessage(const QString &msg);

private:
    ConnectionListener &m_listener;
    HotkeyManager &m_hotkeyManager;
    DisplayViewModel *m_displayViewModel;
    StackedInputViewModel *m_stackedInputViewModel;
    std::unique_ptr<ClientTelnet> m_clientTelnet;
};
