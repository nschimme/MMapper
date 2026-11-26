#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"
#include "../proxy/GmcpMessage.h"
#include "KnownRooms.h"

#include <functional>
#include <optional>

#include <QObject>
#include <QString>

// Session-scoped owner of the character's known-room history. Like RemoteEdit, it is created by
// MainWindow, is fed GMCP by MainWindow, and talks to the MUD through sig_sendGmcp, which the
// Proxy connects for the lifetime of a connection. GameObserver stays a thin signal hub.
//
// The tracker starts disabled: while disabled it requests nothing from the server (the full
// list takes a minute or more) and writes no files.
class NODISCARD_QOBJECT KnownRoomsTracker final : public QObject
{
    Q_OBJECT

public:
    struct NODISCARD StoreLocation final
    {
        QString directory;
        QString server;
    };
    // Where this connection's history lives. The default uses the configured server and Qt's
    // data location; tests inject a temporary directory. No location means no persistence.
    using Locator = std::function<std::optional<StoreLocation>()>;
    NODISCARD static std::optional<StoreLocation> defaultLocation();

private:
    KnownRooms m_known;
    Locator m_locator;
    bool m_enabled = false;

    void configureFromConnection();
    void afterChange(uint64_t previousRevision, bool request);
    void sendRequest();

public:
    explicit KnownRoomsTracker(QObject *parent, Locator locator = &defaultLocation);
    ~KnownRoomsTracker() final;

public:
    NODISCARD const KnownRooms::Snapshot &getSnapshot() const { return m_known.getSnapshot(); }
    NODISCARD bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    void onConnected();
    void onDisconnected();

public slots:
    void slot_parseGmcpInput(const GmcpMessage &msg);

signals:
    // Sent to MUME by the Proxy.
    void sig_sendGmcp(const GmcpMessage &msg);
    // The snapshot's revision changed.
    void sig_changed();
    // Enabled, but the character has no usable saved history (the first list will be slow).
    void sig_cacheMissing();
};
