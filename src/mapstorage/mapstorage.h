#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include <cstdint>
#include <QArgument>
#include <QObject>
#include <QString>
#include <QtGlobal>

#include "../expandoracommon/coordinate.h"
#include "../global/RuleOf5.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/roomfactory.h"
#include "../mapfrontend/mapfrontend.h"
#include "abstractmapstorage.h"

class Connection;
class InfoMark;
class QDataStream;
class QFile;
class QObject;
class Room;

class MapStorage : public AbstractMapStorage
{
    Q_OBJECT

public:
    explicit MapStorage(MapData &, const QString &, QFile *, QObject *parent = nullptr);
    explicit MapStorage(MapData &, const QString &, QObject *parent = nullptr);
    bool mergeData() override;

private:
    virtual bool canLoad() const override { return true; }
    virtual bool canSave() const override { return true; }

    virtual void newData() override;
    virtual bool loadData() override;
    virtual bool saveData(bool baseMapOnly) override;

    RoomFactory factory;
    Room *loadRoom(QDataStream &stream, uint32_t version);
    void loadExits(Room &room, QDataStream &stream, uint32_t version);
    void loadMark(InfoMark *mark, QDataStream &stream, uint32_t version);
    void saveMark(InfoMark *mark, QDataStream &stream);
    void saveRoom(const Room &room, QDataStream &stream);
    void saveExits(const Room &room, QDataStream &stream);

    uint32_t baseId = 0u;
    Coordinate basePosition;
};

class MapFrontendBlocker final
{
public:
    explicit MapFrontendBlocker(MapFrontend &in_data)
        : data(in_data)
    {
        data.block();
    }
    ~MapFrontendBlocker() { data.unblock(); }

public:
    MapFrontendBlocker() = delete;
    DELETE_CTORS_AND_ASSIGN_OPS(MapFrontendBlocker);

private:
    MapFrontend &data;
};
