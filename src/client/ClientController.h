#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/macros.h"
#include "HotkeyManager.h"
#include "InputHistory.h"

#include <memory>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

class ClientLineModel;

// Backend seam for ClientController: the real implementation
// (ClientTelnetBackend, see ClientTelnetBackend.h/.cpp) owns a ClientTelnet
// and forwards its callbacks into a ClientController; tests substitute a
// FakeBackend instead, so ClientController's Q_INVOKABLE logic (command
// splitting, history, hotkeys, ...) can be exercised without a real socket.
struct NODISCARD ClientControllerBackend
{
public:
    explicit ClientControllerBackend() = default;
    virtual ~ClientControllerBackend();
    DELETE_CTORS_AND_ASSIGN_OPS(ClientControllerBackend);

public:
    void connectToMud() { virt_connectToMud(); }
    void disconnectFromMud() { virt_disconnectFromMud(); }
    NODISCARD bool isConnected() const { return virt_isConnected(); }
    // Caller is responsible for appending '\n'.
    void sendToMud(const QString &data) { virt_sendToMud(data); }
    void windowSizeChanged(const int width, const int height)
    {
        virt_windowSizeChanged(width, height);
    }

private:
    virtual void virt_connectToMud() = 0;
    virtual void virt_disconnectFromMud() = 0;
    NODISCARD virtual bool virt_isConnected() const = 0;
    virtual void virt_sendToMud(const QString &data) = 0;
    virtual void virt_windowSizeChanged(int width, int height) = 0;
};

// QML-facing controller for the integrated MUD client. Lifts the behavior
// that used to live in InputWidget/StackedInputWidget/ClientWidget (see
// inputwidget.h/.cpp, stackedinputwidget.h/.cpp, ClientWidget.cpp) into a
// QObject that can be driven from QML (ClientPanel.qml, added in a later
// commit) while staying independently testable via the
// ClientControllerBackend seam above (see TestClient.cpp's FakeBackend).
class NODISCARD_QOBJECT ClientController final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool connected READ getConnected NOTIFY connectedChanged)
    Q_PROPERTY(bool echoVisible READ getEchoVisible NOTIFY echoVisibleChanged)
    Q_PROPERTY(bool usingClient READ getUsingClient NOTIFY usingClientChanged)
    // True on the Wasm build, which auto-clicks the "Play" button on
    // startup (see ClientWidget's ctor: `if constexpr (CURRENT_PLATFORM ==
    // PlatformEnum::Wasm) { ui.playButton->click(); }`); QML uses this to
    // decide whether to call play() itself on load.
    Q_PROPERTY(bool autoPlay READ getAutoPlay CONSTANT)
    // The local listening port to display on the welcome screen before
    // connecting. ClientWidget's constructor reads this directly from
    // Configuration rather than from ConnectionListener (there is no
    // ConnectionListener::getPort() accessor), so this does the same rather
    // than threading a port value through the backend seam or MainWindow.
    Q_PROPERTY(int port READ getPort CONSTANT)

private:
    ClientLineModel &m_model;
    HotkeyManager &m_hotkeys;
    std::unique_ptr<ClientControllerBackend> m_backend;
    InputHistory m_inputHistory;
    TabHistory m_tabHistory;
    bool m_connected = false;
    bool m_echoVisible = true;
    bool m_usingClient = false;

public:
    explicit ClientController(ClientLineModel &model, HotkeyManager &hotkeys, QObject *parent);
    ~ClientController() final;

public:
    // Ownership of the backend belongs to whoever constructs it (typically
    // MainWindow, via a ClientTelnetBackend); tests instead install a
    // FakeBackend. All backend-driving invokables silently no-op if this is
    // never called.
    void setBackend(std::unique_ptr<ClientControllerBackend> backend)
    {
        m_backend = std::move(backend);
    }

public:
    NODISCARD bool getConnected() const { return m_connected; }
    NODISCARD bool getEchoVisible() const { return m_echoVisible; }
    NODISCARD bool getUsingClient() const { return m_usingClient; }
    NODISCARD bool getAutoPlay() const;
    NODISCARD int getPort() const;

public:
    // Telnet-event relays. Called by whatever owns the real telnet
    // connection (see ClientTelnetBackend's private ClientTelnetOutputs
    // implementation), mirroring ClientWidget::initClientTelnet()'s
    // LocalClientTelnetOutputs.
    void onConnected();
    void onDisconnected();
    void onSocketError(const QString &errorStr);
    /** toggles echo mode for passwords */
    void onEchoModeChanged(bool echo);
    void onSendToUser(const QString &str);

public:
    Q_INVOKABLE void play();
    Q_INVOKABLE void sendInput(const QString &text);
    Q_INVOKABLE void sendPassword(const QString &password);
    NODISCARD Q_INVOKABLE bool isHotkey(int key, int modifiers) const;
    Q_INVOKABLE bool sendHotkey(int key, int modifiers);

    // historyUp()/historyDown() mirror InputWidget::backwardHistory() /
    // forwardHistory() (see inputwidget.cpp) EXACTLY, including their
    // boundary asymmetry:
    //  - historyUp() (Up key): at the oldest entry (InputHistory::atEnd()),
    //    relays a boundary message via sig_relayMessage and returns an
    //    INVALID QVariant -- the caller's input text must be left
    //    untouched. Otherwise it returns a valid QVariant<QString> that the
    //    caller's input text must be replaced with wholesale.
    //  - historyDown() (Down key): ALWAYS returns a valid QVariant<QString>
    //    (never invalid) -- the caller's input text must always be
    //    replaced, either with the retrieved value or with an empty string.
    //    This mirrors InputWidget::forwardHistory() unconditionally calling
    //    clear() before checking any boundary, so both the "reached
    //    beginning" boundary and one internal edge case (stepping off the
    //    end lands back at the front without a value to show) clear the
    //    text without a replacement value.
    NODISCARD Q_INVOKABLE QVariant historyUp();
    NODISCARD Q_INVOKABLE QVariant historyDown();

    // restart=true resets the completion dictionary iterator to the most
    // recent word (mirrors InputWidget::tabComplete() starting a fresh
    // completion cycle when the fragment under the cursor changes); returns
    // the next candidate completion, or an empty string once the dictionary
    // is exhausted or has no match (see TabHistory::nextMatch()'s
    // wraparound contract). Selection/cursor mechanics remain the caller's
    // (QML's) responsibility, same as they remained InputWidget's when this
    // logic first moved into TabHistory.
    NODISCARD Q_INVOKABLE QString tabComplete(const QString &fragment, bool restart);

    Q_INVOKABLE void reportWindowSize(int cols, int rows);
    Q_INVOKABLE void disconnectFromMud();

    // The command-separator splitting logic from
    // InputWidget::sendCommandWithSeparator() (e.g. "l;;look" sends "l"
    // then "look" when integratedClient.useCommandSeparator is enabled).
    // Returns {input} unchanged when the separator is disabled or empty.
    NODISCARD static QStringList splitCommands(const QString &input);

signals:
    void connectedChanged();
    void echoVisibleChanged();
    void usingClientChanged();
    // Mirrors ClientWidget::relayMessage()/slot_onShowMessage(): status-bar
    // style messages (connect/disconnect notices, history boundary hints).
    void sig_relayMessage(QString message);
    // Mirrors StackedInputWidget::requestPassword(): ask the UI to show/
    // refocus the password entry surface.
    void sig_passwordPromptRequested();
    // Mirrors ClientTelnetOutputs::virt_connected()'s `getInput().setFocus();`.
    void sig_requestInputFocus();

private:
    // Splits `command` on the configured separator and, for each part,
    // either sends it to the backend (connecting first is dropped -- not
    // queued -- exactly like ClientWidget.cpp's virt_sendUserInput()) and
    // always echoes it in yellow (mirrors
    // StackedInputWidget::displayInputMessage()). Used by both sendInput()
    // (which also records history) and sendHotkey() (which does not).
    void sendCommandWithSeparator(const QString &command);
};
