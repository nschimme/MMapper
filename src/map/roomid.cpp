// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "roomid.h"

#include "../global/AnsiOstream.h"

#include <QDebug>
#include <ostream>

std::ostream &operator<<(std::ostream &os, const RoomId id)
{
    return os << "RoomId(" << id.value() << ")";
}

std::ostream &operator<<(std::ostream &os, const ExternalRoomId id)
{
    return os << "ExternalRoomId(" << id.value() << ")";
}

std::ostream &operator<<(std::ostream &os, const ServerRoomId id)
{
    return os << "ServerRoomId(" << id.value() << ")";
}

///

AnsiOstream &operator<<(AnsiOstream &os, const RoomId id)
{
    return os << "RoomId(" << id.value() << ")";
}

AnsiOstream &operator<<(AnsiOstream &os, const ExternalRoomId id)
{
    return os << "ExternalRoomId(" << id.value() << ")";
}

AnsiOstream &operator<<(AnsiOstream &os, const ServerRoomId id)
{
    return os << "ServerRoomId(" << id.value() << ")";
}

///

QDebug operator<<(QDebug debug, const RoomId id)
{
    return debug << "RoomId(" << id.value() << ")";
}

QDebug operator<<(QDebug debug, const ExternalRoomId id)
{
    return debug << "ExternalRoomId(" << id.value() << ")";
}

QDebug operator<<(QDebug debug, const ServerRoomId id)
{
    return debug << "ServerRoomId(" << id.value() << ")";
}
