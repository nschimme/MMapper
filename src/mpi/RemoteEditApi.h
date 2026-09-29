#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/macros.h"

#include <cstdint>

#include <QString>

class RemoteEdit;
class AnsiOstream;

class NODISCARD RemoteEditApi final
{
private:
    RemoteEdit &m_remoteEdit;

public:
    explicit RemoteEditApi(RemoteEdit &remoteEdit)
        : m_remoteEdit(remoteEdit)
    {}
    ~RemoteEditApi() = default;
    DELETE_CTORS_AND_ASSIGN_OPS(RemoteEditApi);

public:
    void reportStatus(AnsiOstream &aos) const;
    NODISCARD bool reportStatus(AnsiOstream &aos, uint32_t id) const;
    NODISCARD bool cancel(uint32_t id);
    NODISCARD bool discard(uint32_t id);
    void simulateEdit(const QString &title);
};
