#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../map/parseevent.h"
#include "pathmachine.h"

#include <QObject>
#include <QString>

class Configuration;
class MapFrontend;
class ParseEvent;

/*!
 * @brief Concrete implementation of PathMachine.
 */
class NODISCARD_QOBJECT Mmapper2PathMachine final : public PathMachine
{
    Q_OBJECT

public:
    explicit Mmapper2PathMachine(MapFrontend &map, QObject *parent);
    ~Mmapper2PathMachine() override;

signals:
    void sig_state(const QString &);

public slots:
    void slot_handleParseEvent(const SigParseEvent &);
};
