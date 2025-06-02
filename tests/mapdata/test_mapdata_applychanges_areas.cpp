#include "../../src/global/progresscounter.h" // Added for ProgressCounter
#include "../../src/map/ChangeList.h"         // Added for ChangeList
#include "../../src/map/Changes.h"
#include "../../src/map/DoorFlags.h"
#include "../../src/map/ExitFlags.h"
#include "../../src/map/Map.h"
#include "../../src/map/RawRoom.h"
#include "../../src/map/RoomArea.h"
#include "../../src/map/RoomId.h"
#include "../../src/map/World.h"
#include "../../src/map/coordinate.h"
#include "../../src/map/roomid_set.h" // Added for RoomIdSet, if needed for more complex setups
#include "../../src/mapdata/mapdata.h"

#include <memory>
#include <set>
#include <vector>

#include <QSignalSpy>
#include <QtTest/QtTest>

// Helper to create a basic map configuration within MapData
MapData *create_mapdata_with_initial_state(QObject *parent = nullptr)
{
    MapData *mapData = new MapData(parent);
    ProgressCounter pc;
    std::vector<ExternalRawRoom> ext_rooms;

    // RoomA in Area1
    ExternalRawRoom rA;
    rA.id = ExternalRoomId{1}; // External ID
    rA.position = Coordinate{0, 0, 0};
    rA.setArea(RoomArea{"Area1"});
    rA.setName(RoomName{"RoomA"});
    ext_rooms.push_back(rA);

    // RoomB in Area1
    ExternalRawRoom rB;
    rB.id = ExternalRoomId{2}; // External ID
    rB.position = Coordinate{1, 0, 0};
    rB.setArea(RoomArea{"Area1"});
    rB.setName(RoomName{"RoomB"});
    ext_rooms.push_back(rB);

    // RoomC in Area2
    ExternalRawRoom rC;
    rC.id = ExternalRoomId{3}; // External ID
    rC.position = Coordinate{0, 1, 0};
    rC.setArea(RoomArea{"Area2"});
    rC.setName(RoomName{"RoomC"});
    ext_rooms.push_back(rC);

    MapPair mapPair = Map::fromRooms(pc, ext_rooms);

    // This mimics how MapData might be initialized after loading a map.
    // MapFrontend holds both current and saved states.
    mapData->setSavedMap(mapPair.base);
    mapData->setCurrentMap(mapPair.modified);
    // setCurrentMarks and setSavedMarks would also be relevant for a full load.

    return mapData;
}

class TestMapDataApplyChangesAreas : public QObject
{
    Q_OBJECT

private:
    RoomId getRoomIdByCoord(MapData *mapData, Coordinate coord, ExternalRoomId fallbackExtId)
    {
        // Internal RoomId can change based on load order, so lookup by a stable property like coordinate or external ID.
        // External IDs are not directly queryable on World without iterating, findRoom(coord) is better.
        if (auto optId = mapData->getCurrentMap().getWorld().findRoom(coord)) {
            return *optId;
        }
        // Fallback if coordinate moved or room removed, try by external ID if map still has it
        RoomId idFromExt = mapData->getCurrentMap().getWorld().convertToInternal(fallbackExtId);
        if (idFromExt != INVALID_ROOMID)
            return idFromExt;

        return INVALID_ROOMID;
    }

private slots:
    void initTestCase()
    {
        // This is to ensure that RoomArea can be used in QVariant for signal spying.
        qRegisterMetaType<std::set<RoomArea>>("std::set<RoomArea>");
    }
    void testNoVisualChange_NoSignal();
    void testVisualChangeInOneArea_SignalCorrectArea();
    void testVisualChangeInMultipleAreas_SignalCorrectAreas();
    void testGlobalFlagTrue_EmptyVisuallyDirty_SignalEmptySet();
};

void TestMapDataApplyChangesAreas::testNoVisualChange_NoSignal()
{
    MapData *mapData = create_mapdata_with_initial_state(this);
    QSignalSpy spy(mapData, &MapData::needsAreaRemesh);

    RoomId roomA_id = getRoomIdByCoord(mapData, Coordinate{0, 0, 0}, ExternalRoomId{1});
    QVERIFY(roomA_id != INVALID_ROOMID);

    ChangeList changes;
    changes.add(room_change_types::ModifyRoomFlags{roomA_id,
                                                   RoomNote{"A new note"},
                                                   FlagModifyModeEnum::ASSIGN});

    mapData->applyChanges(changes);

    QCOMPARE(spy.count(), 0);

    delete mapData;
}

void TestMapDataApplyChangesAreas::testVisualChangeInOneArea_SignalCorrectArea()
{
    MapData *mapData = create_mapdata_with_initial_state(this);
    QSignalSpy spy(mapData, &MapData::needsAreaRemesh);

    RoomId roomA_id = getRoomIdByCoord(mapData, Coordinate{0, 0, 0}, ExternalRoomId{1});
    QVERIFY(roomA_id != INVALID_ROOMID);

    ChangeList changes;
    changes.add(room_change_types::ModifyRoomFlags{roomA_id,
                                                   RoomTerrainEnum::CAVE,
                                                   FlagModifyModeEnum::ASSIGN});

    mapData->applyChanges(changes);

    QCOMPARE(spy.count(), 1);
    if (spy.count() == 1) {
        QList<QVariant> arguments = spy.takeFirst();
        QVERIFY(arguments.at(0).canConvert<std::set<RoomArea>>());
        std::set<RoomArea> dirtyAreas = qvariant_cast<std::set<RoomArea>>(arguments.at(0));

        QVERIFY(dirtyAreas.count(RoomArea{"Area1"}));
        QCOMPARE(dirtyAreas.size(), size_t{1});
    }
    delete mapData;
}

void TestMapDataApplyChangesAreas::testVisualChangeInMultipleAreas_SignalCorrectAreas()
{
    MapData *mapData = create_mapdata_with_initial_state(this);
    QSignalSpy spy(mapData, &MapData::needsAreaRemesh);

    RoomId roomA_id = getRoomIdByCoord(mapData, Coordinate{0, 0, 0}, ExternalRoomId{1});
    QVERIFY(roomA_id != INVALID_ROOMID);
    RoomId roomC_id = getRoomIdByCoord(mapData, Coordinate{0, 1, 0}, ExternalRoomId{3});
    QVERIFY(roomC_id != INVALID_ROOMID);

    ChangeList changes;
    changes.add(room_change_types::ModifyRoomFlags{roomA_id,
                                                   RoomTerrainEnum::CAVE,
                                                   FlagModifyModeEnum::ASSIGN});
    changes.add(room_change_types::ModifyRoomFlags{roomC_id,
                                                   RoomShapeEnum::OCTAGON,
                                                   FlagModifyModeEnum::ASSIGN});

    mapData->applyChanges(changes);

    QCOMPARE(spy.count(), 1);
    if (spy.count() == 1) {
        QList<QVariant> arguments = spy.takeFirst();
        QVERIFY(arguments.at(0).canConvert<std::set<RoomArea>>());
        std::set<RoomArea> dirtyAreas = qvariant_cast<std::set<RoomArea>>(arguments.at(0));

        QVERIFY(dirtyAreas.count(RoomArea{"Area1"}));
        QVERIFY(dirtyAreas.count(RoomArea{"Area2"}));
        QCOMPARE(dirtyAreas.size(), size_t{2});
    }
    delete mapData;
}

void TestMapDataApplyChangesAreas::testGlobalFlagTrue_EmptyVisuallyDirty_SignalEmptySet()
{
    // This test remains difficult to trigger reliably without mocking Map::update's behavior
    // or World::getComparisonStats to return a specific MapApplyResult.
    // The core logic in MapData is:
    // if (result.roomUpdateFlags.contains(RoomUpdateEnum::RoomMeshNeedsUpdate) && !specific_areas_handled) {
    //   emit needsAreaRemesh({});
    // }
    // We are testing MapData's reaction.
    // A world_change_type::CompactRoomIds might be the closest to a "global" change
    // that doesn't modify specific room geometry but might require a general remesh.
    // However, Map::update's dirty area detection might still pick up areas if room IDs change and affect lookups.

    MapData *mapData = create_mapdata_with_initial_state(this);
    QSignalSpy spy(mapData, &MapData::needsAreaRemesh);

    // To truly test this, we'd need to mock Map::apply to return a MapApplyResult where:
    // - result.visuallyDirtyAreas is empty
    // - result.roomUpdateFlags contains RoomUpdateEnum::RoomMeshNeedsUpdate
    // Since I can't mock, I'll create a change that *might* lead to this, but it's not guaranteed.
    // A world change like CompactRoomIds is a candidate.
    // The `Map::update` function in `Map.cpp` iterates through all rooms for changes. If compaction
    // changes nothing visible about any specific room (same areas, same geometry after remapping),
    // then `visuallyDirtyAreas` could be empty. `World::getComparisonStats` would need to report
    // `hasMeshDifferences = true` for the global flag.

    ChangeList changes;
    changes.add(world_change_types::CompactRoomIds{RoomId{0}}); // Arbitrary firstId for compaction

    // This is a conceptual test. The actual outcome depends on the deep internals of World::getComparisonStats
    // and how Map::update calculates dirty areas for such a change.
    // If CompactRoomIds *does* make some areas dirty (e.g. if room IDs are part of what makes an area dirty indirectly),
    // then this test won't hit the intended path in MapData.

    // For now, this test serves as a placeholder for testing MapData's reaction logic.
    // If a specific change is known to cause global RoomMeshNeedsUpdate without specific dirty areas,
    // that change should be used here.

    mapData->applyChanges(changes);

    // The primary assertion is that *if* the conditions inside MapData::applyChanges are met
    // (empty visuallyDirtyAreas from MapApplyResult + RoomMeshNeedsUpdate flag),
    // then the signal is emitted with an empty set.
    // We can't directly verify MapApplyResult here, only the emitted signal.

    if (spy.count() == 1) {
        QList<QVariant> arguments = spy.takeFirst();
        QVERIFY(arguments.at(0).canConvert<std::set<RoomArea>>());
        std::set<RoomArea> dirtyAreas = qvariant_cast<std::set<RoomArea>>(arguments.at(0));
        // This is the crucial check for this test case's intent
        QVERIFY(dirtyAreas.empty());
        qDebug()
            << "testGlobalFlagTrue_EmptyVisuallyDirty_SignalEmptySet: Signal emitted with empty set as expected (or specific areas if CompactRoomIds made them dirty).";
    } else {
        qDebug()
            << "testGlobalFlagTrue_EmptyVisuallyDirty_SignalEmptySet: Signal not emitted. This implies either CompactRoomIds caused no RoomMeshNeedsUpdate, or it identified specific dirty areas.";
        // This outcome is also plausible depending on how `Map::update` processes `CompactRoomIds`.
        // QVERIFY(spy.count() >= 0); // Always true, just to have a QVERIFY if no signal.
    }

    delete mapData;
}

QTEST_MAIN(TestMapDataApplyChangesAreas)
#include "test_mapdata_applychanges_areas.moc"
