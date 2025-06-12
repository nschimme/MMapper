// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "TestGroup.h"
#include "../src/configuration/configuration.h"
#include "../src/group/mmapper2group.h"
#include "../src/group/groupwidget.h" // For GroupModel
#include "../src/group/CGroupChar.h"
#include "../src/mapdata/mapdata.h"
#include "../src/proxy/GmcpMessage.h" // For crafting GMCP messages
#include "../src/global/JsonValue.h"   // For JsonObj
#include "../src/global/JsonArray.h"  // For JsonArray

// Constructor
TestGroup::TestGroup() = default;

// Destructor
TestGroup::~TestGroup() = default;

void TestGroup::initTestCase() {
    setConfig().read();
}

void TestGroup::cleanupTestCase() {
}

// Helper to create a basic GMCP message for adding/updating a group member
GmcpMessage createGroupMemberMessage(GroupId id, const QString& name, CharacterTypeEnum type, const QString& roomName = "Some Room") {
    JsonObj jsonObj;
    jsonObj.insert("id", static_cast<double>(id.asUint32())); // GMCP uses double for numbers
    jsonObj.insert("name", name.toStdString());
    jsonObj.insert("room", roomName.toStdString()); // GroupModel might use room name

    // Simulate 'type_is_npc' or similar field if Mmapper2Group uses it
    // This depends on Mmapper2Group::getCharacterType implementation
    if (type == CharacterTypeEnum::NPC) {
        jsonObj.insert("type_is_npc", true); // Assuming this key is checked
    } else {
        jsonObj.insert("type_is_npc", false);
    }
    // Add other minimal fields Mmapper2Group::updateChar might expect
    jsonObj.insert("hp", 100.0); // Using double for GMCP numbers
    jsonObj.insert("maxhp", 100.0);
    jsonObj.insert("mana", 100.0);
    jsonObj.insert("maxmana", 100.0);
    jsonObj.insert("moves", 100.0);
    jsonObj.insert("maxmoves", 100.0);


    // Char.Group.Add and Char.Group.Update have slightly different structures
    // For simplicity, we'll use a structure that can populate a character.
    // This might need to be Char.Group.Set for a full list.
    // Let's use Char.Group.Add and assume it works for test setup.
    return GmcpMessage("Char.Group.Add", jsonObj);
}

// Data for NPC Filtering Test
void TestGroup::testGroupModelNpcFiltering_data() {
    QTest::addColumn<bool>("filterEnabled");
    QTest::addColumn<int>("expectedRowCount");
    QTest::addColumn<QStringList>("expectedNamesInOrder");

    QStringList namesNoFilter = {"Player", "FriendlyPC", "GrumpyNPC"};
    QStringList namesFilterNPC = {"Player", "FriendlyPC"};

    QTest::newRow("Filtering Disabled") << false << 3 << namesNoFilter;
    QTest::newRow("Filtering Enabled") << true << 2 << namesFilterNPC;
}

// NPC Filtering Test
void TestGroup::testGroupModelNpcFiltering() {
    QFETCH(bool, filterEnabled);
    QFETCH(int, expectedRowCount);
    QFETCH(QStringList, expectedNamesInOrder);

    Mmapper2Group group(nullptr);
    MapData mapData;
    GroupModel model(&mapData, &group, nullptr);

    // Populate Mmapper2Group using slot_parseGmcpInput
    // 1. Player character (Self)
    GmcpMessage charNameMsg("Char.Name", JsonObj{{"name", "Player"}});
    group.slot_parseGmcpInput(charNameMsg);
    // Mmapper2Group::getSelf() is private, but Char.Name populates m_self.
    // Type YOU is set by getSelf() internally.

    // 2. PC character
    GmcpMessage pcMsg = createGroupMemberMessage(GroupId{1}, "FriendlyPC", CharacterTypeEnum::PC);
    group.slot_parseGmcpInput(pcMsg);

    // 3. NPC character
    GmcpMessage npcMsg = createGroupMemberMessage(GroupId{2}, "GrumpyNPC", CharacterTypeEnum::NPC);
    group.slot_parseGmcpInput(npcMsg);

    // Set filter configuration
    Configuration &config = setConfig();
    bool originalFilterNPCs = config.groupManager.filterNPCs;
    config.groupManager.filterNPCs = filterEnabled;

    model.resetModel(); // Crucial for the model to pick up changes

    QCOMPARE(model.rowCount(), expectedRowCount);

    for (int i = 0; i < expectedRowCount; ++i) {
        QModelIndex nameIndex = model.index(i, static_cast<int>(GroupModel::ColumnTypeEnum::NAME));
        QVERIFY2(nameIndex.isValid(),
                 qPrintable(QString("Index %1 for NAME (row %2) should be valid. Row count is %3, expected %4. Filter: %5")
                 .arg(nameIndex.row()).arg(i).arg(model.rowCount()).arg(expectedRowCount).arg(filterEnabled))));
        QVariant nameData = model.data(nameIndex, Qt::DisplayRole);
        QVERIFY2(nameData.isValid(),
                 qPrintable(QString("Name data for model row %1 should be valid. Expected: %2")
                 .arg(i).arg(expectedNamesInOrder.at(i))));
        QCOMPARE(nameData.toString(), expectedNamesInOrder.at(i));
    }

    config.groupManager.filterNPCs = originalFilterNPCs; // Restore
}


// Player Color Preference Test
void TestGroup::testPlayerColorPreference() {
    Mmapper2Group group(nullptr);

    // Initialize m_self by sending Char.Name
    GmcpMessage charNameMsg("Char.Name", JsonObj{{"name", "TestPlayer"}});
    group.slot_parseGmcpInput(charNameMsg);

    SharedGroupChar self = group.getGroupManagerApi().getMember(CharacterName{"TestPlayer"});
    QVERIFY(self);
    QVERIFY(self->isYou()); // getSelf() should set this.

    QColor originalColor = self->getColor();
    QColor testColor = QColor(Qt::blue);
    if (originalColor == testColor) {
        testColor = QColor(Qt::green); // Ensure test color is different
    }

    Configuration &config = setConfig();
    QColor originalConfigColor = config.groupManager.color; // Save global config state
    config.groupManager.color = testColor;

    group.slot_updateSelfColorFromConfig();

    QCOMPARE(self->getColor(), testColor);

    // Restore global config state
    config.groupManager.color = originalConfigColor;
    // Optionally, call slot_updateSelfColorFromConfig again if other tests depend on m_self's color matching config.
    // group.slot_updateSelfColorFromConfig();
}

QTEST_MAIN(TestGroup)
