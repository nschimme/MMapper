// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "ClientController.h"

#include "../configuration/configuration.h"
#include "../global/AnsiOstream.h"
#include "../global/AnsiTextUtils.h"
#include "../global/ConfigConsts-Computed.h"
#include "../global/TextUtils.h"
#include "ClientLineModel.h"
#include "Hotkey.h"

#include <sstream>

#include <QRegularExpression>

ClientControllerBackend::~ClientControllerBackend() = default;

namespace { // anonymous
NODISCARD QString echoLine(const QString &text)
{
    // Mirrors StackedInputWidget::displayInputMessage() (see
    // stackedinputwidget.cpp).
    return QStringLiteral("\033[0;33m") + text + QStringLiteral("\033[0m\n");
}
} // namespace

ClientController::ClientController(ClientLineModel &model,
                                   HotkeyManager &hotkeys,
                                   QObject *const parent)
    : QObject(parent)
    , m_model{model}
    , m_hotkeys{hotkeys}
{}

ClientController::~ClientController() = default;

bool ClientController::getAutoPlay() const
{
    return CURRENT_PLATFORM == PlatformEnum::Wasm;
}

int ClientController::getPort() const
{
    return getConfig().connection.localPort;
}

void ClientController::play()
{
    if (!m_usingClient) {
        m_usingClient = true;
        emit usingClientChanged();
    }
    if (m_backend) {
        m_backend->connectToMud();
    }
}

void ClientController::sendCommandWithSeparator(const QString &command)
{
    for (const QString &part : splitCommands(command)) {
        if (m_backend) {
            if (m_backend->isConnected()) {
                m_backend->sendToMud(part + "\n");
            } else {
                // Parity with ClientWidget.cpp's virt_sendUserInput(): the
                // command is DROPPED (not queued) when disconnected -- only
                // the connection attempt is kicked off.
                m_backend->connectToMud();
            }
        }
        m_model.appendText(echoLine(part));
    }
}

void ClientController::sendInput(const QString &text)
{
    sendCommandWithSeparator(text);
    // Parity with InputWidget::gotInput(): the raw (unsplit) text is what
    // gets recorded in both histories.
    m_inputHistory.addInputLine(text);
    m_tabHistory.addInputLine(text);
}

void ClientController::sendPassword(const QString &password)
{
    if (m_backend) {
        m_backend->sendToMud(password + "\n");
    }
    // Mirrors StackedInputWidget::gotPasswordInput()'s
    // displayInputMessage("******"); echo mode is left untouched here (it
    // only changes via onEchoModeChanged(), driven by the telnet layer).
    m_model.appendText(echoLine(QStringLiteral("******")));
}

bool ClientController::isHotkey(const int key, const int modifiers) const
{
    const Hotkey hk(static_cast<Qt::Key>(key), static_cast<Qt::KeyboardModifiers>(modifiers));
    return hk.isValid() && m_hotkeys.getCommand(hk).has_value();
}

bool ClientController::sendHotkey(const int key, const int modifiers)
{
    const Hotkey hk(static_cast<Qt::Key>(key), static_cast<Qt::KeyboardModifiers>(modifiers));
    if (!hk.isValid()) {
        return false;
    }
    const auto command = m_hotkeys.getCommand(hk);
    if (!command) {
        return false;
    }
    // Parity with InputWidget::handleCommandInput(): hotkeys go straight
    // through sendCommandWithSeparator(), bypassing gotInput()'s history
    // recording entirely.
    sendCommandWithSeparator(mmqt::toQStringUtf8(*command));
    return true;
}

QVariant ClientController::historyUp()
{
    if (m_inputHistory.atEnd()) {
        emit sig_relayMessage(QStringLiteral("Reached end of input history"));
        return {};
    }
    const QString value = m_inputHistory.value();
    if (!m_inputHistory.atEnd()) {
        m_inputHistory.forward();
    }
    return QVariant::fromValue(value);
}

QVariant ClientController::historyDown()
{
    if (m_inputHistory.atFront()) {
        emit sig_relayMessage(QStringLiteral("Reached beginning of input history"));
        return QVariant::fromValue(QString());
    }
    if (m_inputHistory.atEnd()) {
        m_inputHistory.backward();
    }
    if (!m_inputHistory.atFront()) {
        m_inputHistory.backward();
        return QVariant::fromValue(m_inputHistory.value());
    }
    return QVariant::fromValue(QString());
}

QString ClientController::tabComplete(const QString &fragment, const bool restart)
{
    if (restart) {
        m_tabHistory.reset();
    }
    if (const auto match = m_tabHistory.nextMatch(fragment)) {
        return *match;
    }
    return {};
}

void ClientController::reportWindowSize(const int cols, const int rows)
{
    if (m_backend) {
        m_backend->windowSizeChanged(cols, rows);
    }
}

void ClientController::disconnectFromMud()
{
    if (m_backend) {
        m_backend->disconnectFromMud();
    }
}

QStringList ClientController::splitCommands(const QString &input)
{
    const auto &settings = getConfig().integratedClient;

    // Handle command separator (e.g., "l;;look" sends "l" then "look").
    // Mirrors InputWidget::sendCommandWithSeparator() exactly.
    if (settings.useCommandSeparator && !settings.commandSeparator.isEmpty()) {
        const QString &sep = settings.commandSeparator;
        const QString escaped = QRegularExpression::escape(sep);
        const QRegularExpression regex(QString("(?<!\\\\)%1").arg(escaped));
        QStringList commands = input.split(regex);
        for (QString &cmd : commands) {
            cmd.replace("\\" + sep, sep);
        }
        return commands;
    }
    return {input};
}

void ClientController::onConnected()
{
    // Mirrors ClientWidget::initClientTelnet()'s LocalClientTelnetOutputs::virt_connected().
    emit sig_relayMessage(QStringLiteral("Connected using the integrated client"));
    emit sig_requestInputFocus();
    if (!m_connected) {
        m_connected = true;
        emit connectedChanged();
    }
}

void ClientController::onDisconnected()
{
    // Mirrors ClientWidget::displayReconnectHint(): the exact ANSI banner
    // shown before "Disconnected using the integrated client" is relayed.
    {
        constexpr const auto whiteOnCyan = getRawAnsi(AnsiColor16Enum::white, AnsiColor16Enum::cyan);
        std::stringstream oss;
        AnsiOstream aos{oss};
        aos.writeWithColor(whiteOnCyan, "\n\n\nPress return to reconnect.\n");
        m_model.appendText(mmqt::toQStringUtf8(oss.str()));
    }
    emit sig_relayMessage(QStringLiteral("Disconnected using the integrated client"));
    if (m_connected) {
        m_connected = false;
        emit connectedChanged();
    }
}

void ClientController::onSocketError(const QString &errorStr)
{
    // Mirrors LocalClientTelnetOutputs::virt_socketError().
    m_model.appendText(QStringLiteral("\nInternal error! %1\n").arg(errorStr));
}

void ClientController::onEchoModeChanged(const bool echo)
{
    if (m_echoVisible != echo) {
        m_echoVisible = echo;
        emit echoVisibleChanged();
    }
    if (!echo) {
        // Mirrors StackedInputWidget::setEchoMode()'s unconditional call to
        // requestPassword() (which itself no-ops if already shown) whenever
        // echo turns hidden.
        emit sig_passwordPromptRequested();
    }
}

void ClientController::onSendToUser(const QString &str)
{
    m_model.appendText(str);
    // Parity with ClientWidget.cpp:220-223: re-open the password prompt if
    // a message arrives while echo is still hidden.
    if (!m_echoVisible) {
        emit sig_passwordPromptRequested();
    }
}
