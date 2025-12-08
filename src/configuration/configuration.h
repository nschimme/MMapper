#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/Array.h"
#include "../global/ConfigConsts-Computed.h"
#include "../global/ConfigConsts.h"
#include "../global/ConfigEnums.h"
#include "../global/Consts.h"
#include "../global/FixedPoint.h"
#include "../global/NamedColors.h"
#include "../global/RuleOf5.h"
#include "NamedConfig.h"

#include <memory>
#include <string_view>

#include <QByteArray>
#include <QColor>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtCore>
#include <QtGlobal>

#undef TRANSPARENT // Bad dog, Microsoft; bad dog!!!

#define SUBGROUP() \
    friend class Configuration; \
    void read(const QSettings &conf); \
    void write(QSettings &conf) const

class NODISCARD Configuration final
{
public:
    void read();
    void write() const;
    void reset();

public:
    struct NODISCARD GeneralSettings final
    {
        bool firstRun = false;
        QByteArray windowGeometry;
        QByteArray windowState;
        bool alwaysOnTop = false;
        bool showStatusBar = true;
        bool showScrollBars = true;
        bool showMenuBar = true;
        MapModeEnum mapMode = MapModeEnum::PLAY;
        bool checkForUpdate = true;
        CharacterEncodingEnum characterEncoding = CharacterEncodingEnum::LATIN1;

    private:
        SUBGROUP();
    } general{};

    struct NODISCARD ConnectionSettings final
    {
        QString remoteServerName; /// Remote host and port settings
        uint16_t remotePort = 0u;
        uint16_t localPort = 0u; /// Port to bind to on local machine
        bool tlsEncryption = false;
        bool proxyConnectionStatus = false;
        bool proxyListensOnAnyInterface = false;

    private:
        SUBGROUP();
    } connection;

    struct NODISCARD ParserSettings final
    {
        QString roomNameColor; // ANSI room name color
        QString roomDescColor; // ANSI room descriptions color
        char prefixChar = char_consts::C_UNDERSCORE;
        bool encodeEmoji = true;
        bool decodeEmoji = true;

    private:
        SUBGROUP();
    } parser;

    struct NODISCARD MumeClientProtocolSettings final
    {
        bool internalRemoteEditor = false;
        QString externalRemoteEditorCommand;

    private:
        SUBGROUP();
    } mumeClientProtocol;

    struct NODISCARD MumeNativeSettings final
    {
        /* serialized */
        bool emulatedExits = false;
        bool showHiddenExitFlags = false;
        bool showNotes = false;

    private:
        SUBGROUP();
    } mumeNative;

    static constexpr const std::string_view BACKGROUND_NAME = "background";
    static constexpr const std::string_view CONNECTION_NORMAL_NAME = "connection-normal";
    static constexpr const std::string_view ROOM_DARK_NAME = "room-dark";
    static constexpr const std::string_view ROOM_NO_SUNDEATH_NAME = "room-no-sundeath";

#define XFOREACH_CANVAS_NAMED_COLOR_OPTIONS(X) \
    X(backgroundColor, BACKGROUND, BACKGROUND_NAME, QT_TR_NOOP("Background")) \
    X(connectionNormalColor, CONNECTION_NORMAL, CONNECTION_NORMAL_NAME, QT_TR_NOOP("Normal Connection")) \
    X(highlightNeedsServerId, HIGHLIGHT_NEEDS_SERVER_ID, "highlight-needs-server-id", QT_TR_NOOP("Highlight (Needs Server ID)")) \
    X(highlightUnsaved, HIGHLIGHT_UNSAVED, "highlight-unsaved", QT_TR_NOOP("Highlight (Unsaved)")) \
    X(highlightTemporary, HIGHLIGHT_TEMPORARY, "highlight-temporary", QT_TR_NOOP("Highlight (Temporary)")) \
    X(infomarkComment, INFOMARK_COMMENT, "infomark-comment", QT_TR_NOOP("Infomark (Comment)")) \
    X(infomarkHerb, INFOMARK_HERB, "infomark-herb", QT_TR_NOOP("Infomark (Herb)")) \
    X(infomarkMob, INFOMARK_MOB, "infomark-mob", QT_TR_NOOP("Infomark (Mob)")) \
    X(infomarkObject, INFOMARK_OBJECT, "infomark-object", QT_TR_NOOP("Infomark (Object)")) \
    X(infomarkRiver, INFOMARK_RIVER, "infomark-river", QT_TR_NOOP("Infomark (River)")) \
    X(infomarkRoad, INFOMARK_ROAD, "infomark-road", QT_TR_NOOP("Infomark (Road)")) \
    X(roomDarkColor, ROOM_DARK, ROOM_DARK_NAME, QT_TR_NOOP("Dark Room")) \
    X(roomDarkLitColor, ROOM_NO_SUNDEATH, ROOM_NO_SUNDEATH_NAME, QT_TR_NOOP("Room w/o Sundeath")) \
    X(stream, STREAM, "stream", QT_TR_NOOP("Stream")) \
    X(transparent, TRANSPARENT, ".transparent", nullptr) \
    X(verticalColorClimb, VERTICAL_COLOR_CLIMB, "vertical-climb", QT_TR_NOOP("Vertical Exit (Climb)")) \
    X(verticalColorRegular, VERTICAL_COLOR_REGULAR_EXIT, "vertical-regular", QT_TR_NOOP("Vertical Exit (Regular)")) \
    X(wallBuggedDoor, WALL_COLOR_BUG_WALL_DOOR, "wall-bug-wall-door", QT_TR_NOOP("Wall (Bugged Door)")) \
    X(wallClimb, WALL_COLOR_CLIMB, "wall-climb", QT_TR_NOOP("Wall (Climb)")) \
    X(wallFallDamage, WALL_COLOR_FALL_DAMAGE, "wall-fall-damage", QT_TR_NOOP("Wall (Fall Damage)")) \
    X(wallGuarded, WALL_COLOR_GUARDED, "wall-guarded", QT_TR_NOOP("Wall (Guarded)")) \
    X(wallNoFlee, WALL_COLOR_NO_FLEE, "wall-no-flee", QT_TR_NOOP("Wall (No Flee)")) \
    X(wallNoMatch, WALL_COLOR_NO_MATCH, "wall-no-match", QT_TR_NOOP("Wall (No Match)")) \
    X(wallNotMapped, WALL_COLOR_NOT_MAPPED, "wall-not-mapped", QT_TR_NOOP("Wall (Not Mapped)")) \
    X(wallRandom, WALL_COLOR_RANDOM, "wall-random", QT_TR_NOOP("Wall (Random)")) \
    X(wallRegular, WALL_COLOR_REGULAR_EXIT, "wall-regular-exit", QT_TR_NOOP("Wall (Regular Exit)")) \
    X(wallSpecial, WALL_COLOR_SPECIAL, "wall-special", QT_TR_NOOP("Wall (Special)"))

    struct NODISCARD CanvasNamedColorOptions
    {
#define X_DECL(_id, _capsId, _name, _uiName) XNamedColor _id{_name};
        XFOREACH_CANVAS_NAMED_COLOR_OPTIONS(X_DECL)
#undef X_DECL
        void resetToDefaults();

        NODISCARD std::shared_ptr<const CanvasNamedColorOptions> clone() const
        {
            auto pResult = std::make_shared<CanvasNamedColorOptions>();
            auto &result = deref(pResult);
#define X_CLONE(_id, _capsId, _name, _uiName) result._id = this->_id.getColor();
            XFOREACH_CANVAS_NAMED_COLOR_OPTIONS(X_CLONE)
#undef X_CLONE
            return pResult;
        }
    };

    struct NODISCARD CanvasSettings final : public CanvasNamedColorOptions
    {
        NamedConfig<bool> showMissingMapId{"SHOW_MISSING_MAPID", false};
        NamedConfig<bool> showUnsavedChanges{"SHOW_UNSAVED_CHANGES", false};
        NamedConfig<bool> showUnmappedExits{"SHOW_UNMAPPED_EXITS", false};
        bool drawUpperLayersTextured = false;
        bool drawDoorNames = false;
        int antialiasingSamples = 0;
        bool trilinearFiltering = false;
        bool softwareOpenGL = false;
        QString resourcesDirectory;

        // not saved yet:
        bool drawCharBeacons = true;
        float charBeaconScaleCutoff = 0.4f;
        float doorNameScaleCutoff = 0.4f;
        float infomarkScaleCutoff = 0.25f;
        float extraDetailScaleCutoff = 0.15f;

        MMapper::Array<int, 3> mapRadius{100, 100, 100};

        struct NODISCARD Advanced final
        {
            NamedConfig<bool> use3D{"MMAPPER_3D", true};
            NamedConfig<bool> autoTilt{"MMAPPER_AUTO_TILT", true};
            NamedConfig<bool> printPerfStats{"MMAPPER_GL_PERFSTATS", IS_DEBUG_BUILD};

            // 5..90 degrees
            FixedPoint<1> fov{50, 900, 765};
            // 0..90 degrees
            FixedPoint<1> verticalAngle{0, 900, 450};
            // -45..45 degrees
            FixedPoint<1> horizontalAngle{-450, 450, 0};
            // 1..10 rooms
            FixedPoint<1> layerHeight{10, 100, 15};

        public:
            void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                        const ChangeMonitor::Function &callback);

            Advanced();
        } advanced;

    private:
        SUBGROUP();
    } canvas;


    struct NODISCARD AccountSettings final
    {
        QString accountName;
        bool accountPassword = false;
        bool rememberLogin = false;

    private:
        SUBGROUP();
    } account;

    struct NODISCARD AutoLoadSettings final
    {
        bool autoLoadMap = false;
        QString fileName;
        QString lastMapDirectory;

    private:
        SUBGROUP();
    } autoLoad;

    struct NODISCARD AutoLogSettings final
    {
        QString autoLogDirectory;
        bool autoLog = false;
        AutoLoggerEnum cleanupStrategy = AutoLoggerEnum::DeleteDays;
        int deleteWhenLogsReachDays = 0;
        int deleteWhenLogsReachBytes = 0;
        bool askDelete = false;
        int rotateWhenLogsReachBytes = 0;

    private:
        SUBGROUP();
    } autoLog;

    struct NODISCARD PathMachineSettings final
    {
        double acceptBestRelative = 0.0;
        double acceptBestAbsolute = 0.0;
        double newRoomPenalty = 0.0;
        double multipleConnectionsPenalty = 0.0;
        double correctPositionBonus = 0.0;
        int maxPaths = 0;
        int matchingTolerance = 0;

    private:
        SUBGROUP();
    } pathMachine;

    struct NODISCARD GroupManagerSettings final
    {
        QColor color;
        QColor npcColor;
        bool npcColorOverride = false;
        bool npcSortBottom = false;
        bool npcHide = false;

    private:
        SUBGROUP();
    } groupManager;

    struct NODISCARD MumeClockSettings final
    {
        int64_t startEpoch = 0;
        bool display = false;

    private:
        SUBGROUP();
    } mumeClock;

    struct NODISCARD AdventurePanelSettings final
    {
    private:
        ChangeMonitor m_changeMonitor;
        bool m_displayXPStatus = false;

    public:
        explicit AdventurePanelSettings() = default;
        ~AdventurePanelSettings() = default;
        DELETE_CTORS_AND_ASSIGN_OPS(AdventurePanelSettings);

    public:
        NODISCARD bool getDisplayXPStatus() const { return m_displayXPStatus; }
        void setDisplayXPStatus(const bool display)
        {
            m_displayXPStatus = display;
            m_changeMonitor.notifyAll();
        }

        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback)
        {
            return m_changeMonitor.registerChangeCallback(lifetime, callback);
        }

    private:
        SUBGROUP();
    } adventurePanel;

    struct NODISCARD IntegratedMudClientSettings final
    {
        QString font;
        QColor foregroundColor;
        QColor backgroundColor;
        int columns = 0;
        int rows = 0;
        int linesOfScrollback = 0;
        int linesOfInputHistory = 0;
        int tabCompletionDictionarySize = 0;
        bool clearInputOnEnter = false;
        bool autoResizeTerminal = false;
        int linesOfPeekPreview = 0;
        bool audibleBell = false;
        bool visualBell = false;

    private:
        SUBGROUP();
    } integratedClient;

    struct NODISCARD RoomPanelSettings final
    {
        QByteArray geometry;

    private:
        SUBGROUP();
    } roomPanel;

    struct NODISCARD InfomarksDialog final
    {
        QByteArray geometry;

    private:
        SUBGROUP();
    } infomarksDialog;

    struct NODISCARD RoomEditDialog final
    {
        QByteArray geometry;

    private:
        SUBGROUP();
    } roomEditDialog;

    struct NODISCARD FindRoomsDialog final
    {
        QByteArray geometry;

    private:
        SUBGROUP();
    } findRoomsDialog;

public:
    DELETE_CTORS_AND_ASSIGN_OPS(Configuration);

private:
    Configuration();
    friend Configuration &setConfig();
};

#undef SUBGROUP

/// Must be called before you can call setConfig() or getConfig().
/// Please don't try to cheat it. Only call this function from main().
void setEnteredMain();
/// Returns a reference to the application configuration object.
NODISCARD Configuration &setConfig();
NODISCARD const Configuration &getConfig();

using SharedCanvasNamedColorOptions = std::shared_ptr<const Configuration::CanvasNamedColorOptions>;
using SharedNamedColorOptions = std::shared_ptr<const Configuration::CanvasNamedColorOptions>;

const Configuration::CanvasNamedColorOptions &getCanvasNamedColorOptions();
const Configuration::CanvasNamedColorOptions &getNamedColorOptions();

struct NODISCARD ThreadLocalNamedColorRaii final
{
    explicit ThreadLocalNamedColorRaii(SharedCanvasNamedColorOptions canvasNamedColorOptions,
                                       SharedCanvasNamedColorOptions namedColorOptions);
    ~ThreadLocalNamedColorRaii();
};
