add_compile_definitions(MM_TESTING)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

include_directories(SYSTEM ${GLM_INCLUDE_DIR})

if(MMAPPER_IS_DEBUG)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(
            -Wno-extra-semi-stmt
            -Wno-missing-noreturn
            -Wno-unreachable-code-return
            -Wno-unused-const-variable
            -Wno-unused-function
            -Wno-unused-parameter
            -Wno-unused-template
            -Wno-unused-variable
            -Werror=unused-result
        )
    endif()
endif()

# Clock
set(clock_SRCS
    ../src/clock/mumeclock.cpp
    ../src/clock/mumeclock.h
    ../src/clock/mumemoment.cpp
    ../src/clock/mumemoment.h
    ../src/observer/gameobserver.cpp
    ../src/observer/gameobserver.h
    ../src/proxy/GmcpMessage.cpp
    ../src/proxy/GmcpMessage.h
    )
set(TestClock_SRCS testclock.cpp)
add_executable(TestClock ${TestClock_SRCS} ${clock_SRCS})
add_dependencies(TestClock mm_global)
target_link_libraries(TestClock
        mm_global
        Qt5::Gui
        Qt5::Test
        Qt5::Widgets
        coverage_config)
set_target_properties(
  TestClock PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestClock COMMAND TestClock)

# Expandora
set(expandoracommon_SRCS
    ../src/configuration/configuration.cpp
    ../src/configuration/configuration.h
    ../src/parser/Abbrev.cpp
    )
set(TestExpandoraCommon_SRCS testexpandoracommon.cpp)
add_executable(TestExpandoraCommon ${TestExpandoraCommon_SRCS} ${expandoracommon_SRCS})
add_dependencies(TestExpandoraCommon mm_global mm_map)
target_link_libraries(TestExpandoraCommon
        mm_map
        mm_global
        Qt5::Gui
        Qt5::Network
        Qt5::Test
        Qt5::Widgets
        coverage_config)
set_target_properties(
  TestExpandoraCommon PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestExpandoraCommon COMMAND TestExpandoraCommon)

# Parser
set(parser_EXTRA_SRCS
    # Parser sources
    ../src/parser/abstractparser.cpp
    ../src/parser/AbstractParser-Actions.cpp
    ../src/parser/AbstractParser-Commands.cpp
    ../src/parser/AbstractParser-Config.cpp
    ../src/parser/AbstractParser-Group.cpp
    ../src/parser/AbstractParser-Mark.cpp
    ../src/parser/AbstractParser-Room.cpp
    ../src/parser/AbstractParser-Timer.cpp
    ../src/parser/AbstractParser-Utils.cpp
    ../src/parser/Action.cpp
    ../src/parser/Abbrev.cpp
    ../src/parser/CommandQueue.cpp
    ../src/parser/DoorAction.cpp
    ../src/parser/mumexmlparser.cpp
    ../src/parser/mumexmlparser-gmcp.cpp

    # Syntax sources
    ../src/syntax/Accept.cpp
    ../src/syntax/IArgument.cpp
    ../src/syntax/MatchResult.cpp
    ../src/syntax/ParserInput.cpp
    ../src/syntax/Sublist.cpp
    ../src/syntax/SyntaxArgs.cpp
    ../src/syntax/TokenMatcher.cpp
    ../src/syntax/TreeParser.cpp
    ../src/syntax/Value.cpp

    # Clock sources
    ../src/clock/mumeclock.cpp
    ../src/clock/mumemoment.cpp

    # Group sources
    ../src/group/mmapper2group.cpp
    ../src/group/CGroupChar.cpp
    ../src/group/ColorGenerator.cpp
    ../src/group/GroupManagerApi.cpp
    ../src/group/mmapper2character.cpp
    ../src/group/enums.cpp 

    # Timers sources
    ../src/timers/CTimers.cpp

    # Configuration sources
    ../src/configuration/configuration.cpp 
    ../src/configuration/PasswordConfig.cpp 

    # Observer sources
    ../src/observer/gameobserver.cpp

    # Proxy API sources (and their direct dependencies if simple)
    ../src/proxy/ProxyParserApi.cpp 
    ../src/proxy/GmcpMessage.cpp 
    ../src/proxy/GmcpModule.cpp
    ../src/proxy/GmcpUtils.cpp
    # ../src/proxy/proxy.cpp # Explicitly NOT adding this due to its complexity

    # MapData and MapFrontend sources (expected by AbstractParser and its components)
    ../src/mapdata/mapdata.cpp
    ../src/mapdata/roomfilter.cpp
    ../src/mapdata/roomselection.cpp
    ../src/mapdata/shortestpath.cpp
    ../src/mapdata/GenericFind.cpp
    ../src/mapdata/MarkerList.cpp
    ../src/mapfrontend/mapfrontend.cpp
    
    # Viewer related files - trying to avoid, but if linker errors persist for them, might be needed for some AbstractParser paths
    # ../src/viewers/AnsiViewWindow.cpp # Example, if needed
    # ../src/viewers/LaunchAsyncViewer.h # This is a header, but might indicate .cpp
    # ../src/viewers/TopLevelWindows.cpp # Example, if needed
)
set(TestParser_SRCS testparser.cpp)
add_executable(TestParser ${TestParser_SRCS} ${parser_EXTRA_SRCS})
add_dependencies(TestParser mm_global mm_map)
target_link_libraries(TestParser
        mm_map    # For MapData, CommandId etc.
        mm_global # For global utils, some configuration aspects
        Qt5::Gui
        Qt5::Network
        Qt5::Test
        Qt5::Widgets
        coverage_config)
set_target_properties(
  TestParser PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestParser COMMAND TestParser)

# Proxy
set(proxy_SRCS
    ../src/proxy/GmcpMessage.cpp
    ../src/proxy/GmcpMessage.h
    ../src/proxy/GmcpModule.cpp
    ../src/proxy/GmcpModule.h
    ../src/proxy/GmcpUtils.cpp
    ../src/proxy/GmcpUtils.h
    ../src/proxy/telnetfilter.cpp
    ../src/proxy/telnetfilter.h
     )
set(TestProxy_SRCS TestProxy.cpp)
add_executable(TestProxy ${TestProxy_SRCS} ${proxy_SRCS})
add_dependencies(TestProxy mm_global)
target_link_libraries(TestProxy mm_global Qt5::Test coverage_config)
set_target_properties(
  TestProxy PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestProxy COMMAND TestProxy)

# MainWindow
set(mainwindow_SRCS
    ../src/global/Version.h
    ../src/mainwindow/UpdateDialog.cpp
    ../src/mainwindow/UpdateDialog.h
    )
set(TestMainWindow_SRCS TestMainWindow.cpp)
add_executable(TestMainWindow ${TestMainWindow_SRCS} ${mainwindow_SRCS})
add_dependencies(TestMainWindow mm_global)
target_link_libraries(TestMainWindow
        mm_global
        Qt5::Network
        Qt5::Test
        Qt5::Widgets
        coverage_config)
set_target_properties(
  TestMainWindow PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestMainWindow COMMAND TestMainWindow)

# Global
set(TestGlobal_SRCS TestGlobal.cpp)
add_executable(TestGlobal ${TestGlobal_SRCS})
add_dependencies(TestGlobal mm_global)
target_link_libraries(TestGlobal
        mm_global
        Qt5::Gui
        Qt5::Test
        Qt5::Widgets
        coverage_config)
set_target_properties(
  TestGlobal PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestGlobal COMMAND TestGlobal)

# Global
set(TestMap_SRCS
        ../src/configuration/configuration.cpp
        ../src/configuration/configuration.h
        TestMap.cpp
)
add_executable(TestMap ${TestMap_SRCS})
add_dependencies(TestMap mm_global mm_map)
target_link_libraries(TestMap
        mm_map
        mm_global
        Qt5::Gui
        Qt5::Network
        Qt5::Test
        Qt5::Widgets
        coverage_config)
set_target_properties(
        TestMap PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestMap COMMAND TestMap)

# Adventure
set(adventure_SRCS
        ../src/adventure/adventuresession.cpp
        ../src/adventure/adventuresession.h
        ../src/adventure/adventuretracker.cpp
        ../src/adventure/adventuretracker.h
        ../src/adventure/lineparsers.cpp
        ../src/adventure/lineparsers.h
        ../src/configuration/configuration.cpp
        ../src/configuration/configuration.h
        ../src/observer/gameobserver.cpp
        ../src/observer/gameobserver.h
        )
set(TestAdventure_SRCS testadventure.cpp testadventure.h)
add_executable(TestAdventure ${TestAdventure_SRCS} ${adventure_SRCS})
add_dependencies(TestAdventure mm_global)
target_link_libraries(TestAdventure mm_global Qt5::Widgets Qt5::Network Qt5::Test coverage_config)
set_target_properties(
        TestAdventure PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestAdventure COMMAND TestAdventure)

# Timer
set(timer_test
        ../src/timers/CTimers.cpp
        ../src/timers/CTimers.h
)
set(TestTimer TestCTimers.cpp)
add_executable(TestTimer ${timer_test} ${TestTimer})
add_dependencies(TestTimer mm_global)
target_link_libraries(TestTimer mm_global Qt5::Test coverage_config)
set_target_properties(
        TestTimer PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestTimer COMMAND TestTimer)

# Room panel mob
set(room_mob
        ../src/roompanel/RoomMob.h
        ../src/roompanel/RoomMob.cpp
)
set(RoomMob roompanel/TestRoomMob.cpp)
add_executable(TestRoomMob ${room_mob} ${RoomMob})
add_dependencies(TestRoomMob mm_global)
target_link_libraries(TestRoomMob mm_global Qt5::Test Qt5::Gui Qt5::Widgets coverage_config)
set_target_properties(
        TestRoomMob PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestRoomMob COMMAND TestRoomMob)


# Room panel mobs
set(room_mobs
        ../src/roompanel/RoomMobs.h
        ../src/roompanel/RoomMobs.cpp
        ../src/roompanel/RoomMob.h
        ../src/roompanel/RoomMob.cpp
)
set(RoomMobs roompanel/TestRoomMobs.cpp)
add_executable(TestRoomMobs ${room_mobs} ${RoomMobs})
add_dependencies(TestRoomMobs mm_global)
target_link_libraries(TestRoomMobs mm_global Qt5::Test Qt5::Gui Qt5::Widgets coverage_config)
set_target_properties(
        TestRoomMobs PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestRoomMobs COMMAND TestRoomMobs)

# Room panel manager
set(room_manager
        ../src/roompanel/RoomMobs.h
        ../src/roompanel/RoomMobs.cpp
        ../src/roompanel/RoomMob.h
        ../src/roompanel/RoomMob.cpp
        ../src/roompanel/RoomManager.cpp
        ../src/roompanel/RoomManager.h
        ../src/proxy/GmcpMessage.cpp
)
set(RoomManager roompanel/TestRoomManager.cpp)
add_executable(TestRoomManager ${room_manager} ${RoomManager})
add_dependencies(TestRoomManager mm_global)
target_link_libraries(TestRoomManager mm_global Qt5::Test Qt5::Gui Qt5::Widgets coverage_config)
set_target_properties(
        TestRoomManager PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestRoomManager COMMAND TestRoomManager)

# Utility function tests (minimal dependencies)
set(parserutils_SRCS
    testparserutils.cpp
    ../src/map/mmapper2room.cpp
    ../src/map/ExitDirection.cpp
    ../src/map/coordinate.cpp
    ../src/global/TextUtils.cpp
    ../src/global/logging.cpp
    ../src/map/sanitizer.cpp         # Added
    ../src/global/Charset.cpp       # Added
    ../src/global/Charset-Utf8.cpp  # Added
    ../src/global/Charset-Utf16.cpp
    ../src/global/NullPointerException.cpp 
    ../src/global/utils.cpp         
    ../src/global/tests.cpp         
    ../src/global/AnsiTextUtils.cpp 
    ../src/global/Color.cpp           # Added
    ../src/global/NamedColors.cpp
    ../src/global/PrintUtils.cpp
    ../src/global/AnsiOstream.cpp
    ../src/global/CharUtils.cpp    # Added dependency for AnsiOstream
    ../src/global/LineUtils.cpp   # Added dependency for AnsiOstream
    ../src/global/parserutils.cpp # Added for ParserUtils::removeAnsiMarksInPlace
    # Removed duplicate parserutils.cpp

    # Dependencies for createParseEventTest
    ../src/map/parseevent.cpp
    ../src/map/RawRoom.cpp
    ../src/map/RawExit.cpp         # Added
    ../src/map/PromptFlags.cpp
    ../src/map/CommandId.cpp      # Added
    ../src/global/TaggedString.cpp # Added
    ../src/map/ExitFieldVariant.cpp # Added for tags::DoorNameTag::isValid
    # ExitsFlags.h and ConnectedRoomFlags.h are header-only via parseevent.h
)
add_executable(TestParserUtils ${parserutils_SRCS})
target_link_libraries(TestParserUtils
    Qt5::Test
    Qt5::Core
    Qt5::Gui # Added for QColor dependency
    coverage_config
)
set_target_properties(TestParserUtils PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestParserUtils COMMAND TestParserUtils)
