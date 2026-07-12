#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/macros.h"
#include "ClientController.h"
#include "ClientTelnet.h"

#include <memory>

class ConnectionListener;

// Real (non-test) implementation of ClientControllerBackend: owns a
// ClientTelnet and forwards its ClientTelnetOutputs callbacks into a
// ClientController, mirroring the wiring ClientWidget::initClientTelnet()
// used to perform directly against widgets (see ClientWidget.cpp).
class NODISCARD ClientTelnetBackend final : public ClientControllerBackend
{
private:
    struct Outputs;

private:
    ConnectionListener &m_listener;
    ClientController &m_controller;
    std::unique_ptr<ClientTelnetOutputs> m_outputs;
    std::unique_ptr<ClientTelnet> m_telnet;

public:
    explicit ClientTelnetBackend(ConnectionListener &listener, ClientController &controller);
    ~ClientTelnetBackend() final;
    DELETE_CTORS_AND_ASSIGN_OPS(ClientTelnetBackend);

private:
    void virt_connectToMud() final;
    void virt_disconnectFromMud() final;
    NODISCARD bool virt_isConnected() const final;
    void virt_sendToMud(const QString &data) final;
    void virt_windowSizeChanged(int width, int height) final;
};
