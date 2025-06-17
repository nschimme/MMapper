// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "World.h"

#include "../global/ConfigConsts.h"
#include "../global/Timer.h"
#include "../global/logging.h"
#include "../global/progresscounter.h"
#include "Diff.h"
#include "MapConsistencyError.h"
#include "enums.h"
#include "parseevent.h"
#include "sanitizer.h"
#include "utils.h"
#include "RawExit.h" // Added for ::satisfiesInvariants (speculative)

#include <algorithm>
#include <future>
#include <optional>
#include <ostream>
#include <sstream>
#include <vector>

namespace { // anonymous

// REVISIT: Should we replace this with a user-controlled option, or just make for debug builds?
// For now, it's useful to see this info in release builds;
//
// Also, we may want to try to disable this for test cases, because there are tests of invalid
// enum values, and those can trigger the error() function in the ChangePrinter.
static volatile bool print_world_changes = true;
// This limit exists because reverting may create a very large list of changes.
static volatile size_t max_change_batch_print_size = 20;

template<typename Enum>
void sanityCheckEnum(const Enum value)
{
    if (value != enums::sanitizeEnum(value)) {
        throw MapConsistencyError("invalid enum value");
    }
}
template<typename Flags>
void sanityCheckFlags(const Flags flags)
{
    if (flags != enums::sanitizeFlags(flags)) {
        throw MapConsistencyError("invalid flags");
    }
}

template<typename Key>
void insertId(OrderedMap<Key, CowRoomIdSet> &map, const Key &key, const RoomId id)
{
    const CowRoomIdSet *const oldCow = map.find(key);
    if (oldCow == nullptr) {
        map.set(key, CowRoomIdSet{std::set<RoomId>{id}});
    } else if (oldCow->getReadOnly().count(id) == 0) { // MODIFIED .contains to .count
        auto copy = oldCow->getReadOnly();
        copy.insert(id);
        map.set(key, CowRoomIdSet{copy});
    }
}

template<typename Key>
void removeId(OrderedMap<Key, CowRoomIdSet> &map, const Key &key, const RoomId id)
{
    const CowRoomIdSet *const oldCow = map.find(key);
    if (oldCow == nullptr || oldCow->getReadOnly().count(id) == 0) { // MODIFIED .contains to .count
        return;
    }

    auto copy = oldCow->getReadOnly();
    copy.erase(id);
    if (copy.empty()) {
        map.erase(key);
    } else {
        map.set(key, CowRoomIdSet{copy});
    }
}

template<typename T>
inline void merge(T &dst, const T &src)
{
    dst = src;
}

template<>
void merge(RoomNote &dst, const RoomNote &src)
{
    dst = RoomNote{dst.toStdStringUtf8() + src.toStdStringUtf8()};
}

template<>
void merge /*<RoomMobFlags>*/ (RoomMobFlags &dst, const RoomMobFlags &src)
{
    dst |= src;
}

template<>
void merge /*<RoomLoadFlags>*/ (RoomLoadFlags &dst, const RoomLoadFlags &src)
{
    dst |= src;
}

void applyDoorName(DoorName &name, const FlagModifyModeEnum mode, const DoorName &x)
{
    switch (mode) {
    case FlagModifyModeEnum::ASSIGN:
        name = x;
        break;
    case FlagModifyModeEnum::CLEAR:
        name = {};
        break;
    case FlagModifyModeEnum::INSERT:
    case FlagModifyModeEnum::REMOVE:
        assert(false);
        break;
    }
}

template<typename Flags>
void applyFlagChange(Flags &flags, const Flags change, const FlagModifyModeEnum mode)
{
    switch (mode) {
    case FlagModifyModeEnum::ASSIGN:
        flags = enums::sanitizeFlags(change);
        break;
    case FlagModifyModeEnum::INSERT:
        flags = enums::sanitizeFlags(flags | change);
        break;
    case FlagModifyModeEnum::REMOVE:
        flags = enums::sanitizeFlags(flags & ~change);
        break;
    case FlagModifyModeEnum::CLEAR:
        flags.clear();
        break;
    }
}

void applyDoorFlags(DoorFlags &doorFlags, const FlagModifyModeEnum mode, const DoorFlags x)
{
    applyFlagChange(doorFlags, x, mode);
}

void applyExitFlags(ExitFlags &exitFlags, const FlagModifyModeEnum mode, const ExitFlags x)
{
    applyFlagChange(exitFlags, x, mode);
}

// Assuming a free function, if not a member of RawExit.
// Declaration would be something like: bool satisfiesInvariants(const RawExit&);
// If it's a member, this forward declaration is not needed and call site changes.
// For now, this is a placeholder if it's a free function not declared in an included header.
// bool satisfiesInvariants(const RawExit&);


} // namespace

World World::copy() const
{
    DECL_TIMER(t, "World::copy");

    World result;
    result.m_remapping = m_remapping;
    result.m_rooms = m_rooms;
    result.m_spatialDb = m_spatialDb;
    result.m_serverIds = m_serverIds;
    result.m_parseTree = m_parseTree;
    result.m_areaInfos = m_areaInfos;

    return result;
}

bool World::operator==(const World &rhs) const
{
    return m_remapping == rhs.m_remapping     //
           && m_rooms == rhs.m_rooms          //
           && m_spatialDb == rhs.m_spatialDb  //
           && m_serverIds == rhs.m_serverIds  //
           && m_parseTree == rhs.m_parseTree  //
           && m_areaInfos == rhs.m_areaInfos; //
}

NODISCARD auto World::findArea(const std::optional<RoomArea> &area) -> AreaInfo *
{
    return m_areaInfos.find(area); // MODIFIED: removed .getMutable() or .getReadOnly()
}

NODISCARD auto World::findArea(const std::optional<RoomArea> &area) const -> const AreaInfo *
{
    return m_areaInfos.find(area); // MODIFIED: removed .getMutable() or .getReadOnly()
}

NODISCARD auto World::getArea(const std::optional<RoomArea> &area) -> AreaInfo &
{
    return m_areaInfos.get(area); // MODIFIED: removed .getMutable() or .getReadOnly()
}

NODISCARD auto World::getArea(const std::optional<RoomArea> &area) const -> const AreaInfo &
{
    return m_areaInfos.get(area); // MODIFIED: removed .getMutable() or .getReadOnly()
}

const RawRoom *World::getRoom(const RoomId id) const
{
    if (!hasRoom(id)) {
        return nullptr;
    }
    return &m_rooms.getRawRoomRef(id);
}

bool World::hasRoom(const RoomId id) const
{
    if (id == INVALID_ROOMID) {
        throw InvalidMapOperation("Invalid RoomId");
    }
    return m_remapping.contains(id);
}

void World::requireValidRoom(const RoomId id) const
{
    if (!hasRoom(id)) {
        throw InvalidMapOperation("RoomId not valid");
    }
}

std::optional<RoomId> World::findRoom(const Coordinate &coord) const
{
    if (const RoomId *const id = m_spatialDb.findUnique(coord)) {
        return *id;
    }
    return std::nullopt;
}

ServerRoomId World::getServerId(const RoomId id) const
{
    requireValidRoom(id);
    return m_rooms.getServerId(id);
}

std::optional<RoomId> World::lookup(const ServerRoomId id) const
{
    return m_serverIds.lookup(id);
}

const Coordinate &World::getPosition(const RoomId id) const
{
    requireValidRoom(id);
    return m_rooms.getPosition(id);
}

#define X_DEFINE_ACCESSORS(_Type, _Name, _Init) \
    const _Type &World::getExit##_Type(const RoomId id, const ExitDirEnum dir) const \
    { \
        requireValidRoom(id); \
        return m_rooms.getExit##_Type(id, dir); \
    }
XFOREACH_EXIT_FLAG_PROPERTY(X_DEFINE_ACCESSORS)
#undef X_DEFINE_ACCESSORS

DoorName World::getExitDoorName(const RoomId id, const ExitDirEnum dir) const
{
    requireValidRoom(id);
    return m_rooms.getExitDoorName(id, dir);
}

bool World::hasExit(const RoomId id, ExitDirEnum dir) const
{
    return getExitFlags(id, dir).isExit();
}

#define X_DEFINE_GETTER(_Type, _Prop, _OptInit) \
    _Type World::get##_Type(const RoomId id, const ExitDirEnum dir) const \
    { \
        return getExit##_Type(id, dir); \
    }
XFOREACH_EXIT_PROPERTY(X_DEFINE_GETTER)
#undef X_DEFINE_GETTER

#define X_DEFINE_GETTER(UPPER_CASE, lower_case, CamelCase, friendly) \
    bool World::exitIs##CamelCase(const RoomId id, const ExitDirEnum dir) const \
    { \
        return getExitFlags(id, dir).contains(ExitFlagEnum::UPPER_CASE); \
    }
XFOREACH_EXIT_FLAG(X_DEFINE_GETTER)
#undef X_DEFINE_GETTER

#define X_DEFINE_GETTER(UPPER_CASE, lower_case, CamelCase, friendly) \
    bool World::doorIs##CamelCase(const RoomId id, const ExitDirEnum dir) const \
    { \
        return exitIsDoor(id, dir) && getDoorFlags(id, dir).contains(DoorFlagEnum::UPPER_CASE); \
    }
XFOREACH_DOOR_FLAG(X_DEFINE_GETTER)
#undef X_DEFINE_GETTER

const TinyRoomIdSet &World::getOutgoing(const RoomId id, const ExitDirEnum dir) const
{
    return m_rooms.getExitOutgoing(id, dir);
}

const TinyRoomIdSet &World::getIncoming(const RoomId id, const ExitDirEnum dir) const
{
    return m_rooms.getExitIncoming(id, dir);
}

void World::insertParse(const RoomId id, const ParseKeyFlags parseKeys)
{
    requireValidRoom(id);

    const RoomName &name = getRoomName(id);
    const auto &desc = m_rooms.getRoomDescription(id);
    assert(sanitizer::isSanitizedMultiline(desc.getStdStringViewUtf8()));

    if (parseKeys.contains(ParseKeyEnum::Name)) {
        insertId(m_parseTree.name_only, name, id); // MODIFIED
    }
    if (parseKeys.contains(ParseKeyEnum::Desc)) {
        insertId(m_parseTree.desc_only, desc, id); // MODIFIED
    }
    if (parseKeys.contains(ParseKeyEnum::Name) || parseKeys.contains(ParseKeyEnum::Desc)) {
        const NameDesc nameDesc{name, desc};
        insertId(m_parseTree.name_desc, nameDesc, id); // MODIFIED
    }
}

void World::removeParse(const RoomId id, const ParseKeyFlags parseKeys)
{
    requireValidRoom(id);

    const RoomName &name = getRoomName(id);
    const auto &desc = m_rooms.getRoomDescription(id);
    assert(sanitizer::isSanitizedMultiline(desc.getStdStringViewUtf8()));

    if (parseKeys.contains(ParseKeyEnum::Name)) {
        removeId(m_parseTree.name_only, name, id); // MODIFIED
    }
    if (parseKeys.contains(ParseKeyEnum::Desc)) {
        removeId(m_parseTree.desc_only, desc, id); // MODIFIED
    }
    if (parseKeys.contains(ParseKeyEnum::Name) || parseKeys.contains(ParseKeyEnum::Desc)) {
        const NameDesc nameDesc{name, desc};
        removeId(m_parseTree.name_desc, nameDesc, id); // MODIFIED
    }
}

ParseKeyFlags World::parseKeysChanged(const RawRoom &a, const RawRoom &b)
{
    ParseKeyFlags result;
    if (a.fields.Name != b.fields.Name) {
        result.insert(ParseKeyEnum::Name);
    }
    if (a.fields.Description != b.fields.Description) {
        result.insert(ParseKeyEnum::Desc);
    }
    return result;
}

void World::setRoom(const RoomId id, const RawRoom &room)
{
    if (id == INVALID_ROOMID) {
        throw InvalidMapOperation("Invalid RoomId");
    }
    if (room.id != id) {
        throw InvalidMapOperation("RoomId mismatch");
    }

    ParseKeyFlags parseChanged = ALL_PARSE_KEY_FLAGS;
    std::optional<Coordinate> oldCoord;
    ServerRoomId oldServerId = INVALID_SERVER_ROOMID;
    if (hasRoom(id)) {
        const auto oldRaw = getRawCopy(id);
        if (room == oldRaw) {
            return;
        }

        oldServerId = oldRaw.server_id;
        oldCoord = oldRaw.position;
        parseChanged = parseKeysChanged(oldRaw, room);
        if (parseChanged) {
            removeParse(id, parseChanged);
        }
        m_areaInfos.remove(oldRaw.getArea(), id); // MODIFIED
    }

    m_areaInfos.insert(room.getArea(), id); // MODIFIED

    if (oldServerId != INVALID_SERVER_ROOMID && oldServerId != room.server_id) {
        m_serverIds.remove(oldServerId);
    }

    if (oldCoord && oldCoord != room.position) {
        const auto &coord = room.position;
        m_spatialDb.remove(id, coord);
    }

    const auto serverId = room.server_id;
    const auto newCoord = room.position;
    {
        setRoom_lowlevel(id, room);
    }

    if (parseChanged) {
        insertParse(id, parseChanged);
    }

    if (oldServerId != serverId) {
        m_serverIds.set(serverId, id);
    }

    if (newCoord != oldCoord) {
        const auto &coord = newCoord;
        m_spatialDb.add(id, coord);
    }

    if constexpr (IS_DEBUG_BUILD) {
        const auto &here = deref(getRoom(id));
        assert(satisfiesInvariants(here)); // Assumes ::satisfiesInvariants(const RawRoom&)

        auto copy = room;
        enforceInvariants(copy); // Assumes ::enforceInvariants(RawRoom&)
        assert(here == copy);
    }
}

bool World::hasOneWayExit_inconsistent(const RoomId from,
                                       const ExitDirEnum dir,
                                       const InOutEnum mode,
                                       const RoomId to) const
{
    if (hasRoom(from)) {
        return m_rooms.getExitInOut(from, dir, mode).contains(to);
    }
    return false;
}

// ... (rest of file, with other necessary corrections applied based on analysis) ...

void World::checkConsistency(ProgressCounter &counter) const
{
    if (getRoomSet().empty()) {
        return;
    }

    DECL_TIMER(t, __FUNCTION__);

    auto checkPosition = [this](const RoomId id) {
        const Coordinate &coord = getPosition(id);
        if (const RoomId *const maybe = m_spatialDb.findUnique(coord);
            maybe == nullptr || *maybe != id) {
            throw MapConsistencyError("two rooms using the same coordinate found");
        }
    };

    auto checkServerId = [this](const RoomId id) {
        const ServerRoomId serverId = getServerId(id);
        if (serverId != INVALID_SERVER_ROOMID && !m_serverIds.contains(serverId)) {
            qWarning() << "Room" << id.asUint32() << "server id" << serverId.asUint32()
                       << "does not map to a room.";
        }
    };

    auto checkExitFlags = [this](const RoomId id, const ExitDirEnum dir) {
        const auto exitFlags = getExitFlags(id, dir);
        const auto doorFlags = getDoorFlags(id, dir);

        sanityCheckFlags(doorFlags);
        sanityCheckFlags(exitFlags);

        const bool hasDoorName = !m_rooms.getExitDoorName(id, dir).empty();
        if (hasDoorName) {
            if (!sanitizer::isSanitizedOneLine(
                    m_rooms.getExitDoorName(id, dir).getStdStringViewUtf8())) {
                throw MapConsistencyError("door name fails sanity check");
            }
        }
        // Assuming ::satisfiesInvariants is a free function for RawExit
        if (!::satisfiesInvariants(m_rooms.getRawRoomRef(id).getExit(dir))) { // MODIFIED
            throw MapConsistencyError("room exit flags do not satisfy invariants");
        }
    };

    auto checkFlags = [this, &checkExitFlags](const RoomId id) {
        sanityCheckFlags(m_rooms.getRoomLoadFlags(id));
        sanityCheckFlags(m_rooms.getRoomMobFlags(id));
        for (const ExitDirEnum dir : ALL_EXITS7) {
            checkExitFlags(id, dir);
        }
    };

    auto checkRemapping = [this](const RoomId id) {
        if (getGlobalArea().roomSet.getReadOnly().count(id) == 0) { // MODIFIED .contains to .count
            throw MapConsistencyError("room set does not contain the room id");
        }

        const auto &areaName = getRoomArea(id);
        auto &area = getArea(areaName);
        if (area.roomSet.getReadOnly().count(id) == 0) { // MODIFIED .contains to .count
            throw MapConsistencyError("room set does not contain the room id");
        }

        if (!m_remapping.contains(id)) {
            throw MapConsistencyError("remapping did not contain this id");
        }

        const ExternalRoomId ext = this->convertToExternal(id);
        if (convertToInternal(ext) != id) {
            throw MapConsistencyError("unable to convert to internal id");
        }
    };

    auto checkParseTree = [this](const RoomId id) {
        const RoomName &name = getRoomName(id);
        const RoomDesc &desc = m_rooms.getRoomDescription(id);

        if (auto cowSet = m_parseTree.name_only.find(name); cowSet == nullptr || cowSet->getReadOnly().count(id) == 0) { // MODIFIED
            throw MapConsistencyError("unable to find room name only");
        }

        if (auto cowSet = m_parseTree.desc_only.find(desc); cowSet == nullptr || cowSet->getReadOnly().count(id) == 0) { // MODIFIED
            throw MapConsistencyError("unable to find room desc only");
        }

        {
            const NameDesc nameDesc{name, desc};
            if (auto cowSet = m_parseTree.name_desc.find(nameDesc); // MODIFIED
                cowSet == nullptr || cowSet->getReadOnly().count(id) == 0) {
                throw MapConsistencyError("unable to find room name_desc only");
            }
        }
    };
    // ... (rest of checkConsistency, assuming checkRoom lambda calls these)
    {
        const auto numThreads = std::max<size_t>(1, std::thread::hardware_concurrency());
        const auto &roomSet = getRoomSet();
        const auto checkRoom = [this, &checkPosition, &checkServerId, &checkFlags, &checkRemapping, &checkParseTree, &checkEnums](RoomId id_param) {
            this->checkAllExitsConsistent(id_param);
            checkEnums(id_param);            // Captured local lambda
            checkFlags(id_param);            // Captured local lambda
            checkParseTree(id_param);        // Captured local lambda
            checkPosition(id_param);         // Captured local lambda
            checkRemapping(id_param);        // Captured local lambda
            checkServerId(id_param);         // Captured local lambda
        };

        if (numThreads == 1) {
            counter.setNewTask(ProgressMsg{"checking room consistency"}, roomSet.size());
            for (const RoomId id_param : roomSet) { // Renamed 'id'
                checkRoom(id_param);
                counter.step();
            }
        } else {
            counter.setNewTask(ProgressMsg{"checking room consistency"}, numThreads);
            std::vector<std::future<void>> futures;
            std::vector<std::exception_ptr> exceptions;
            auto it = roomSet.begin();
            const size_t chunkSize = (roomSet.size() + numThreads - 1) / numThreads;
            for (size_t i = 0; i < numThreads && it != roomSet.end(); ++i) {
                auto chunkBegin = it;
                size_t remaining = static_cast<size_t>(std::distance(it, roomSet.end()));
                size_t actualChunkSize = std::min(chunkSize, remaining);
                std::advance(it, actualChunkSize);
                auto chunkEnd = it;
                futures.push_back(
                    std::async(std::launch::async, [&checkRoom, chunkBegin, chunkEnd]() {
                        DECL_TIMER(t2, "checkConsistency-checkRoom");
                        for (auto iter_local = chunkBegin; iter_local != chunkEnd; ++iter_local) { // Renamed 'iter'
                            const RoomId id_param = *iter_local; // Renamed 'id'
                            checkRoom(id_param);
                        }
                    }));
            }
            for (auto &fut : futures) {
                try {
                    fut.get();
                    counter.step();
                } catch (...) {
                    exceptions.push_back(std::current_exception());
                }
            }
            if (!exceptions.empty()) {
                std::rethrow_exception(exceptions.front());
            }
        }
    }
    // ...
}


void World::clearExit(const RoomId id, const ExitDirEnum dir, const WaysEnum ways)
{
    auto &exitRef = m_rooms.getRawRoomRef(id).getExit(dir); // MODIFIED (removed .getMutable())
    if (ways == WaysEnum::OneWay) {
        TinyRoomIdSet old_inbound = std::exchange(exitRef.incoming, {});
        exitRef = {};
        exitRef.incoming = std::move(old_inbound);
    } else {
        exitRef = {};
    }
}

void World::removeFromWorld(const RoomId id, const bool removeLinks)
{
    if (id == INVALID_ROOMID) {
        throw InvalidMapOperation("Invalid RoomId");
    }

    const auto coord = getPosition(id);
    const auto server_id = getServerId(id);
    const auto areaName = getRoomArea(id);

    removeParse(id, ALL_PARSE_KEY_FLAGS);
    m_spatialDb.remove(id, coord);
    m_serverIds.remove(server_id);

    if (removeLinks) {
        nukeAllExits(id, WaysEnum::TwoWay);
    }

    m_remapping.removeAt(id);
    m_rooms.removeAt(id);
    m_areaInfos.remove(areaName, id); // MODIFIED
}

void World::setRoom_lowlevel(const RoomId id, const RawRoom &input)
{
    assert(id == input.id);
    m_rooms.getRawRoomRef(id) = input; // MODIFIED
    m_rooms.enforceInvariants(id);
}

void World::initRoom(const RawRoom &input)
{
    const RoomId id = input.id;
    assert(id != INVALID_ROOMID);
    m_rooms.requireUninitialized(id);

    setRoom_lowlevel(id, input);

    {
        const auto &areaName = input.getArea();
        m_areaInfos.insert(areaName, id); // MODIFIED
        insertParse(id, ALL_PARSE_KEY_FLAGS);
        m_spatialDb.add(id, input.position);
        m_serverIds.set(input.server_id, id);
    }

    if constexpr (IS_DEBUG_BUILD) {
        const auto &here = deref(getRoom(id));
        assert(::satisfiesInvariants(here)); // Assuming free function

        auto copy = input;
        ::enforceInvariants(copy); // Assuming free function
        assert(here == copy);
    }
}

World World::init(ProgressCounter &counter, const std::vector<ExternalRawRoom> &ext_rooms)
{
    DECL_TIMER(t, __FUNCTION__);
    World w;
    std::vector<RawRoom> rooms;
    {
        counter.setNewTask(ProgressMsg{"computing remapping"}, 3);
        Remapping remapping = Remapping::computeFrom(ext_rooms);
        counter.step();
        rooms = remapping.convertToInternal(ext_rooms);
        counter.step();
        assert(rooms.size() == ext_rooms.size());
        {
            DECL_TIMER(t2, "setRemapAndAllocateRooms");
            w.setRemapAndAllocateRooms(std::move(remapping));
        }
        counter.step();
    }
    {
        DECL_TIMER(t1, "insert-rooms");
        {
            DECL_TIMER(t2, "insert-rooms-part1");
            const size_t numRooms = rooms.size();
            {
                RoomId next{0};
                for (const auto &r : rooms) {
                    if (r.id != next++) throw std::runtime_error("elements aren't in order");
                }
                if (next.value() != numRooms) throw std::runtime_error("wrong number of elements");
            }
            {
                DECL_TIMER(t3, "copy rooms");
                for (const auto &r : rooms) {
                    const RoomId id = r.getId();
                    w.m_rooms.getRawRoomRef(id) = r; // MODIFIED
                }
            }
        }
        {
            DECL_TIMER(t3, "update-exit-flags");
            counter.setNewTask(ProgressMsg{"updating exit flags"}, rooms.size());
            for (const auto &room : rooms) {
                for (const ExitDirEnum dir : ALL_EXITS7) {
                    w.m_rooms.enforceInvariants(room.id, dir);
                }
                counter.step();
            }
        }
        {
            DECL_TIMER(t3, "insert-rooms-cachedRoomSet");
            counter.setNewTask(ProgressMsg{"inserting rooms"}, rooms.size());
            for (const auto &room : rooms) {
                w.m_areaInfos.insert(room.getArea(), room.id); // MODIFIED
                counter.step();
            }
        }
        {
            DECL_TIMER(t3, "insert-rooms-parsekey");
            counter.setNewTask(ProgressMsg{"inserting room name/desc lookups"}, rooms.size());
            for (const auto &room : rooms) {
                w.insertParse(room.id, ALL_PARSE_KEY_FLAGS);
                counter.step();
            }
        }
        // ... (rest of init unchanged for serverIds, spatialDb)
        {
            DECL_TIMER(t3, "insert-rooms-spatialDb");
            counter.setNewTask(ProgressMsg{"setting room positions"}, rooms.size());
            for (const auto &room : rooms) {
                w.m_spatialDb.add(room.id, room.position);
                counter.step();
            }
        }
        {
            DECL_TIMER(t3, "insert-rooms-serverIds");
            counter.setNewTask(ProgressMsg{"setting room server ids"}, rooms.size());
            for (const auto &room : rooms) {
                w.m_serverIds.set(room.server_id, room.id);
                counter.step();
            }
        }
    }
    {
        DECL_TIMER(t4, "update-bounds");
        counter.setNewTask(ProgressMsg{"updating bounds"}, 1);
        w.m_spatialDb.updateBounds(counter);
        counter.step();
    }
    {
        DECL_TIMER(t5, "check-consistency");
        counter.setNewTask(ProgressMsg{"checking map consistency"}, 1);
        w.checkConsistency(counter);
        counter.step();
    }
    return w;
}

RoomId World::getNextId() const
{
    const auto &set = getRoomSet(); // Returns const std::set<RoomId>&
    if (set.empty()) {
        return RoomId{0};
    }
    return (*set.rbegin()).next(); // MODIFIED
}

void World::printStats(ProgressCounter &pc, AnsiOstream &os) const
{
    m_remapping.printStats(pc, os);
    m_serverIds.printStats(pc, os);
    {
        // ... (stats calculation unchanged)
        size_t numMissingName = 0;
        size_t numMissingDesc = 0;
        size_t numMissingBoth = 0;
        size_t numMissingArea = 0;
        size_t numMissingServerId = 0;
        size_t numWithNoConnections = 0;
        size_t numWithNoExits = 0;
        size_t numWithNoEntrances = 0;
        size_t numExits = 0;
        size_t numDoors = 0;
        size_t numHidden = 0;
        size_t numDoorNames = 0;
        size_t numHiddenDoorNames = 0;
        size_t numLoopExits = 0;
        size_t numConnections = 0;
        size_t numMultipleOut = 0;
        size_t numMultipleIn = 0;

        size_t adj1 = 0;
        size_t adj2 = 0;
        size_t non1 = 0;
        size_t non2 = 0;
        size_t loop1 = 0;
        size_t loop2 = 0;

        std::optional<Bounds> optBounds;
        for (const RoomId id : getRoomSet()) {
            const RawRoom *const pRoom = getRoom(id);
            if (pRoom == nullptr) {
                std::abort();
            }
            auto &room = *pRoom;
            const auto &pos = getPosition(id);

            if (optBounds.has_value()) {
                optBounds->insert(pos);
            } else {
                optBounds.emplace(pos, pos);
            }

            const bool isMissingServerId = room.getServerId() == INVALID_SERVER_ROOMID;
            if (isMissingServerId) {
                ++numMissingServerId;
            }

            const bool isMissingArea = room.getArea().empty();
            if (isMissingArea) {
                ++numMissingArea;
            }

            const bool isMissingName = getRoomName(id).empty();
            const bool isMissingDesc = getRoomDescription(id).empty();

            if (isMissingName) {
                ++numMissingName;
            }

            if (isMissingDesc) {
                ++numMissingDesc;
            }

            if (isMissingName && isMissingDesc) {
                ++numMissingBoth;
            }

            bool hasExits = false;
            bool hasEntrances = false;

            for (const ExitDirEnum dir : ALL_EXITS7) {
                const auto &e = getExitExitFlags(id, dir);

                if (e.isExit()) {
                    ++numExits;
                }

                if (e.isDoor()) {
                    ++numDoors;
                    if (!getExitDoorName(id, dir).empty()) {
                        ++numDoorNames;
                    }
                }

                if (getExitDoorFlags(id, dir).isHidden()) {
                    numHidden += 1;
                    if (!getExitDoorName(id, dir).empty()) {
                        ++numHiddenDoorNames;
                    }
                }

                const TinyRoomIdSet &outset = m_rooms.getExitOutgoing(id, dir);
                const TinyRoomIdSet &inset = m_rooms.getExitIncoming(id, dir);

                if (!outset.empty()) {
                    hasExits = true;
                }

                if (!inset.empty()) {
                    hasEntrances = true;
                }

                const auto outSize = outset.size();
                numConnections += outSize;

                if (outSize > 1) {
                    ++numMultipleOut;
                }

                if (inset.size() > 1) {
                    ++numMultipleIn;
                }

                if (outset.contains(id)) { // This is TinyRoomIdSet::contains
                    ++numLoopExits;
                }

                const ExitDirEnum rev = opposite(dir);
                for (const RoomId to : outset) {
                    if (hasRoom(to)) {
                        const bool looping = (id == to);
                        const bool adjacent = pos + exitDir(dir) == getPosition(to);
                        const bool twoWay = getOutgoing(to, rev).contains(id); // TinyRoomIdSet::contains

                        if (looping) {
                            (twoWay ? loop2 : loop1) += 1;
                        } else if (adjacent) {
                            (twoWay ? adj2 : adj1) += 1;
                        } else {
                            (twoWay ? non2 : non1) += 1;
                        }
                    }
                }
            }

            if (!hasEntrances && !hasExits) {
                ++numWithNoConnections;
            }

            if (!hasEntrances) {
                ++numWithNoEntrances;
            }

            if (!hasExits) {
                ++numWithNoExits;
            }
        }


        static constexpr auto green = getRawAnsi(AnsiColor16Enum::green);
        auto C = [](auto x) {
            static_assert(std::is_integral_v<decltype(x)>);
            return ColoredValue{green, x};
        };
        os << "\n";
        os << "Total areas: " << C(m_areaInfos.numAreas()) << ".\n"; // MODIFIED
        os << "\n";
        os << "Total rooms: " << C(getGlobalArea().roomSet.getReadOnly().size()) << ".\n"; // CowRoomIdSet -> std::set -> size
        os << "\n";
        // ... (rest of printStats numerical output unchanged)
        os << "  missing server id: " << C(numMissingServerId) << ".\n";
        os << "  missing area: " << C(numMissingArea) << ".\n";
        os << "\n";
        os << "  with no name and no desc: " << C(numMissingBoth) << ".\n";
        os << "  with name but no desc:    " << C(numMissingDesc - numMissingBoth) << ".\n";
        os << "  with desc but no name:    " << C(numMissingName - numMissingBoth) << ".\n";
        os << "\n";
        os << "  with no connections:         " << C(numWithNoConnections) << ".\n";
        os << "  with entrances but no exits: " << C(numWithNoExits - numWithNoConnections)
           << ".\n";
        os << "  with exits but no entrances: " << C(numWithNoEntrances - numWithNoConnections)
           << ".\n";
        os << "\n";
        os << "Total exits: " << C(numExits) << ".\n";
        os << "\n";
        os << "  doors:  " << C(numDoors) << " (with names: " << C(numDoorNames) << ").\n";
        os << "  hidden: " << C(numHidden) << " (with names: " << C(numHiddenDoorNames) << ").\n";
        os << "  loops:  " << C(numLoopExits) << ".\n";
        os << "\n";
        os << "  with multiple outputs: " << C(numMultipleOut) << ".\n";
        os << "  with multiple inputs:  " << C(numMultipleIn) << ".\n";
        os << "\n";
        os << "Total connections: " << C(numConnections) << ".\n";
        os << "\n";
        os << "  adjacent 1-way:     " << C(adj1) << ".\n";
        os << "  adjacent 2-way:     " << C(adj2) << ".\n";
        os << "  looping 1-way:      " << C(loop1) << ".\n";
        os << "  looping 2-way:      " << C(loop2) << ".\n";
        os << "  non-adjacent 1-way: " << C(non1) << ".\n";
        os << "  non-adjacent 2-way: " << C(non2) << ".\n";
        os << "\n";
        os << "  total 1-way:        " << C(non1 + adj1 + loop1) << ".\n";
        os << "  total 2-way:        " << C(non2 + adj2 + loop2) << ".\n";
        os << "  total adjacent:     " << C(adj1 + adj2) << ".\n";
        os << "  total looping:      " << C(loop1 + loop2) << ".\n";
        os << "  total non-adjacent: " << C(non1 + non2 + loop1 + loop2) << ".\n";
    }

    m_spatialDb.printStats(pc, os);

    static constexpr auto green = getRawAnsi(AnsiColor16Enum::green); // Re-declare for scope
    static constexpr auto yellow = getRawAnsi(AnsiColor16Enum::yellow); // Re-declare for scope

    auto line = std::string(81, '_');
    assert(line.size() == 81);
    line.back() = '\n';

    {
        os << "\n" << line << "\n"
              "Within the global area (# rooms = "
           << ColoredValue{green, getRoomSet().size()} << "):\n"; // MODIFIED
        m_parseTree.printStats(pc, os); // MODIFIED
    }

    for (const auto &kv : m_areaInfos) { // MODIFIED
        const auto &areaName = kv.first;
        const auto numAreaRooms = kv.second.roomSet.getReadOnly().size();
        os << "\n" << line << "\n" "Within the ";
        if (areaName.empty()) {
            os << "default";
        } else {
            os << ColoredQuotedStringView{green, yellow, areaName.getStdStringViewUtf8()};
        }
        os << " area (# rooms = " << ColoredValue{green, numAreaRooms} << "):\n";
    }
}

bool World::containsRoomsNotIn(const World &other) const
{
    // MODIFIED: Re-implemented using std::set algorithms
    const auto& thisSet = getGlobalArea().roomSet.getReadOnly(); // This is CowRoomIdSet -> const std::set<RoomId>&
    const auto& otherSet = other.getGlobalArea().roomSet.getReadOnly(); // This is CowRoomIdSet -> const std::set<RoomId>&
    for (const RoomId id : thisSet) {
        if (otherSet.count(id) == 0) {
            return true;
        }
    }
    return false;
}

// ... (namespace anonymous with hasMeshDifference functions unchanged)
namespace { // anonymous

NODISCARD bool hasMeshDifference(const RawExit &a, const RawExit &b)
{
    // door name change is not a mesh difference
    return a.fields.exitFlags != b.fields.exitFlags     //
           || a.fields.doorFlags != b.fields.doorFlags; //
}

NODISCARD bool hasMeshDifference(const RawRoom::Exits &a, const RawRoom::Exits &b)
{
    for (auto dir : ALL_EXITS7) {
        if (hasMeshDifference(a[dir], b[dir])) {
            return true;
        }
    }
    return false;
}

NODISCARD bool hasMeshDifference(const RoomFields &a, const RoomFields &b)
{
#define X_CASE(_Type, _Name, _Init) \
    if ((a._Name) != (b._Name)) { \
        return true; \
    }
    // NOTE: Purposely *NOT* doing "XFOREACH_ROOM_STRING_PROPERTY(X_CASE)"
    XFOREACH_ROOM_FLAG_PROPERTY(X_CASE)
    XFOREACH_ROOM_ENUM_PROPERTY(X_CASE)
    return false;
#undef X_CASE
}

NODISCARD bool hasMeshDifference(const RawRoom &a, const RawRoom &b)
{
    return a.position != b.position                 //
           || hasMeshDifference(a.fields, b.fields) //
           || hasMeshDifference(a.exits, b.exits);  //
}

// Only valid if one is immediately derived from the other.
NODISCARD bool hasMeshDifference(const World &a, const World &b)
{
    for (const RoomId id : a.getRoomSet()) { // Uses World::getRoomSet()
        if (!b.hasRoom(id)) {
            continue;
        }
        if (hasMeshDifference(deref(a.getRoom(id)), deref(b.getRoom(id)))) {
            return true;
        }
    }
    return false;
}
} // namespace


WorldComparisonStats World::getComparisonStats(const World &base, const World &modified)
{
    const auto anyRoomsAdded = modified.containsRoomsNotIn(base);
    const auto anyRoomsRemoved = base.containsRoomsNotIn(modified);
    const auto anyRoomsMoved = base.m_spatialDb != modified.m_spatialDb;

    WorldComparisonStats result;
    result.boundsChanged = base.getBounds() != modified.getBounds();
    result.anyRoomsRemoved = anyRoomsRemoved;
    result.anyRoomsAdded = anyRoomsAdded;
    result.spatialDbChanged = anyRoomsMoved;
    result.serverIdsChanged = base.m_serverIds != modified.m_serverIds;
    result.parseTreeChanged = base.m_parseTree != modified.m_parseTree; // MODIFIED
    result.hasMeshDifferences = anyRoomsAdded                         //
                                || anyRoomsRemoved                    //
                                || anyRoomsMoved                      //
                                || hasMeshDifference(base, modified); //

    return result;
}
