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
  CXX_STANDARD 20
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
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestExpandoraCommon COMMAND TestExpandoraCommon)

# Parser
set(parser_SRCS
    )
set(TestParser_SRCS testparser.cpp)
add_executable(TestParser ${TestParser_SRCS} ${parser_SRCS})
add_dependencies(TestParser mm_global mm_map)
target_link_libraries(TestParser
        mm_map
        mm_global
        Qt5::Gui
        Qt5::Test
        Qt5::Widgets
        coverage_config)
set_target_properties(
  TestParser PROPERTIES
  CXX_STANDARD 20
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
  CXX_STANDARD 20
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
  CXX_STANDARD 20
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
  CXX_STANDARD 20
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
        CXX_STANDARD 20
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
        CXX_STANDARD 20
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
        CXX_STANDARD 20
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
        CXX_STANDARD 20
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
        CXX_STANDARD 20
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
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestRoomManager COMMAND TestRoomManager)

# Config Dialog Test
add_executable(testconfigdialog TestConfigDialog.cpp)
qt_standard_project_setup(TARGET testconfigdialog) # Handles Qt version specifics and sets up include directories etc.
# mmapper_lib should be the target name for the library containing ConfigDialog, DeveloperPage, Configuration etc.
# In src/CMakeLists.txt, the main executable is 'mmapper'. If there isn't a specific library target
# that these tests should link against, we might need to link against object files or create one.
# However, other tests link against mm_global and mm_map.
# ConfigDialog and DeveloperPage are part of the main mmapper sources, not specifically mm_global or mm_map.
# Let's assume for now that necessary symbols are in mm_global or transitively available.
# If linking fails, we'll need to identify the correct library target from src/CMakeLists.txt
# that exports these classes, or adjust src/CMakeLists.txt to create such a library.
# The ConfigDialog related files are not part of mm_global or mm_map according to src/CMakeLists.txt filters.
# This will likely require linking against the main executable's object files or a new library.
# For now, let's try linking mm_global and mm_map as they are common dependencies.
add_dependencies(testconfigdialog mm_global mm_map)
target_link_libraries(testconfigdialog PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core # Core is usually pulled in by Widgets, but explicit doesn't hurt.
    mm_global # For Configuration, setConfig, getConfig
    mm_map # Potentially for other utils, though less likely for ConfigDialog
    # If ConfigDialog uses classes from the main 'mmapper' target not in mm_global/mm_map,
    # we might need to link against $<TARGET_OBJECTS:mmapper> if supported, or create an interface library.
    coverage_config
)
set_target_properties(
  testconfigdialog PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestConfigDialog COMMAND testconfigdialog)

# GroupPage Test
add_executable(testgrouppage TestGroupPage.cpp)
qt_standard_project_setup(TARGET testgrouppage)
# Link against Qt Test, Widgets, Core, and necessary application libraries
# Assuming GroupPage and Configuration are in mm_global or mm_lib, similar to TestConfigDialog
# GroupPage itself is in src/preferences, Configuration is in src/configuration
# These are part of the main mmapper_SRCS, not specifically mm_global or mm_map.
# We need to link against the library that contains these.
# For now, using mm_global and mm_map as placeholders, may need adjustment.
add_dependencies(testgrouppage mm_global mm_map) # Might not be strictly needed if linking main lib
target_link_libraries(testgrouppage PRIVATE
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core
    mm_global # For Configuration
    # mm_map # If GroupPage depends on map-related things indirectly
    # We likely need to link against the target that compiles grouppage.cpp and its dependencies.
    # If all src files are compiled into a single mmapper_lib (or ${PROJECT_NAME}_lib), that's the target.
    # The testconfigdialog linked mm_global and mm_map. This might be insufficient if grouppage.cpp
    # is not part of those filtered libraries.
    # Let's assume for now these are enough or covered by a more general app library link.
    coverage_config
)
set_target_properties(
  testgrouppage PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestGroupPage COMMAND testgrouppage)
