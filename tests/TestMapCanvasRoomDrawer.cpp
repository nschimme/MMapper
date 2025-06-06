// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include <gtest/gtest.h>
#include "global/GlobalAppStateRAII.h" // For global state management
#include "opengl/OpenGLProxyForTests.h" // For tests needing GL context

// Includes for classes under test and dependencies
#include "display/MapCanvasRoomDrawer.h"
#include "display/MapBatches.h"
#include "display/MapCanvasData.h" // For MouseSel, etc., if needed by helpers
#include "display/RoadIndex.h"     // For getRoadIndex
#include "map/Map.h"
#include "map/RawRoom.h"
#include "map/RawExit.h"
#include "mapdata/mapdata.h"        // For getMapData()
#include "configuration/configuration.h" // For getConfig()
#include "configuration/NamedConfig.h"   // For color access
#include "global/TextUtils.h" // For to_QString, etc.
#include "map/RoomDefinition.h"
#include "map/RoomDefinitionHandle.h"
#include "map/RoomDefinitionList.h" // Required by MapData for RoomDefinition storage
#include "map/DefaultRoomDefinitions.h" // For default room definitions in MapData mock if needed

// Mock for MapData to control RoomDefinitionHandle resolution
// Using a more complete mock based on common patterns.
class MockMapData : public MapData {
public:
    MockMapData() : MapData(nullptr, nullptr, nullptr) {
        // Ensure default definitions list is usable or provide a mock one.
        // The base MapData constructor might try to access/create default definitions.
        // For simplicity, we assume this is handled or we provide a dummy list.
        m_roomDefinitions = std::make_shared<RoomDefinitionList>();
        // Add a default definition for handle 0 and 1 if not done by base/test setup
        RoomDefinitionData data0; data0.setRoomName(to_QString("Default Def 0")); data0.setDefaultTerrainType(RoomTerrainEnum::PLAINS);
        m_roomDefinitions->v_addDefinition(data0, RoomDefinitionHandle{0}); // Use v_addDefinition
        RoomDefinitionData data1; data1.setRoomName(to_QString("Default Def 1")); data1.setDefaultTerrainType(RoomTerrainEnum::FOREST);
        m_roomDefinitions->v_addDefinition(data1, RoomDefinitionHandle{1}); // Use v_addDefinition
    }

    MOCK_METHOD(const RoomDefinitionHandle&, getRoomDefinitionHandle, (RoomId roomId), (const, override));

    const RoomDefinition& getRoomDefinition(const RoomDefinitionHandle& handle) const override {
        if (mock_getRoomDefinition_override) { // Changed name to avoid conflict
            return mock_getRoomDefinition_override(handle);
        }
        auto def = m_roomDefinitions->v_getDefinition(handle); // Use v_getDefinition
        if (def) return *def;
        // Fallback to a globally available default if specific mock not found, or throw.
        // For tests, throwing is often better if a specific setup is expected.
        static RoomDefinition fallback_def_data; // Static to return a stable reference
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

    // Make sure all pure virtual methods from MapDataInterface are overridden
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
class DisplayTest : public ::testing::Test {
protected:
    GlobalAppStateRAII globalAppStateRAII;
    OpenGLProxyForTests glProxy;
    testing::NiceMock<MockMapData> mockMapDataInstance;
    MapData* oldMapDataPtr = nullptr;

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

        // Override the getDefaultRoomDefinitionHandle to return a valid handle for tests if needed
        ON_CALL(mockMapDataInstance, getDefaultRoomDefinitionHandle())
            .WillByDefault(testing::Return(RoomDefinitionHandle{0}));

        // Default action for getRoomDefinition to use the concrete implementation
        ON_CALL(mockMapDataInstance, getRoomDefinition(testing::_))
            .WillByDefault(testing::Invoke(&mockMapDataInstance, &MockMapData::getRoomDefinition_concrete));

    }

    ~DisplayTest() override {
        GlobalAppState::instance().mapData = oldMapDataPtr;
    }


    // Helper to create a Map with some rooms for testing
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
            // Use getRawForUpdate to modify the RawRoom's exits
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
};

TEST_F(DisplayTest, RoomGeometryHashAndEquality) {
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


    ASSERT_EQ(geom1, geom2) << "Identical RoomGeometry objects should be equal.";
    ASSERT_NE(geom1, geom3) << "Different RoomGeometry objects should not be equal.";

    std::hash<RoomGeometry> hasher;
    ASSERT_EQ(hasher(geom1), hasher(geom2)) << "Hashes for identical RoomGeometry objects should be equal.";
}

TEST_F(DisplayTest, RoomGeometryPopulation) {
    auto map = createTestMap();
    auto roomHandle = map->getRoomHandle(RoomId{10});
    ASSERT_TRUE(roomHandle);

    RawRoom& raw = roomHandle.getRawForUpdate();
    raw.setLightType(RoomLightEnum::DARK);
    raw.setLoadFlags(RoomLoadFlags{RoomLoadFlagEnum::FOOD});
    raw.getExitsForUpdate()[ExitDirEnum::EAST].setExitFlags(ExitFlags{ExitFlagEnum::EXISTS, ExitFlagEnum::DOOR});
    raw.getExitsForUpdate()[ExitDirEnum::EAST].setDoorFlags(DoorFlags{DoorFlagEnum::CLOSED});
    raw.getExitsForUpdate()[ExitDirEnum::WEST].setExitFlags(ExitFlags{ExitFlagEnum::EXISTS});

    // Setup for incoming stream to EAST exit of room 10
    // Room 11 is used as neighbor
    RoomId neighborRoomId{11};
    auto neighborHandle = map->getRoomHandle(neighborRoomId);
    ASSERT_TRUE(neighborHandle);
    neighborHandle.getRawForUpdate().getExitsForUpdate()[ExitDirEnum::WEST].setExitFlags(ExitFlags{ExitFlagEnum::EXISTS, ExitFlagEnum::FLOW});
    neighborHandle.getRawForUpdate().getExitsForUpdate()[ExitDirEnum::WEST].getOutgoingSetForUpdate().insert(RoomId{10});
    raw.getExitsForUpdate()[ExitDirEnum::EAST].getIncomingSetForUpdate().insert(neighborRoomId);


    RoomGeometry expectedGeom;
    expectedGeom.lightType = RoomLightEnum::DARK;
    // Terrain type comes from RoomDefinitionHandle{0} which is PLAINS in the mock setup.
    expectedGeom.terrainType = RoomTerrainEnum::PLAINS;
    expectedGeom.loadFlags = RoomLoadFlags{RoomLoadFlagEnum::FOOD};
    expectedGeom.mobFlags = RoomMobFlags{};
    expectedGeom.ridableType = RoomRidableEnum::UNDEFINED; // Default from RawRoom
    expectedGeom.sundeathType = RoomSundeathEnum::UNDEFINED; // Default from RawRoom
    expectedGeom.roadIndex = getRoadIndex(raw);

    expectedGeom.exits[ExitDirEnum::EAST].exitFlags = ExitFlags{ExitFlagEnum::EXISTS, ExitFlagEnum::DOOR};
    expectedGeom.exits[ExitDirEnum::EAST].doorFlags = DoorFlags{DoorFlagEnum::CLOSED};
    expectedGeom.exits[ExitDirEnum::EAST].outIsEmpty = true;
    expectedGeom.exits[ExitDirEnum::EAST].hasIncomingStream = true;

    expectedGeom.exits[ExitDirEnum::WEST].exitFlags = ExitFlags{ExitFlagEnum::EXISTS};
    expectedGeom.exits[ExitDirEnum::WEST].doorFlags = DoorFlags{};
    expectedGeom.exits[ExitDirEnum::WEST].outIsEmpty = true;
    expectedGeom.exits[ExitDirEnum::WEST].hasIncomingStream = false;

    expectedGeom.exits[ExitDirEnum::NORTH].exitFlags = ExitFlags{ExitFlagEnum::EXISTS}; // From createTestMap
    expectedGeom.exits[ExitDirEnum::NORTH].outIsEmpty = true;
    expectedGeom.exits[ExitDirEnum::NORTH].hasIncomingStream = false;


    RoomGeometry generatedGeom = populateRoomGeometryForTest(roomHandle);
    // Individual field comparison for easier debugging if RoomGeometry::operator== has subtle issues
    ASSERT_EQ(generatedGeom.loadFlags.value(), expectedGeom.loadFlags.value());
    ASSERT_EQ(generatedGeom.mobFlags.value(), expectedGeom.mobFlags.value());
    ASSERT_EQ(generatedGeom.lightType, expectedGeom.lightType);
    ASSERT_EQ(generatedGeom.ridableType, expectedGeom.ridableType);
    ASSERT_EQ(generatedGeom.sundeathType, expectedGeom.sundeathType);
    ASSERT_EQ(generatedGeom.terrainType, expectedGeom.terrainType);
    ASSERT_EQ(generatedGeom.roadIndex, expectedGeom.roadIndex);
    ASSERT_EQ(generatedGeom.exits[ExitDirEnum::NORTH], expectedGeom.exits[ExitDirEnum::NORTH]);
    ASSERT_EQ(generatedGeom.exits[ExitDirEnum::EAST], expectedGeom.exits[ExitDirEnum::EAST]);
    ASSERT_EQ(generatedGeom.exits[ExitDirEnum::WEST], expectedGeom.exits[ExitDirEnum::WEST]);
    ASSERT_EQ(generatedGeom.exits[ExitDirEnum::SOUTH], expectedGeom.exits[ExitDirEnum::SOUTH]);
    ASSERT_EQ(generatedGeom.exits[ExitDirEnum::UP], expectedGeom.exits[ExitDirEnum::UP]);
    ASSERT_EQ(generatedGeom.exits[ExitDirEnum::DOWN], expectedGeom.exits[ExitDirEnum::DOWN]);
    ASSERT_EQ(generatedGeom, expectedGeom) << "Generated RoomGeometry does not match expected.";
}


TEST_F(DisplayTest, InstancingLogicDataPreparation) {
    auto map = createTestMap();

    mctp::MapCanvasTexturesProxy texturesProxy(nullptr);
    VisitRoomOptions visitOptions;

    std::map<int, RoomVector> layerToRooms;
    for (RoomId id : map->getRooms()) {
        auto rh = map->getRoomHandle(id);
        layerToRooms[rh.getPosition().z].push_back(rh);
    }

    ASSERT_EQ(layerToRooms.size(), 2) << "Test map should have 2 layers.";

    // --- Test Layer 0 ---
    {
        LayerBatchData layer0_batchData;
        LayerBatchBuilder layer0_builder(layer0_batchData, texturesProxy, OptBounds{});
        visitRooms(layerToRooms[0], texturesProxy, layer0_builder, visitOptions);

        // Expected unique geometries on Layer 0:
        // GeomA: Rooms 11, 12 (Def0, PLAINS, default exits)
        // GeomB: Room 13 (Def1, FOREST, default exits)
        // GeomC: Room 10 (Def0, PLAINS, N exit modified)
        ASSERT_EQ(layer0_batchData.sourceRoomForGeometry.size(), 3) << "Layer 0 should have 3 unique geometries.";
        ASSERT_EQ(layer0_batchData.roomInstanceTransforms.size(), 3) << "Layer 0 should have transforms for 3 unique geometries.";

        RoomGeometry expectedGeomA = populateRoomGeometryForTest(map->getRoomHandle(RoomId{11}));
        RoomGeometry expectedGeomB = populateRoomGeometryForTest(map->getRoomHandle(RoomId{13}));
        RoomGeometry expectedGeomC = populateRoomGeometryForTest(map->getRoomHandle(RoomId{10}));


        ASSERT_TRUE(layer0_batchData.sourceRoomForGeometry.count(expectedGeomA));
        ASSERT_TRUE(layer0_batchData.sourceRoomForGeometry.count(expectedGeomB));
        ASSERT_TRUE(layer0_batchData.sourceRoomForGeometry.count(expectedGeomC));

        ASSERT_EQ(layer0_batchData.roomInstanceTransforms.at(expectedGeomA).size(), 2) << "GeomA on Layer 0 should have 2 instances.";
        ASSERT_EQ(layer0_batchData.roomInstanceTransforms.at(expectedGeomB).size(), 1) << "GeomB on Layer 0 should have 1 instance.";
        ASSERT_EQ(layer0_batchData.roomInstanceTransforms.at(expectedGeomC).size(), 1) << "GeomC on Layer 0 should have 1 instance.";

        glm::mat4 expectedTransform_Room11 = glm::translate(glm::mat4(1.0f), map->getRoomHandle(RoomId{11}).getPosition().to_vec3());
        bool found_room11_transform = false;
        for(const auto& transform : layer0_batchData.roomInstanceTransforms.at(expectedGeomA)){
            if(transform == expectedTransform_Room11){
                found_room11_transform = true;
                break;
            }
        }
        ASSERT_TRUE(found_room11_transform) << "Transform for room 11 (instance of GeomA) not found or incorrect on Layer 0.";
    }

    // --- Test Layer 1 ---
    {
        LayerBatchData layer1_batchData;
        LayerBatchBuilder layer1_builder(layer1_batchData, texturesProxy, OptBounds{});
        visitRooms(layerToRooms[1], texturesProxy, layer1_builder, visitOptions);

        // Room 20 (Def0, PLAINS, default exits) -> GeomA (same as room 11/12)
        // Room 21 (Def1, FOREST, default exits) -> GeomB (same as room 13)
        ASSERT_EQ(layer1_batchData.sourceRoomForGeometry.size(), 2) << "Layer 1 should have 2 unique geometries.";

        RoomGeometry expectedGeomA_L0 = populateRoomGeometryForTest(map->getRoomHandle(RoomId{11})); // From Layer 0
        RoomGeometry expectedGeomB_L0 = populateRoomGeometryForTest(map->getRoomHandle(RoomId{13})); // From Layer 0


        ASSERT_TRUE(layer1_batchData.sourceRoomForGeometry.count(expectedGeomA_L0)); // GeomA from L0 should match GeomA from L1
        ASSERT_TRUE(layer1_batchData.sourceRoomForGeometry.count(expectedGeomB_L0)); // GeomB from L0 should match GeomB from L1

        ASSERT_EQ(layer1_batchData.roomInstanceTransforms.at(expectedGeomA_L0).size(), 1) << "GeomA on Layer 1 should have 1 instance.";
        ASSERT_EQ(layer1_batchData.roomInstanceTransforms.at(expectedGeomB_L0).size(), 1) << "GeomB on Layer 1 should have 1 instance.";
    }
}


TEST_F(DisplayTest, RoomComponentMeshesCreationBasic) {
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
    ASSERT_TRUE(layerMeshes.uniqueRoomMeshes.count(geomRoom10));

    const RoomComponentMeshes& components = layerMeshes.uniqueRoomMeshes.at(geomRoom10);

    ASSERT_TRUE(components.terrain.isValid()) << "Terrain mesh for Room 10 should be valid.";
    ASSERT_FALSE(components.trails.isValid()) << "Trails mesh for Room 10 should be invalid (default setup).";
    ASSERT_TRUE(components.overlays.empty()) << "Overlays for Room 10 should be empty (default setup).";

    ASSERT_FALSE(components.walls.empty()) << "Walls vector for Room 10 should not be empty.";
    if (!components.walls.empty()) {
       ASSERT_TRUE(components.walls[0].isValid()) << "First wall mesh for Room 10 should be valid.";
    }
    ASSERT_TRUE(components.doors.empty()) << "Doors for Room 10 should be empty (North exit is not a door).";
}

// main function for tests if not using a separate test runner main
// int main(int argc, char **argv) {
//     ::testing::InitGoogleTest(&argc, argv);
//     QApplication app(argc, argv); // Needed for Qt based components / global state
//     return RUN_ALL_TESTS();
// }

```
