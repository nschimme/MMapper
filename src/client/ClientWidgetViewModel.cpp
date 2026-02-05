// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "ClientWidgetViewModel.h"
#include "ClientTelnet.h"
#include "../proxy/connectionlistener.h"
#include "../global/AnsiOstream.h"

class ViewModelClientTelnetOutputs final : public ClientTelnetOutputs {
    ClientWidgetViewModel &m_vm; DisplayViewModel *m_dvm; StackedInputViewModel *m_ivm;
public:
    ViewModelClientTelnetOutputs(ClientWidgetViewModel &vm, DisplayViewModel *d, StackedInputViewModel *i) : m_vm(vm), m_dvm(d), m_ivm(i) {}
    void virt_connected() override { m_vm.handleDisplayMessage("Connected using the integrated client"); m_dvm->returnFocusToInput(); }
    void virt_disconnected() override { m_vm.displayReconnectHint(); m_vm.handleDisplayMessage("Disconnected using the integrated client"); }
    void virt_socketError(const QString &e) override { m_dvm->slot_displayText(QString("\nInternal error! %1\n").arg(e)); }
    void virt_echoModeChanged(bool e) override { m_ivm->setPasswordMode(!e); }
    void virt_sendToUser(const QString &s) override { m_dvm->slot_displayText(s); if (m_ivm->currentIndex() == 1) m_ivm->setPasswordMode(true); }
};

ClientWidgetViewModel::ClientWidgetViewModel(ConnectionListener &l, HotkeyManager &hm, DisplayViewModel *d, StackedInputViewModel *i, QObject *p)
    : QObject(p), m_listener(l), m_hotkeyManager(hm), m_displayViewModel(d), m_stackedInputViewModel(i)
{
    m_clientTelnet = std::make_unique<ClientTelnet>(*new ViewModelClientTelnetOutputs(*this, d, i));
    connect(m_displayViewModel, &DisplayViewModel::sig_windowSizeChanged, [this](int w, int h) { m_clientTelnet->onWindowSizeChanged(w, h); });
    connect(m_stackedInputViewModel->inputViewModel(), &InputViewModel::sig_sendUserInput, this, &ClientWidgetViewModel::sendUserInput);
}

ClientWidgetViewModel::~ClientWidgetViewModel() = default;

void ClientWidgetViewModel::sendUserInput(const QString &msg) {
    if (!m_clientTelnet->isConnected()) m_clientTelnet->connectToHost(m_listener);
    else m_clientTelnet->sendToMud(msg);
}

void ClientWidgetViewModel::handleDisplayMessage(const QString &msg) { emit sig_relayMessage(msg); }

void ClientWidgetViewModel::displayReconnectHint() {
    std::stringstream oss; AnsiOstream aos{oss};
    aos.writeWithColor(getRawAnsi(AnsiColor16Enum::white, AnsiColor16Enum::cyan), "\n\n\nPress return to reconnect.\n");
    m_displayViewModel->slot_displayText(mmqt::toQStringUtf8(oss.str()));
}
