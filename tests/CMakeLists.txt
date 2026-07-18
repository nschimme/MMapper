add_compile_definitions(MM_TESTING)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

include_directories(SYSTEM ${GLM_INCLUDE_DIR})
include_directories(SYSTEM ${IMMER_INCLUDE_DIR})

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

# Common test library
add_library(mm_test STATIC
    ../src/configuration/configuration.cpp
    ../src/configuration/configuration.h
    ../src/configuration/GroupConfig.cpp
    ../src/configuration/GroupConfig.h
    ../src/observer/gameobserver.cpp
    ../src/observer/gameobserver.h
    ../src/proxy/GmcpMessage.cpp
    ../src/proxy/GmcpMessage.h
)
set_target_properties(mm_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    COMPILE_FLAGS "${WARNING_FLAGS}"
)
target_link_libraries(mm_test
    mm_global
    Qt6::Core
    Qt6::Network
    coverage_config
)

# Clock
set(clock_SRCS
    ../src/clock/ClockStrings.cpp
    ../src/clock/ClockStrings.h
    ../src/clock/mumeclock.cpp
    ../src/clock/mumeclock.h
    ../src/clock/mumemoment.cpp
    ../src/clock/mumemoment.h
    )
set(TestClock_SRCS testclock.cpp)
add_executable(TestClock ${TestClock_SRCS} ${clock_SRCS})
add_dependencies(TestClock mm_test mm_global)
target_link_libraries(TestClock
        mm_test
        mm_global
        Qt6::Network
        Qt6::Gui
        Qt6::Test
        Qt6::Widgets
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
    ../src/parser/Abbrev.cpp
    )
set(TestExpandoraCommon_SRCS testexpandoracommon.cpp)
add_executable(TestExpandoraCommon ${TestExpandoraCommon_SRCS} ${expandoracommon_SRCS})
add_dependencies(TestExpandoraCommon mm_test mm_global mm_map)
target_link_libraries(TestExpandoraCommon
        mm_map
        mm_test
        mm_global
        Qt6::Gui
        Qt6::Network
        Qt6::Test
        Qt6::Widgets
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
set(TestParser_SRCS testparser.cpp)
add_executable(TestParser ${TestParser_SRCS})
add_dependencies(TestParser mm_test mm_global mm_map)
target_link_libraries(TestParser
        mm_map
        mm_test
        mm_global
        Qt6::Gui
        Qt6::Network
        Qt6::Test
        Qt6::Widgets
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
    ../src/proxy/GmcpModule.cpp
    ../src/proxy/GmcpModule.h
    ../src/proxy/GmcpUtils.cpp
    ../src/proxy/GmcpUtils.h
    ../src/proxy/telnetfilter.cpp
    ../src/proxy/telnetfilter.h
     )
set(TestProxy_SRCS TestProxy.cpp)
add_executable(TestProxy ${TestProxy_SRCS} ${proxy_SRCS})
add_dependencies(TestProxy mm_test mm_global)
target_link_libraries(TestProxy mm_test mm_global Qt6::Test coverage_config)
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
    ../src/mainwindow/AudioVolumeSlider.cpp
    ../src/mainwindow/AudioVolumeSlider.h
    ../src/mainwindow/UpdateChecker.cpp
    ../src/mainwindow/UpdateChecker.h
    )
# PreferencesController + the five SIMPLE-page adapters (see
# ../src/preferences/PreferencesController.h); these are plain QObjects with
# no Qt Quick dependency, so they are exercised here rather than in TestQml,
# keeping their round-trip coverage available in both the WITH_QML and
# non-QML build variants.
set(preferences_adapter_SRCS
    ../src/preferences/AdvancedGraphicsModel.cpp
    ../src/preferences/AdvancedGraphicsModel.h
    ../src/preferences/AnsiColorDialog.cpp
    ../src/preferences/AnsiColorDialog.h
    ../src/preferences/AnsiColorDialog.ui
    ../src/preferences/AnsiColorTables.cpp
    ../src/preferences/AnsiColorTables.h
    ../src/preferences/ansicombo.cpp
    ../src/preferences/ansicombo.h
    ../src/preferences/AudioPageAdapter.cpp
    ../src/preferences/AudioPageAdapter.h
    ../src/preferences/AutoLogPageAdapter.cpp
    ../src/preferences/AutoLogPageAdapter.h
    ../src/preferences/ClientPageAdapter.cpp
    ../src/preferences/ClientPageAdapter.h
    ../src/preferences/GeneralPageAdapter.cpp
    ../src/preferences/GeneralPageAdapter.h
    ../src/preferences/GraphicsPageAdapter.cpp
    ../src/preferences/GraphicsPageAdapter.h
    ../src/preferences/GroupPageAdapter.cpp
    ../src/preferences/GroupPageAdapter.h
    ../src/preferences/ManagePasswordDialog.cpp
    ../src/preferences/ManagePasswordDialog.h
    ../src/preferences/ManagePasswordDialog.ui
    ../src/preferences/MumeProtocolPageAdapter.cpp
    ../src/preferences/MumeProtocolPageAdapter.h
    ../src/preferences/ParserPageAdapter.cpp
    ../src/preferences/ParserPageAdapter.h
    ../src/preferences/PasswordDialogController.cpp
    ../src/preferences/PasswordDialogController.h
    ../src/preferences/PathMachinePageAdapter.cpp
    ../src/preferences/PathMachinePageAdapter.h
    ../src/preferences/PreferencesController.cpp
    ../src/preferences/PreferencesController.h
    ../src/configuration/PasswordConfig.cpp
    ../src/configuration/PasswordConfig.h
    ../src/opengl/OpenGLConfig.cpp
    ../src/opengl/OpenGLConfig.h
    )
set(TestMainWindow_SRCS TestMainWindow.cpp)
add_executable(TestMainWindow ${TestMainWindow_SRCS} ${mainwindow_SRCS} ${preferences_adapter_SRCS})
add_dependencies(TestMainWindow mm_test mm_global)
target_link_libraries(TestMainWindow
        mm_test
        mm_global
        Qt6::Network
        Qt6::Test
        Qt6::Widgets
        coverage_config)
if(WITH_AUDIO)
    # AudioPageAdapter.cpp uses QAudioDevice/QMediaDevices unless
    # MMAPPER_NO_AUDIO is defined (see ../src/preferences/AudioPageAdapter.cpp),
    # same as TestQml's equivalent guard above.
    target_link_libraries(TestMainWindow Qt6::Multimedia)
endif()
if(WITH_QTKEYCHAIN)
    # ../src/configuration/PasswordConfig.h includes <keychain.h> unless
    # MMAPPER_NO_QTKEYCHAIN is defined; mirror src/CMakeLists.txt's mmapper
    # target so PasswordConfig.cpp (pulled in via preferences_adapter_SRCS
    # for GeneralPageAdapter's account/password round-trip) can find it here.
    target_include_directories(TestMainWindow SYSTEM PRIVATE ${QTKEYCHAIN_INCLUDE_DIR})
    target_link_libraries(TestMainWindow ${QTKEYCHAIN_LIBRARY})
    if(NOT QTKEYCHAIN_FOUND)
        add_dependencies(TestMainWindow qtkeychain)
    endif()
endif()
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
        Qt6::Gui
        Qt6::Test
        Qt6::Widgets
        coverage_config)
set_target_properties(
  TestGlobal PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestGlobal COMMAND TestGlobal)

# Group
set(group_SRCS
        ../src/group/CGroupChar.cpp
        ../src/group/CGroupChar.h
        ../src/group/GroupModel.cpp
        ../src/group/GroupModel.h
        ../src/group/enums.cpp
        ../src/group/enums.h
        ../src/group/mmapper2character.cpp
        ../src/group/mmapper2character.h
)
set(TestGroup_SRCS ${group_SRCS} TestGroup.cpp)
add_executable(TestGroup ${TestGroup_SRCS})
add_dependencies(TestGroup mm_test mm_global mm_map)
target_link_libraries(TestGroup
        mm_map
        mm_test
        mm_global
        Qt6::Gui
        Qt6::Network
        Qt6::Test
        Qt6::Widgets
        coverage_config)
set_target_properties(
        TestGroup PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestGroup COMMAND TestGroup)

# Map
set(TestMap_SRCS
        TestMap.cpp
)
add_executable(TestMap ${TestMap_SRCS})
add_dependencies(TestMap mm_test mm_global mm_map)
target_link_libraries(TestMap
        mm_map
        mm_test
        mm_global
        Qt6::Gui
        Qt6::Network
        Qt6::Test
        Qt6::Widgets
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
        ../src/adventure/AdventureLogModel.cpp
        ../src/adventure/AdventureLogModel.h
        ../src/adventure/adventuresession.cpp
        ../src/adventure/adventuresession.h
        ../src/adventure/adventuretracker.cpp
        ../src/adventure/adventuretracker.h
        ../src/adventure/lineparsers.cpp
        ../src/adventure/lineparsers.h
        )
set(TestAdventure_SRCS testadventure.cpp testadventure.h)
add_executable(TestAdventure ${TestAdventure_SRCS} ${adventure_SRCS})
add_dependencies(TestAdventure mm_test mm_global)
target_link_libraries(TestAdventure mm_test mm_global Qt6::Widgets Qt6::Network Qt6::Test coverage_config)
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
        ../src/timers/TimerController.cpp
        ../src/timers/TimerController.h
        ../src/timers/TimerModel.cpp
        ../src/timers/TimerModel.h
)
set(TestTimer TestCTimers.cpp)
add_executable(TestTimer ${timer_test} ${TestTimer})
add_dependencies(TestTimer mm_global)
target_link_libraries(TestTimer mm_global Qt6::Test coverage_config)
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
target_link_libraries(TestRoomMob mm_global Qt6::Test Qt6::Gui Qt6::Widgets coverage_config)
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
target_link_libraries(TestRoomMobs mm_global Qt6::Test Qt6::Gui Qt6::Widgets coverage_config)
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
        ../src/roompanel/RoomModel.cpp
        ../src/roompanel/RoomModel.h
)
set(RoomManager roompanel/TestRoomManager.cpp)
add_executable(TestRoomManager ${room_manager} ${RoomManager})
add_dependencies(TestRoomManager mm_test mm_global)
target_link_libraries(TestRoomManager mm_test mm_global Qt6::Test Qt6::Gui Qt6::Widgets coverage_config)
set_target_properties(
        TestRoomManager PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestRoomManager COMMAND TestRoomManager)

# Client (ClientLineModel + InputHistory/TabHistory + ClientController +
# HotkeyManager + shared ANSI decode helpers)
set(client_line_model_SRCS
        ../src/client/ClientController.cpp
        ../src/client/ClientController.h
        ../src/client/ClientLineModel.cpp
        ../src/client/ClientLineModel.h
        ../src/client/Hotkey.cpp
        ../src/client/Hotkey.h
        ../src/client/HotkeyManager.cpp
        ../src/client/HotkeyManager.h
        ../src/client/InputHistory.cpp
        ../src/client/InputHistory.h
)
# RemoteEditDocumentOps is the widget-free (QtGui-only) core of the MPI remote
# editor's text operations; RemoteEditHighlighter is not linked here since it's
# QSyntaxHighlighter-only and has no free functions worth unit testing directly.
set(remote_edit_document_ops_SRCS
        ../src/mpi/RemoteEditDocumentOps.cpp
        ../src/mpi/RemoteEditDocumentOps.h
)
set(TestClient_SRCS TestClient.cpp TestClient.h FakeClientBackend.cpp FakeClientBackend.h)
add_executable(TestClient ${TestClient_SRCS} ${client_line_model_SRCS} ${remote_edit_document_ops_SRCS})
add_dependencies(TestClient mm_test mm_global mm_map)
target_link_libraries(TestClient mm_map mm_test mm_global Qt6::Gui Qt6::Test Qt6::Network coverage_config)
set_target_properties(
        TestClient PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestClient COMMAND TestClient)

# HotkeyManager
set(hotkey_manager_SRCS
        ../src/client/Hotkey.cpp
        ../src/client/Hotkey.h
        ../src/client/HotkeyManager.cpp
        ../src/client/HotkeyManager.h
)
set(TestHotkeyManager_SRCS TestHotkeyManager.cpp TestHotkeyManager.h)
add_executable(TestHotkeyManager ${TestHotkeyManager_SRCS} ${hotkey_manager_SRCS})
add_dependencies(TestHotkeyManager mm_test mm_global mm_map)
target_link_libraries(TestHotkeyManager mm_map mm_test mm_global Qt6::Test Qt6::Network coverage_config)
set_target_properties(
        TestHotkeyManager PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestHotkeyManager COMMAND TestHotkeyManager)

# Media
set(media_SRCS
        ../src/media/StackBlur.cpp
        ../src/media/StackBlur.h
)
set(TestMedia_SRCS TestMedia.cpp TestMedia.h)
add_executable(TestMedia ${TestMedia_SRCS} ${media_SRCS})
add_dependencies(TestMedia mm_test mm_global)
target_link_libraries(TestMedia mm_test mm_global Qt6::Gui Qt6::Test coverage_config)
set_target_properties(
        TestMedia PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        COMPILE_FLAGS "${WARNING_FLAGS}"
)
add_test(NAME TestMedia COMMAND TestMedia)

# QML
if(WITH_QML)
    set(TestQml_SRCS TestQml.cpp TestQml.h FakeClientBackend.cpp FakeClientBackend.h)
    # MapViewModel is a plain, rendering-independent QObject (see
    # ../src/display/MapViewModel.h's file comment: deliberately has no
    # dependency on MapCanvasCore/OpenGL), so it's linked directly. The real
    # MapCanvasQuickItem class (mm_qml's "MapCanvasItem" QML type) is NOT
    # linked here: its Q_PROPERTY(MapCanvasCore *core ...) drags in
    # MapCanvasCore's vtable/RTTI (Qt's automatic QMetaType<MapCanvasCore*>
    # registration needs it even though this binary would never construct
    # one), which cascades into MapCanvasCore's OpenGL/rendering member
    # types (FrameManager, GLWeather, OpenGL, GLFont, MapCanvasViewport,
    # MapCanvasInputState, ...) -- exactly the mapfrontend/parser/mapstorage-
    # style dependency graph every other stub in TestQml.cpp (see
    # GroupControllerStub, InfomarkEditControllerStub) deliberately avoids.
    # loadMapView() below instead registers a lightweight QML-only stub
    # under the same "MapCanvasItem" type name, exercising MapView.qml's
    # chrome/layout without the real C++ item.
    set(display_map_view_SRCS
            ../src/display/MapViewModel.cpp
            ../src/display/MapViewModel.h
    )
    set(log_model_SRCS
            ../src/mainwindow/LogModel.cpp
            ../src/mainwindow/LogModel.h
    )
    set(tasks_model_SRCS
            ../src/mainwindow/TasksModel.cpp
            ../src/mainwindow/TasksModel.h
    )
    # Sources for loadDescriptionPanel(). Deliberately excludes
    # display/Filenames.cpp (whose getParserCommandName() use would drag the
    # parser/mapdata graph into this small binary); TestQml.cpp provides a
    # stub getAssetsPath() instead. ansicombo.cpp/.h moved to
    # preferences_adapter_SRCS (ParserPageAdapter now also needs it); not
    # duplicated here to avoid double-linking it into TestQml.
    set(description_panel_SRCS
            ../src/media/DescriptionAdapter.cpp
            ../src/media/DescriptionAdapter.h
            ../src/media/DescriptionImageStore.h
            ../src/media/MediaLibrary.cpp
            ../src/media/MediaLibrary.h
            ../src/media/StackBlur.cpp
            ../src/media/StackBlur.h
    )
    # Sources for loadClockStrip(): mumeclock/mumemoment (also linked into
    # TestClock via clock_SRCS above) plus ClockAdapter and the shared
    # ClockStrings helpers it (and MumeClockWidget, on the non-QML path) use.
    set(clock_adapter_SRCS
            ${clock_SRCS}
            ../src/clock/ClockAdapter.cpp
            ../src/clock/ClockAdapter.h
    )
    # Sources for loadXpStatusItem().
    set(xp_status_adapter_SRCS
            ../src/adventure/XpStatusAdapter.cpp
            ../src/adventure/XpStatusAdapter.h
    )
    # Sources for loadAboutDialog().
    set(about_info_SRCS
            ../src/mainwindow/AboutInfo.cpp
            ../src/mainwindow/AboutInfo.h
    )
    # Sources for loadAnsiViewDialog().
    set(ansi_view_content_SRCS
            ../src/viewers/AnsiViewContent.cpp
            ../src/viewers/AnsiViewContent.h
    )
    # Sources for loadAnsiColorPickerDialog(). AnsiColorTables.cpp/.h is
    # already part of preferences_adapter_SRCS below (ParserPageAdapter now
    # also needs it); not duplicated here.
    set(ansi_color_picker_controller_SRCS
            ../src/mainwindow/AnsiColorPickerController.cpp
            ../src/mainwindow/AnsiColorPickerController.h
    )
    # Sources for loadUpdateDialog(). TestQml.cpp stubs the generated
    # Version.cpp symbols UpdateChecker uses (same approach as TestMainWindow).
    set(update_checker_SRCS
            ../src/mainwindow/UpdateChecker.cpp
            ../src/mainwindow/UpdateChecker.h
    )
    # Sources for findRoomsModelBasics()/loadFindRoomsDialog(). Only the model
    # is linked here; FindRoomsController needs a live MapData (see
    # GroupControllerStub's comment in TestQml.cpp for why that whole
    # mapfrontend/parser/mapstorage graph isn't linked into this test binary).
    set(find_rooms_model_SRCS
            ../src/mainwindow/FindRoomsModel.cpp
            ../src/mainwindow/FindRoomsModel.h
    )
    # Sources for checkableFlagModelBasics(). mmqt::ansiToHtml() (used by
    # ansiToHtmlBasics()) needs no extra entry here: global/AnsiHtml.cpp is
    # already part of mm_global, which TestQml links below.
    set(checkable_flag_model_SRCS
            ../src/mainwindow/CheckableFlagModel.cpp
            ../src/mainwindow/CheckableFlagModel.h
    )
    # Sources for remoteEditControllerOps()/remoteEditDialogCloseGuard()/
    # loadRemoteEditDialog(). RemoteEditController.cpp/.h and
    # RemoteEditDialogHost.cpp/.h are NOT listed here: they're already
    # compiled into mm_qml (see src/CMakeLists.txt's qt_add_qml_module
    # SOURCES), which TestQml links below, so #include-ing their headers is
    # enough. RemoteEditHighlighter.cpp/.h is, though -- it's part of
    # mmapper_SRCS (the "mmapper" executable's own sources), not any static
    # lib TestQml links, so it needs the same direct treatment as
    # remote_edit_document_ops_SRCS above (used by RemoteEditController.cpp).
    set(remote_edit_highlighter_SRCS
            ../src/mpi/RemoteEditHighlighter.cpp
            ../src/mpi/RemoteEditHighlighter.h
    )
    # Sources for remoteEditSessionQmlPath(): the RemoteEdit/RemoteEditSession
    # manager layer that constructs RemoteEditController+RemoteEditDialogHost
    # under MMAPPER_WITH_QML (see mpi/remoteeditsession.cpp). Pulls in
    # remoteeditprocess.cpp too since remoteeditsession.cpp unconditionally
    # includes it outside Q_OS_WASM; neither drags in any proxy .cpp beyond
    # the header-only TaggedBytes.h already used elsewhere in this binary.
    set(remote_edit_session_SRCS
            ../src/mpi/remoteedit.cpp
            ../src/mpi/remoteedit.h
            ../src/mpi/remoteeditsession.cpp
            ../src/mpi/remoteeditsession.h
    )
    if(NOT EMSCRIPTEN)
        list(APPEND remote_edit_session_SRCS
                ../src/mpi/remoteeditprocess.cpp
                ../src/mpi/remoteeditprocess.h
        )
    endif()
    add_executable(TestQml ${TestQml_SRCS} ${timer_test} ${adventure_SRCS} ${room_manager}
            ${log_model_SRCS} ${tasks_model_SRCS} ${group_SRCS} ${description_panel_SRCS}
            ${clock_adapter_SRCS} ${xp_status_adapter_SRCS} ${client_line_model_SRCS}
            ${about_info_SRCS} ${update_checker_SRCS} ${find_rooms_model_SRCS}
            ${checkable_flag_model_SRCS} ${preferences_adapter_SRCS}
            ${ansi_view_content_SRCS} ${ansi_color_picker_controller_SRCS}
            ${remote_edit_document_ops_SRCS} ${remote_edit_highlighter_SRCS}
            ${remote_edit_session_SRCS} ${display_map_view_SRCS})
    add_dependencies(TestQml mm_qml mm_qmlplugin mm_test mm_global mm_map)
    target_link_libraries(TestQml
            mm_qml
            mm_qmlplugin
            mm_map
            mm_test
            mm_global
            Qt6::Quick
            Qt6::QuickWidgets
            Qt6::Widgets
            Qt6::Qml
            Qt6::Network
            Qt6::Test
            coverage_config)
    if(WITH_AUDIO)
        # MediaLibrary.cpp uses QMediaFormat unless MMAPPER_NO_AUDIO is
        # defined (which the top-level CMakeLists only does when audio is off).
        target_link_libraries(TestQml Qt6::Multimedia)
    endif()
    if(WITH_QTKEYCHAIN)
        # preferences_adapter_SRCS pulls in GeneralPageAdapter.cpp/.h and
        # PasswordConfig.cpp/.h; ../src/configuration/PasswordConfig.h
        # includes <keychain.h> unless MMAPPER_NO_QTKEYCHAIN is defined.
        # Mirror src/CMakeLists.txt's mmapper target (and TestMainWindow's
        # equivalent guard above) so it can find it here.
        target_include_directories(TestQml SYSTEM PRIVATE ${QTKEYCHAIN_INCLUDE_DIR})
        target_link_libraries(TestQml ${QTKEYCHAIN_LIBRARY})
        if(NOT QTKEYCHAIN_FOUND)
            add_dependencies(TestQml qtkeychain)
        endif()
    endif()
    set_target_properties(
            TestQml PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
            COMPILE_FLAGS "${WARNING_FLAGS}"
    )
    add_test(NAME TestQml COMMAND TestQml)
endif()
