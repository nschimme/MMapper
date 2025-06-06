// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include <QtTest/QtTest> // For QtTest framework
#include <QObject>      // For QObject base class

#include "global/GlobalAppStateRAII.h"
#include "opengl/OpenGLProxyForTests.h"

// Includes for classes under test and dependencies
#include "display/MapCanvasRoomDrawer.h"
#include "display/MapBatches.h"
#include "display/MapCanvasData.h"
#include "display/RoadIndex.h"
#include "map/Map.h"
#include "map/RawRoom.h"
#include "map/RawExit.h"
#include "mapdata/mapdata.h"
#include "configuration/configuration.h"
#include "configuration/NamedConfig.h"
#include "global/TextUtils.h"
#include "map/RoomDefinition.h"
#include "map/RoomDefinitionHandle.h"
#include "map/RoomDefinitionList.h"
#include "map/DefaultRoomDefinitions.h"

// Using GTest's MOCK_METHOD for now. If this were a pure QtTest setup,
// a different mocking approach or manual stubs would be used.
// For this conversion, we'll assume GTest's mocking macros are still available and functional
// or would be replaced by an equivalent Qt-based mocking if this was a full migration.
#include <gmock/gmock.h>


// Mock for MapData to control RoomDefinitionHandle resolution
class MockMapData : public MapData {
public:
    MockMapData() : MapData(nullptr, nullptr, nullptr) {
        m_roomDefinitions = std::make_shared<RoomDefinitionList>();
        RoomDefinitionData data0; data0.setRoomName(to_QString("Default Def 0")); data0.setDefaultTerrainType(RoomTerrainEnum::PLAINS);
        m_roomDefinitions->v_addDefinition(data0, RoomDefinitionHandle{0});
        RoomDefinitionData data1; data1.setRoomName(to_QString("Default Def 1")); data1.setDefaultTerrainType(RoomTerrainEnum::FOREST);
        m_roomDefinitions->v_addDefinition(data1, RoomDefinitionHandle{1});
    }

    MOCK_METHOD(const RoomDefinitionHandle&, getRoomDefinitionHandle, (RoomId roomId), (const, override));

    const RoomDefinition& getRoomDefinition(const RoomDefinitionHandle& handle) const override {
        if (mock_getRoomDefinition_override) {
            return mock_getRoomDefinition_override(handle);
        }
        auto def = m_roomDefinitions->v_getDefinition(handle);
        if (def) return *def;
        static RoomDefinition fallback_def_data;
        fallback_def_data.setRoomName(to_QString("Fallback Mocked Definition"));
        std::cerr << "Warning: MockMapData returning fallback for handle " << handle.toInt() << std::endl;
        return fallback_def_data;
    }
    std::function<const RoomDefinition&(const RoomDefinitionHandle&)> mock_getRoomDefinition_override;

    MOCK_METHOD(RoomDefinitionHandle, findOrCreateRoomDefinition, (const RoomDefinitionData& data), (override));

    RoomDefinitionHandle addDefinitionToMock(const RoomDefinitionData& data) {
        return m_roomDefinitions->v_addDefinition(data);
    }
    RoomDefinitionHandle addDefinitionToMock(const RoomDefinitionData& data, const RoomDefinitionHandle& handle) {
         m_roomDefinitions->v_addDefinition(data, handle);
         return handle;
    }

    MOCK_METHOD(void, setModified, (bool modified), (override));
    MOCK_METHOD(bool, isModified, (), (const, override));
    MOCK_METHOD(std::optional<QString>, getFilename, (), (const, override));
    MOCK_METHOD(void, setFilename, (const QString& filename), (override));
    MOCK_METHOD(RoomDefinitionHandle, getDefaultRoomDefinitionHandle, (), (const, override));
    MOCK_METHOD(const RoomDefinitionList&, getRoomDefinitions, (), (const, override));
    MOCK_METHOD(RoomDefinitionList&, getRoomDefinitionsForUpdate, (), (override));
    MOCK_METHOD(QStringList, getAreaNames, (), (const, override));
    MOCK_METHOD(const ColorPalette&, getPalette, (), (const, override));
    MOCK_METHOD(ColorPalette&, getPaletteForUpdate, (), (override));
    MOCK_METHOD(void, setPalette, (const ColorPalette& palette), (override));
    MOCK_METHOD(const InfomarkDataList&, getInfomarks, (), (const, override));
    MOCK_METHOD(InfomarkDataList&, getInfomarksForUpdate, (), (override));
    MOCK_METHOD(const StringDataList&, getStrings, (), (const, override));
    MOCK_METHOD(StringDataList&, getStringsForUpdate, (), (override));
    MOCK_METHOD(const StubDataList&, getStubs, (), (const, override));
    MOCK_METHOD(StubDataList&, getStubsForUpdate, (), (override));
};


// Test fixture
class DisplayTest : public QObject { // Inherit from QObject
    Q_OBJECT // Add Q_OBJECT macro

protected:
    GlobalAppStateRAII globalAppStateRAII;
    OpenGLProxyForTests glProxy;
    testing::NiceMock<MockMapData> mockMapDataInstance;
    MapData* oldMapDataPtr = nullptr;

public: // Constructor must be public for QTEST_MAIN
    DisplayTest() {
        Configuration::instance().initEmptyForTests();
        oldMapDataPtr = GlobalAppState::instance().mapData;
        GlobalAppState::instance().mapData = &mockMapDataInstance;

        RoomDefinitionData defData0;
        defData0.setRoomName(to_QString("Type A Room"));
        defData0.setAreaName(to_QString("Area A"));
        defData0.setNote(to_QString("Note A"));
        defData0.setDefaultTerrainType(RoomTerrainEnum::PLAINS);
        mockMapDataInstance.addDefinitionToMock(defData0, RoomDefinitionHandle{0});

        RoomDefinitionData defData1;
        defData1.setRoomName(to_QString("Type B Room"));
        defData1.setAreaName(to_QString("Area B"));
        defData1.setNote(to_QString("Note B"));
        defData1.setDefaultTerrainType(RoomTerrainEnum::FOREST);
        mockMapDataInstance.addDefinitionToMock(defData1, RoomDefinitionHandle{1});

        ON_CALL(mockMapDataInstance, getDefaultRoomDefinitionHandle())
            .WillByDefault(testing::Return(RoomDefinitionHandle{0}));

        ON_CALL(mockMapDataInstance, getRoomDefinition(testing::_))
            .WillByDefault(testing::Invoke(&mockMapDataInstance, &MockMapData::getRoomDefinition_concrete));
    }

    // Using QObject destructor (or a cleanup slot if needed)
    ~DisplayTest() override {
        GlobalAppState::instance().mapData = oldMapDataPtr;
    }

    std::shared_ptr<Map> createTestMap() {
        auto map = std::make_shared<Map>();

        map->createRoom(10, Coordinate(0, 0, 0), RoomStatusEnum::Permanent, RoomDefinitionHandle{0});
        map->createRoom(11, Coordinate(1, 0, 0), RoomStatusEnum::Permanent, RoomDefinitionHandle{0});
        map->createRoom(12, Coordinate(0, 1, 0), RoomStatusEnum::Permanent, RoomDefinitionHandle{0});
        map->createRoom(13, Coordinate(1, 1, 0), RoomStatusEnum::Permanent, RoomDefinitionHandle{1});

        auto room10Handle = map->getRoomHandle(RoomId{10});
        if (room10Handle) {
            RawExit northExit;
            northExit.setExitFlags(ExitFlags{ExitFlagEnum::EXISTS});
            room10Handle.getRawForUpdate().getExitsForUpdate()[ExitDirEnum::NORTH] = northExit;
        }

        map->createRoom(20, Coordinate(0, 0, 1), RoomStatusEnum::Permanent, RoomDefinitionHandle{0});
        map->createRoom(21, Coordinate(1, 0, 1), RoomStatusEnum::Permanent, RoomDefinitionHandle{1});

        return map;
    }

    RoomGeometry populateRoomGeometryForTest(const RoomHandle& room) {
        RoomGeometry geometry;
        const auto& rawRoom = room.getRaw();

        geometry.loadFlags = rawRoom.getLoadFlags();
        geometry.mobFlags = rawRoom.getMobFlags();
        geometry.lightType = rawRoom.getLightType();
        geometry.ridableType = rawRoom.getRidableType();
        geometry.sundeathType = rawRoom.getSundeathType();
        geometry.terrainType = rawRoom.getTerrainType();
        geometry.roadIndex = getRoadIndex(rawRoom);

        const Map& map_ref = room.getMap();

        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            const RawExit& exit_data = rawRoom.getExit(dir);
            RoomExitGeometry& exitGeom = geometry.exits[dir];

            exitGeom.exitFlags = exit_data.getExitFlags();
            exitGeom.doorFlags = exit_data.getDoorFlags();
            exitGeom.outIsEmpty = exit_data.outIsEmpty();

            exitGeom.hasIncomingStream = false;
            if (!exit_data.inIsEmpty()) {
                for (const RoomId targetRoomId : exit_data.getIncomingSet()) {
                    const RoomHandle targetRoom = map_ref.getRoomHandle(targetRoomId);
                    if (!targetRoom.isValid()) continue;

                    for (const ExitDirEnum targetDir : ALL_EXITS_NESWUD) {
                        const RawExit& targetExit = targetRoom.getExit(targetDir);
                        if (targetExit.getExitFlags().isFlow() && targetExit.containsOut(room.getRoomId())) {
                            exitGeom.hasIncomingStream = true;
                            break;
                        }
                    }
                    if (exitGeom.hasIncomingStream) break;
                }
            }
        }
        return geometry;
    }

private slots: // Test methods are now private slots
    void testRoomGeometryHashAndEquality() {
        RoomGeometry geom1, geom2, geom3;

        geom1.terrainType = RoomTerrainEnum::PLAINS;
        geom1.loadFlags = RoomLoadFlags{RoomLoadFlagEnum::WATER};
        geom1.exits[ExitDirEnum::NORTH].exitFlags = ExitFlags{ExitFlagEnum::EXISTS};
        geom1.exits[ExitDirEnum::NORTH].hasIncomingStream = true;
        geom1.exits[ExitDirEnum::NORTH].outIsEmpty = true;

        geom2.terrainType = RoomTerrainEnum::PLAINS;
        geom2.loadFlags = RoomLoadFlags{RoomLoadFlagEnum::WATER};
        geom2.exits[ExitDirEnum::NORTH].exitFlags = ExitFlags{ExitFlagEnum::EXISTS};
        geom2.exits[ExitDirEnum::NORTH].hasIncomingStream = true;
        geom2.exits[ExitDirEnum::NORTH].outIsEmpty = true;

        geom3.terrainType = RoomTerrainEnum::FOREST;
        geom3.loadFlags = RoomLoadFlags{RoomLoadFlagEnum::WATER};
        geom3.exits[ExitDirEnum::NORTH].exitFlags = ExitFlags{ExitFlagEnum::EXISTS};
        geom3.exits[ExitDirEnum::NORTH].hasIncomingStream = true;
        geom3.exits[ExitDirEnum::NORTH].outIsEmpty = true;

        QCOMPARE(geom1, geom2); // "Identical RoomGeometry objects should be equal."
        QVERIFY(geom1 != geom3); // "Different RoomGeometry objects should not be equal."

        std::hash<RoomGeometry> hasher;
        QCOMPARE(hasher(geom1), hasher(geom2)); // "Hashes for identical RoomGeometry objects should be equal."
    }

    void testRoomGeometryPopulation() {
        auto map = createTestMap();
        auto roomHandle = map->getRoomHandle(RoomId{10});
        QVERIFY(roomHandle); // ASSERT_TRUE

        RawRoom& raw = roomHandle.getRawForUpdate();
        raw.setLightType(RoomLightEnum::DARK);
        raw.setLoadFlags(RoomLoadFlags{RoomLoadFlagEnum::FOOD});
        raw.getExitsForUpdate()[ExitDirEnum::EAST].setExitFlags(ExitFlags{ExitFlagEnum::EXISTS, ExitFlagEnum::DOOR});
        raw.getExitsForUpdate()[ExitDirEnum::EAST].setDoorFlags(DoorFlags{DoorFlagEnum::CLOSED});
        raw.getExitsForUpdate()[ExitDirEnum::WEST].setExitFlags(ExitFlags{ExitFlagEnum::EXISTS});

        RoomId neighborRoomId{11};
        auto neighborHandle = map->getRoomHandle(neighborRoomId);
        QVERIFY(neighborHandle);
        neighborHandle.getRawForUpdate().getExitsForUpdate()[ExitDirEnum::WEST].setExitFlags(ExitFlags{ExitFlagEnum::EXISTS, ExitFlagEnum::FLOW});
        neighborHandle.getRawForUpdate().getExitsForUpdate()[ExitDirEnum::WEST].getOutgoingSetForUpdate().insert(RoomId{10});
        raw.getExitsForUpdate()[ExitDirEnum::EAST].getIncomingSetForUpdate().insert(neighborRoomId);

        RoomGeometry expectedGeom;
        expectedGeom.lightType = RoomLightEnum::DARK;
        expectedGeom.terrainType = RoomTerrainEnum::PLAINS;
        expectedGeom.loadFlags = RoomLoadFlags{RoomLoadFlagEnum::FOOD};
        expectedGeom.mobFlags = RoomMobFlags{};
        expectedGeom.ridableType = RoomRidableEnum::UNDEFINED;
        expectedGeom.sundeathType = RoomSundeathEnum::UNDEFINED;
        expectedGeom.roadIndex = getRoadIndex(raw);

        expectedGeom.exits[ExitDirEnum::EAST].exitFlags = ExitFlags{ExitFlagEnum::EXISTS, ExitFlagEnum::DOOR};
        expectedGeom.exits[ExitDirEnum::EAST].doorFlags = DoorFlags{DoorFlagEnum::CLOSED};
        expectedGeom.exits[ExitDirEnum::EAST].outIsEmpty = true;
        expectedGeom.exits[ExitDirEnum::EAST].hasIncomingStream = true;

        expectedGeom.exits[ExitDirEnum::WEST].exitFlags = ExitFlags{ExitFlagEnum::EXISTS};
        expectedGeom.exits[ExitDirEnum::WEST].doorFlags = DoorFlags{};
        expectedGeom.exits[ExitDirEnum::WEST].outIsEmpty = true;
        expectedGeom.exits[ExitDirEnum::WEST].hasIncomingStream = false;

        expectedGeom.exits[ExitDirEnum::NORTH].exitFlags = ExitFlags{ExitFlagEnum::EXISTS};
        expectedGeom.exits[ExitDirEnum::NORTH].outIsEmpty = true;
        expectedGeom.exits[ExitDirEnum::NORTH].hasIncomingStream = false;

        RoomGeometry generatedGeom = populateRoomGeometryForTest(roomHandle);

        // Using QCOMPARE for direct value reporting on failure
        QCOMPARE(generatedGeom.loadFlags.value(), expectedGeom.loadFlags.value());
        QCOMPARE(generatedGeom.mobFlags.value(), expectedGeom.mobFlags.value());
        QCOMPARE(generatedGeom.lightType, expectedGeom.lightType);
        QCOMPARE(generatedGeom.ridableType, expectedGeom.ridableType);
        QCOMPARE(generatedGeom.sundeathType, expectedGeom.sundeathType);
        QCOMPARE(generatedGeom.terrainType, expectedGeom.terrainType);
        QCOMPARE(generatedGeom.roadIndex, expectedGeom.roadIndex);
        QCOMPARE(generatedGeom.exits[ExitDirEnum::NORTH], expectedGeom.exits[ExitDirEnum::NORTH]);
        QCOMPARE(generatedGeom.exits[ExitDirEnum::EAST], expectedGeom.exits[ExitDirEnum::EAST]);
        QCOMPARE(generatedGeom.exits[ExitDirEnum::WEST], expectedGeom.exits[ExitDirEnum::WEST]);
        QCOMPARE(generatedGeom.exits[ExitDirEnum::SOUTH], expectedGeom.exits[ExitDirEnum::SOUTH]);
        QCOMPARE(generatedGeom.exits[ExitDirEnum::UP], expectedGeom.exits[ExitDirEnum::UP]);
        QCOMPARE(generatedGeom.exits[ExitDirEnum::DOWN], expectedGeom.exits[ExitDirEnum::DOWN]);
        QCOMPARE(generatedGeom, expectedGeom); // "Generated RoomGeometry does not match expected."
    }

    void testInstancingLogicDataPreparation() {
        auto map = createTestMap();

        mctp::MapCanvasTexturesProxy texturesProxy(nullptr);
        VisitRoomOptions visitOptions;

        std::map<int, RoomVector> layerToRooms;
        for (RoomId id : map->getRooms()) {
            auto rh = map->getRoomHandle(id);
            layerToRooms[rh.getPosition().z].push_back(rh);
        }

        QCOMPARE(layerToRooms.size(), 2); // "Test map should have 2 layers."

        // --- Test Layer 0 ---
        {
            LayerBatchData layer0_batchData;
            LayerBatchBuilder layer0_builder(layer0_batchData, texturesProxy, OptBounds{});
            visitRooms(layerToRooms[0], texturesProxy, layer0_builder, visitOptions);

            QCOMPARE(layer0_batchData.sourceRoomForGeometry.size(), 3);
            QCOMPARE(layer0_batchData.roomInstanceTransforms.size(), 3);

            RoomGeometry expectedGeomA = populateRoomGeometryForTest(map->getRoomHandle(RoomId{11}));
            RoomGeometry expectedGeomB = populateRoomGeometryForTest(map->getRoomHandle(RoomId{13}));
            RoomGeometry expectedGeomC = populateRoomGeometryForTest(map->getRoomHandle(RoomId{10}));

            QVERIFY(layer0_batchData.sourceRoomForGeometry.count(expectedGeomA));
            QVERIFY(layer0_batchData.sourceRoomForGeometry.count(expectedGeomB));
            QVERIFY(layer0_batchData.sourceRoomForGeometry.count(expectedGeomC));

            QCOMPARE(layer0_batchData.roomInstanceTransforms.at(expectedGeomA).size(), size_t(2));
            QCOMPARE(layer0_batchData.roomInstanceTransforms.at(expectedGeomB).size(), size_t(1));
            QCOMPARE(layer0_batchData.roomInstanceTransforms.at(expectedGeomC).size(), size_t(1));

            glm::mat4 expectedTransform_Room11 = glm::translate(glm::mat4(1.0f), map->getRoomHandle(RoomId{11}).getPosition().to_vec3());
            bool found_room11_transform = false;
            for(const auto& transform : layer0_batchData.roomInstanceTransforms.at(expectedGeomA)){
                if(transform == expectedTransform_Room11){
                    found_room11_transform = true;
                    break;
                }
            }
            QVERIFY(found_room11_transform); // "Transform for room 11 (instance of GeomA) not found or incorrect on Layer 0."
        }

        // --- Test Layer 1 ---
        {
            LayerBatchData layer1_batchData;
            LayerBatchBuilder layer1_builder(layer1_batchData, texturesProxy, OptBounds{});
            visitRooms(layerToRooms[1], texturesProxy, layer1_builder, visitOptions);

            QCOMPARE(layer1_batchData.sourceRoomForGeometry.size(), 2);

            RoomGeometry expectedGeomA_L0 = populateRoomGeometryForTest(map->getRoomHandle(RoomId{11}));
            RoomGeometry expectedGeomB_L0 = populateRoomGeometryForTest(map->getRoomHandle(RoomId{13}));

            QVERIFY(layer1_batchData.sourceRoomForGeometry.count(expectedGeomA_L0));
            QVERIFY(layer1_batchData.sourceRoomForGeometry.count(expectedGeomB_L0));

            QCOMPARE(layer1_batchData.roomInstanceTransforms.at(expectedGeomA_L0).size(), size_t(1));
            QCOMPARE(layer1_batchData.roomInstanceTransforms.at(expectedGeomB_L0).size(), size_t(1));
        }
    }

    void testRoomComponentMeshesCreationBasic() {
        glProxy.makeCurrent();
        OpenGL& gl = glProxy.gl;

        auto map = createTestMap();
        auto room10Handle = map->getRoomHandle(RoomId{10});

        mctp::MapCanvasTexturesProxy texturesProxy(nullptr);
        VisitRoomOptions visitOptions;

        LayerBatchData batchData;
        LayerBatchBuilder builder(batchData, texturesProxy, OptBounds{});
        RoomVector layer0Rooms = {room10Handle};
        visitRooms(layer0Rooms, texturesProxy, builder, visitOptions);

        LayerMeshes layerMeshes = batchData.getMeshes(gl);

        RoomGeometry geomRoom10 = populateRoomGeometryForTest(room10Handle);
        QVERIFY(layerMeshes.uniqueRoomMeshes.count(geomRoom10));

        const RoomComponentMeshes& components = layerMeshes.uniqueRoomMeshes.at(geomRoom10);

        QVERIFY(components.terrain.isValid()); // "Terrain mesh for Room 10 should be valid."
        QVERIFY(!components.trails.isValid()); // "Trails mesh for Room 10 should be invalid (default setup)."
        QVERIFY(components.overlays.empty()); // "Overlays for Room 10 should be empty (default setup)."

        QVERIFY(!components.walls.empty()); // "Walls vector for Room 10 should not be empty."
        if (!components.walls.empty()) {
           QVERIFY(components.walls[0].isValid()); // "First wall mesh for Room 10 should be valid."
        }
        QVERIFY(components.doors.empty()); // "Doors for Room 10 should be empty (North exit is not a door)."
    }
};

QTEST_MAIN(DisplayTest)
// Include MOC file if Q_OBJECT is used and not automatically handled by build system
// For example, if not using qmake, you might need: #include "TestMapCanvasRoomDrawer.moc"
// However, modern CMake with AUTOGEN/AUTOMOC usually handles this.

```
