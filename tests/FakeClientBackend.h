#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../src/client/ClientController.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

// Records every call made through the ClientControllerBackend seam so tests
// can assert on them without a real ClientTelnet/socket. Shared by
// TestClient.cpp (unit tests of ClientController's invokable logic) and
// TestQml.cpp (loadClientPanel(), which drives ClientController through the
// actual ClientPanel.qml UI).
struct NODISCARD FakeBackend final : public ClientControllerBackend
{
    bool connected = false;
    int connectCount = 0;
    int disconnectCount = 0;
    QStringList sentToMud;
    QList<QPair<int, int>> windowSizes;

    ~FakeBackend() final;

private:
    void virt_connectToMud() final { ++connectCount; }
    void virt_disconnectFromMud() final { ++disconnectCount; }
    NODISCARD bool virt_isConnected() const final { return connected; }
    void virt_sendToMud(const QString &data) final { sentToMud.push_back(data); }
    void virt_windowSizeChanged(const int width, const int height) final
    {
        windowSizes.push_back({width, height});
    }
};
