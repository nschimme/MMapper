// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "mmapper2pathmachine.h"

#include "../configuration/configuration.h"
#include "../global/SendToUser.h"
#include "../map/parseevent.h"
#include "pathmachine.h"
#include "pathparameters.h"

#include <cassert>

#include <QElapsedTimer>
#include <QString>

NODISCARD static const char *stateName(const PathStateEnum state)
{
    if (state == PathStateEnum::APPROVED) {
        return "";
    }
#define CASE(x) \
    do { \
    case PathStateEnum::x: \
        return #x; \
    } while (0)
    switch (state) {
        CASE(APPROVED);
        CASE(EXPERIMENTING);
        CASE(SYNCING);
    }
#undef CASE
    assert(false);
    return "UNKNOWN";
}

void Mmapper2PathMachine::slot_handleParseEvent(const SigParseEvent &sigParseEvent)
{
    // m_params are now kept in sync with configuration by the PathMachine base class
    // constructor and its NamedConfig callbacks.

    try {
        PathMachine::handleParseEvent(sigParseEvent);
    } catch (const std::exception &e) {
        global::sendToUser(QString("ERROR: %1\n").arg(e.what()));
    } catch (...) {
        global::sendToUser("ERROR: unknown exception\n");
    }

    emit sig_state(stateName(getState()));
}

Mmapper2PathMachine::Mmapper2PathMachine(MapFrontend &map, QObject *const parent)
    : PathMachine(map, parent)
{}
