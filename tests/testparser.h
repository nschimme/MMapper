#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../src/global/macros.h"

#include <QObject>

class NODISCARD_QOBJECT TestParser final : public QObject
{
    Q_OBJECT

public:
    TestParser();
    ~TestParser() final;

private Q_SLOTS:
    void parseUserCommandsTest();
    void testParseSpecialCommand_SetPrefix();
    void testParseSpecialCommand_Config();
    void testParseSpecialCommand_Map();

    // void testCompressDirections(); // Moved to TestParserUtils
    // void testGetTerrainSymbol();   // Moved to TestParserUtils
    // void testGetLightSymbol();     // Moved to TestParserUtils

    void testDoMove_Offline_QueueInteraction();

    // ParserUtils
    // static void createParseEventTest(); // Moved to TestParserUtils
    // static void removeAnsiMarksTest(); // Moved to TestParserUtils
    // static void toAsciiTest();         // Moved to TestParserUtils
};
