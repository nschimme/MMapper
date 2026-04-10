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
#include "../global/Signal2.h"
#include "ConfigValue.h"
#include "GroupConfig.h"

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

    void readFrom(QSettings &conf);
    void writeTo(QSettings &conf) const;

public:
    struct NODISCARD GeneralSettings final
    {
        NODISCARD ThemeEnum getTheme() const { return theme.get(); }
        void setTheme(const ThemeEnum value) { theme.set(value); }
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            theme.registerChangeCallback(lifetime, callback);
            firstRun.registerChangeCallback(lifetime, callback);
            windowGeometry.registerChangeCallback(lifetime, callback);
            windowState.registerChangeCallback(lifetime, callback);
            alwaysOnTop.registerChangeCallback(lifetime, callback);
            showStatusBar.registerChangeCallback(lifetime, callback);
            showScrollBars.registerChangeCallback(lifetime, callback);
            showMenuBar.registerChangeCallback(lifetime, callback);
            mapMode.registerChangeCallback(lifetime, callback);
            checkForUpdate.registerChangeCallback(lifetime, callback);
            characterEncoding.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<ThemeEnum> theme{"General/Theme", "Theme", ThemeEnum::System};
        ConfigValue<bool> firstRun{"General/Run first time", "First Run", false};
        ConfigValue<QByteArray> windowGeometry{"General/Window Geometry", "Window Geometry", {}};
        ConfigValue<QByteArray> windowState{"General/Window State", "Window State", {}};
        ConfigValue<bool> alwaysOnTop{"General/Always On Top", "Always On Top", false};
        ConfigValue<bool> showStatusBar{"General/Show Status Bar", "Show Status Bar", true};
        ConfigValue<bool> showScrollBars{"General/Show Scroll Bars", "Show Scroll Bars", true};
        ConfigValue<bool> showMenuBar{"General/Show Menu Bar", "Show Menu Bar", true};
        ConfigValue<MapModeEnum> mapMode{"General/Map Mode", "Map Mode", MapModeEnum::PLAY};
        ConfigValue<bool> checkForUpdate{"General/Check for update", "Check for update", true};
        ConfigValue<CharacterEncodingEnum> characterEncoding{"General/Character encoding",
                                                             "Character encoding",
                                                             CharacterEncodingEnum::LATIN1};

        bool operator==(const GeneralSettings &other) const = default;

    private:
        SUBGROUP();
    } general;

    struct NODISCARD ConnectionSettings final
    {
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            remoteServerName.registerChangeCallback(lifetime, callback);
            remotePort.registerChangeCallback(lifetime, callback);
            localPort.registerChangeCallback(lifetime, callback);
            tlsEncryption.registerChangeCallback(lifetime, callback);
            proxyConnectionStatus.registerChangeCallback(lifetime, callback);
            proxyListensOnAnyInterface.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<QString> remoteServerName{"Connection/Server name", "Remote server", "mume.org"};
        ConfigValue<uint16_t> remotePort{"Connection/Remote port number", "Remote port", 4242};
        ConfigValue<uint16_t> localPort{"Connection/Local port number", "Local port", 4242};
        ConfigValue<bool> tlsEncryption{"Connection/TLS encryption", "Require encryption", false};
        ConfigValue<bool> proxyConnectionStatus{"Connection/Proxy connection status",
                                                "Proxy connection status",
                                                false};
        ConfigValue<bool> proxyListensOnAnyInterface{"Connection/Proxy listens on any interface",
                                                     "Proxy listens on any interface",
                                                     false};

        bool operator==(const ConnectionSettings &other) const = default;

    private:
        SUBGROUP();
    } connection;

    struct NODISCARD ParserSettings final
    {
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            roomNameColor.registerChangeCallback(lifetime, callback);
            roomDescColor.registerChangeCallback(lifetime, callback);
            prefixChar.registerChangeCallback(lifetime, callback);
            encodeEmoji.registerChangeCallback(lifetime, callback);
            decodeEmoji.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<QString> roomNameColor{"Parser/Room name ansi color", "Room name color", "[32m"};
        ConfigValue<QString> roomDescColor{"Parser/Room desc ansi color", "Room desc color", "[0m"};
        ConfigValue<char> prefixChar{"Parser/Command prefix character",
                                     "Command prefix character",
                                     char_consts::C_UNDERSCORE};
        ConfigValue<bool> encodeEmoji{"Parser/encode emoji", "Encode emoji", true};
        ConfigValue<bool> decodeEmoji{"Parser/decode emoji", "Decode emoji", true};

        bool operator==(const ParserSettings &other) const = default;

    private:
        SUBGROUP();
    } parser;

    struct NODISCARD MumeClientProtocolSettings final
    {
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            internalRemoteEditor.registerChangeCallback(lifetime, callback);
            externalRemoteEditorCommand.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<bool> internalRemoteEditor{"Mume client protocol/Use internal editor",
                                               "Use internal editor",
                                               false};
        ConfigValue<QString> externalRemoteEditorCommand{"Mume client protocol/External editor "
                                                         "command",
                                                         "External editor command",
                                                         ""};

        bool operator==(const MumeClientProtocolSettings &other) const = default;

    private:
        SUBGROUP();
    } mumeClientProtocol;

    struct NODISCARD MumeNativeSettings final
    {
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            emulatedExits.registerChangeCallback(lifetime, callback);
            showHiddenExitFlags.registerChangeCallback(lifetime, callback);
            showNotes.registerChangeCallback(lifetime, callback);
        }

        /* serialized */
        ConfigValue<bool> emulatedExits{"Mume native/Emulated Exits", "Show emulated exits", false};
        ConfigValue<bool> showHiddenExitFlags{"Mume native/Show hidden exit flags",
                                              "Show hidden exit flags",
                                              false};
        ConfigValue<bool> showNotes{"Mume native/Show notes", "Show notes", false};

        bool operator==(const MumeNativeSettings &other) const = default;

    private:
        SUBGROUP();
    } mumeNative;

#define XFOREACH_CANVAS_NAMED_COLOR_OPTIONS(X) \
    X(backgroundColor, BACKGROUND) \
    X(connectionNormalColor, CONNECTION_NORMAL) X(roomDarkColor, ROOM_DARK) \
        X(roomDarkLitColor, ROOM_NO_SUNDEATH)

    struct CanvasNamedColorOptions;
    struct NODISCARD ResolvedCanvasNamedColorOptions final
    {
        ResolvedCanvasNamedColorOptions() = default;
#define X_DECL(_id, _name) Color _id;
        XFOREACH_CANVAS_NAMED_COLOR_OPTIONS(X_DECL)
#undef X_DECL
        void setFrom(const CanvasNamedColorOptions &);
    };

    struct NODISCARD CanvasNamedColorOptions
    {
#define X_DECL(_id, _name) ConfigValue<Color> _id;
        XFOREACH_CANVAS_NAMED_COLOR_OPTIONS(X_DECL)
#undef X_DECL

        bool operator==(const CanvasNamedColorOptions &other) const = default;

        NODISCARD std::shared_ptr<const ResolvedCanvasNamedColorOptions> clone() const
        {
            auto pResult = std::make_shared<ResolvedCanvasNamedColorOptions>();
            auto &result = deref(pResult);
            result.setFrom(*this);
            return pResult;
        }
    };

    struct NODISCARD CanvasSettings final : public CanvasNamedColorOptions
    {
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const;

        ConfigValue<int> antialiasingSamples{"Canvas/Number of anti-aliasing samples",
                                             "Anti-aliasing samples",
                                             0};
        ConfigValue<bool> trilinearFiltering{"Canvas/Use trilinear filtering",
                                             "Use trilinear filtering",
                                             true};
        ConfigValue<bool> showMissingMapId{"Canvas/Show missing map id",
                                           "Show missing map id",
                                           false};
        ConfigValue<bool> showUnsavedChanges{"Canvas/Show unsaved changes",
                                             "Show unsaved changes",
                                             false};
        ConfigValue<bool> showUnmappedExits{"Canvas/Draw not mapped exits",
                                            "Show unmapped exits",
                                            false};
        ConfigValue<bool> drawUpperLayersTextured{"Canvas/Draw upper layers textured",
                                                  "Draw upper layers textured",
                                                  false};
        ConfigValue<bool> drawDoorNames{"Canvas/Draw door names", "Draw door names", false};
        ConfigValue<bool> softwareOpenGL{"Canvas/software OpenGL", "Software OpenGL", false};
        ConfigValue<QString> resourcesDirectory{"Canvas/canvas.resourcesDir",
                                                "Resources directory",
                                                ""};

        // not saved yet:
        ConfigValue<bool> drawCharBeacons{"", "Draw character beacons", true};
        ConfigValue<float> charBeaconScaleCutoff{"", "Character beacon scale cutoff", 0.4f};
        ConfigValue<float> doorNameScaleCutoff{"", "Door name scale cutoff", 0.4f};
        ConfigValue<float> infomarkScaleCutoff{"", "Infomark scale cutoff", 0.25f};
        ConfigValue<float> extraDetailScaleCutoff{"", "Extra detail scale cutoff", 0.15f};

        ConfigValue<MMapper::Array<int, 3>> mapRadius{"",
                                                      "Map radius",
                                                      MMapper::Array<int, 3>{100, 100, 100}};

        ConfigValue<int> weatherAtmosphereIntensity{"Canvas/weather.atmosphereIntensity",
                                                    "Weather atmosphere intensity",
                                                    50};
        ConfigValue<int> weatherPrecipitationIntensity{"Canvas/weather.precipitationIntensity",
                                                       "Weather precipitation intensity",
                                                       50};
        ConfigValue<int> weatherTimeOfDayIntensity{"Canvas/weather.todIntensity",
                                                   "Weather time of day intensity",
                                                   50};

        bool operator==(const CanvasSettings &other) const = default;

        struct NODISCARD Advanced final
        {
            ConfigValue<bool> use3D{"Canvas/canvas.advanced.use3D", "Use 3D Map", true};
            ConfigValue<bool> autoTilt{"Canvas/canvas.advanced.autoTilt", "Auto Tilt", true};
            ConfigValue<bool> printPerfStats{"Canvas/canvas.advanced.printPerfStats",
                                             "Print Performance Stats",
                                             IS_DEBUG_BUILD};
            FixedPoint<0> maximumFps{"Canvas/canvas.advanced.maximumFps", "Maximum FPS", 4, 240, 60};

            // 5..90 degrees
            FixedPoint<1> fov{"Canvas/canvas.advanced.fov", "Field of View", 50, 900, 765};
            // 0..90 degrees
            FixedPoint<1> verticalAngle{"Canvas/canvas.advanced.verticalAngle",
                                        "Vertical Angle",
                                        0,
                                        900,
                                        450};
            // -180..180 degrees
            FixedPoint<1> horizontalAngle{"Canvas/canvas.advanced.horizontalAngle",
                                          "Horizontal Angle",
                                          -1800,
                                          1800,
                                          0};
            // 1..10 rooms
            FixedPoint<1> layerHeight{"Canvas/canvas.advanced.layerHeight",
                                      "Layer Height",
                                      10,
                                      100,
                                      15};

        public:
            void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                        const ChangeMonitor::Function &callback) const;

            Advanced();
            bool operator==(const Advanced &other) const = default;
        } advanced;

    private:
        SUBGROUP();
    } canvas;

    struct NODISCARD NamedColorOptions;
    struct NODISCARD ResolvedNamedColorOptions final
    {
        ResolvedNamedColorOptions() = default;
#define X_DECL(_id, _name) Color _id;
        XFOREACH_NAMED_COLOR_OPTIONS(X_DECL)
#undef X_DECL

        void setFrom(const NamedColorOptions &);
    };

    struct NODISCARD NamedColorOptions
    {
#define X_DECL(_id, _name) ConfigValue<Color> _id;
        XFOREACH_NAMED_COLOR_OPTIONS(X_DECL)
#undef X_DECL
        NamedColorOptions();
        bool operator==(const NamedColorOptions &other) const = default;
        void resetToDefaults();
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const;

        NODISCARD std::shared_ptr<const ResolvedNamedColorOptions> clone() const
        {
            auto pResult = std::make_shared<ResolvedNamedColorOptions>();
            auto &result = deref(pResult);
            result.setFrom(*this);
            return pResult;
        }
    };

    struct NODISCARD ColorSettings final : public NamedColorOptions
    {
        // TODO: save color settings
        // TODO: record which named colors require a full map update.
        bool operator==(const ColorSettings &other) const;

    private:
        SUBGROUP();
    } colorSettings;

    struct NODISCARD AccountSettings final
    {
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            accountName.registerChangeCallback(lifetime, callback);
            accountPassword.registerChangeCallback(lifetime, callback);
            rememberLogin.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<QString> accountName{"Account/account name", "Account name", ""};
        ConfigValue<bool> accountPassword{"Account/account password", "Save password", false};
        ConfigValue<bool> rememberLogin{"Account/remember login", "Remember login", false};

        bool operator==(const AccountSettings &other) const = default;

    private:
        SUBGROUP();
    } account;

    struct NODISCARD AutoLoadSettings final
    {
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            autoLoadMap.registerChangeCallback(lifetime, callback);
            fileName.registerChangeCallback(lifetime, callback);
            lastMapDirectory.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<bool> autoLoadMap{"Auto load world/Auto load", "Auto load map", false};
        ConfigValue<QString> fileName{"Auto load world/File name", "Map file name", ""};
        ConfigValue<QString> lastMapDirectory{"Auto load world/Last map load directory",
                                              "Last map directory",
                                              ""};

        bool operator==(const AutoLoadSettings &other) const = default;

    private:
        SUBGROUP();
    } autoLoad;

    struct NODISCARD AutoLogSettings final
    {
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            autoLogDirectory.registerChangeCallback(lifetime, callback);
            autoLog.registerChangeCallback(lifetime, callback);
            cleanupStrategy.registerChangeCallback(lifetime, callback);
            deleteWhenLogsReachDays.registerChangeCallback(lifetime, callback);
            deleteWhenLogsReachBytes.registerChangeCallback(lifetime, callback);
            askDelete.registerChangeCallback(lifetime, callback);
            rotateWhenLogsReachBytes.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<QString> autoLogDirectory{"Auto log/Auto log directory",
                                              "Auto log directory",
                                              ""};
        ConfigValue<bool> autoLog{"Auto log/Auto log", "Enable auto logging", false};
        ConfigValue<AutoLoggerEnum> cleanupStrategy{"Auto log/Auto log cleanup strategy",
                                                    "Cleanup strategy",
                                                    AutoLoggerEnum::DeleteDays};
        ConfigValue<int> deleteWhenLogsReachDays{"Auto log/Auto log delete after X days",
                                                 "Delete logs after days",
                                                 0};
        ConfigValue<int> deleteWhenLogsReachBytes{"Auto log/Auto log delete after X bytes",
                                                  "Delete logs after size",
                                                  0};
        ConfigValue<bool> askDelete{"Auto log/Auto log ask before deleting",
                                    "Ask before deleting",
                                    false};
        ConfigValue<int> rotateWhenLogsReachBytes{"Auto log/Auto log rotate after X bytes",
                                                  "Rotate log after size",
                                                  0};

        bool operator==(const AutoLogSettings &other) const = default;

    private:
        SUBGROUP();
    } autoLog;

    struct NODISCARD PathMachineSettings final
    {
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            acceptBestRelative.registerChangeCallback(lifetime, callback);
            acceptBestAbsolute.registerChangeCallback(lifetime, callback);
            newRoomPenalty.registerChangeCallback(lifetime, callback);
            multipleConnectionsPenalty.registerChangeCallback(lifetime, callback);
            correctPositionBonus.registerChangeCallback(lifetime, callback);
            maxPaths.registerChangeCallback(lifetime, callback);
            matchingTolerance.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<double> acceptBestRelative{"Path Machine/relative path acceptance",
                                               "Relative path acceptance",
                                               0.0};
        ConfigValue<double> acceptBestAbsolute{"Path Machine/absolute path acceptance",
                                               "Absolute path acceptance",
                                               0.0};
        ConfigValue<double> newRoomPenalty{"Path Machine/room creation penalty",
                                           "New room penalty",
                                           0.0};
        ConfigValue<double> multipleConnectionsPenalty{"Path Machine/multiple connections penalty",
                                                       "Multiple connections penalty",
                                                       0.0};
        ConfigValue<double> correctPositionBonus{"Path Machine/correct position bonus",
                                                 "Correct position bonus",
                                                 0.0};
        ConfigValue<int> maxPaths{"Path Machine/maximum number of paths", "Maximum paths", 0};
        ConfigValue<int> matchingTolerance{"Path Machine/room matching tolerance",
                                           "Matching tolerance",
                                           0};

        bool operator==(const PathMachineSettings &other) const = default;

    private:
        SUBGROUP();
    } pathMachine;

    struct NODISCARD GroupManagerSettings final
    {
        ConfigValue<QColor> color{"Group Manager/color", "My group color", QColor(Qt::yellow)};
        ConfigValue<QColor> npcColor{"Group Manager/npc color", "NPC color", QColor(Qt::lightGray)};
        ConfigValue<bool> npcColorOverride{"Group Manager/npc color override",
                                           "NPC color override",
                                           false};
        ConfigValue<bool> npcSortBottom{"Group Manager/npc sort bottom", "NPC sort bottom", false};
        ConfigValue<bool> npcHide{"Group Manager/npc hide", "NPC hide", false};

        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            color.registerChangeCallback(lifetime, callback);
            npcColor.registerChangeCallback(lifetime, callback);
            npcColorOverride.registerChangeCallback(lifetime, callback);
            npcSortBottom.registerChangeCallback(lifetime, callback);
            npcHide.registerChangeCallback(lifetime, callback);
        }

        bool operator==(const GroupManagerSettings &other) const = default;

    private:
        SUBGROUP();
    } groupManager;

    struct NODISCARD MumeClockSettings final
    {
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            startEpoch.registerChangeCallback(lifetime, callback);
            display.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<int64_t> startEpoch{"Mume Clock/Mume start epoch", "Mume start epoch", 0};
        ConfigValue<bool> display{"Mume Clock/Display clock", "Show clock", false};

        bool operator==(const MumeClockSettings &other) const = default;

    private:
        SUBGROUP();
    } mumeClock;

    struct NODISCARD AdventurePanelSettings final
    {
        NODISCARD bool getDisplayXPStatus() const { return displayXPStatus.get(); }
        void setDisplayXPStatus(const bool value) { displayXPStatus.set(value); }
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            displayXPStatus.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<bool> displayXPStatus{"Adventure Panel/Display XP status bar widget",
                                          "Show session XP",
                                          false};

        bool operator==(const AdventurePanelSettings &other) const = default;

    private:
        SUBGROUP();
    } adventurePanel;

    struct NODISCARD AudioSettings final
    {
        NODISCARD int getMusicVolume() const { return musicVolume.get(); }
        void setMusicVolume(const int value) { musicVolume.set(value); }

        NODISCARD int getSoundVolume() const { return soundVolume.get(); }
        void setSoundVolume(const int value) { soundVolume.set(value); }

        NODISCARD const QByteArray &getOutputDeviceId() const { return outputDeviceId.get(); }
        void setOutputDeviceId(const QByteArray &value) { outputDeviceId.set(value); }

        NODISCARD bool isUnlocked() const { return unlocked.get(); }
        void setUnlocked() { unlocked.set(true); }

        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            musicVolume.registerChangeCallback(lifetime, callback);
            soundVolume.registerChangeCallback(lifetime, callback);
            outputDeviceId.registerChangeCallback(lifetime, callback);
            unlocked.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<int> musicVolume{"Audio/Music volume", "Music volume", 50};
        ConfigValue<int> soundVolume{"Audio/Sound volume", "Sound volume", 50};
        ConfigValue<QByteArray> outputDeviceId{"Audio/Audio output device", "Output device", {}};
        ConfigValue<bool> unlocked{"Audio/Audio unlocked", "Audio unlocked", false};

        bool operator==(const AudioSettings &other) const = default;

    private:
        SUBGROUP();
    } audio;

    struct NODISCARD IntegratedMudClientSettings final
    {
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            font.registerChangeCallback(lifetime, callback);
            foregroundColor.registerChangeCallback(lifetime, callback);
            backgroundColor.registerChangeCallback(lifetime, callback);
            commandSeparator.registerChangeCallback(lifetime, callback);
            columns.registerChangeCallback(lifetime, callback);
            rows.registerChangeCallback(lifetime, callback);
            linesOfScrollback.registerChangeCallback(lifetime, callback);
            linesOfInputHistory.registerChangeCallback(lifetime, callback);
            tabCompletionDictionarySize.registerChangeCallback(lifetime, callback);
            clearInputOnEnter.registerChangeCallback(lifetime, callback);
            autoResizeTerminal.registerChangeCallback(lifetime, callback);
            linesOfPeekPreview.registerChangeCallback(lifetime, callback);
            audibleBell.registerChangeCallback(lifetime, callback);
            visualBell.registerChangeCallback(lifetime, callback);
            useCommandSeparator.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<QString> font{"Integrated Mud Client/Font", "Font", ""};
        ConfigValue<QColor> foregroundColor{"Integrated Mud Client/Foreground color",
                                            "Foreground color",
                                            QColor(Qt::lightGray)};
        ConfigValue<QColor> backgroundColor{"Integrated Mud Client/Background color",
                                            "Background color",
                                            QColor(Qt::black)};
        ConfigValue<QString> commandSeparator{"Integrated Mud Client/Command separator",
                                              "Command separator",
                                              ""};
        ConfigValue<int> columns{"Integrated Mud Client/Columns", "Columns", 80};
        ConfigValue<int> rows{"Integrated Mud Client/Rows", "Rows", 24};
        ConfigValue<int> linesOfScrollback{"Integrated Mud Client/Lines of scrollback",
                                           "Scrollback lines",
                                           10000};
        ConfigValue<int> linesOfInputHistory{"Integrated Mud Client/Lines of input history",
                                             "Input history lines",
                                             100};
        ConfigValue<int> tabCompletionDictionarySize{"Integrated Mud Client/Tab completion "
                                                     "dictionary size",
                                                     "Tab completion size",
                                                     100};
        ConfigValue<bool> clearInputOnEnter{"Integrated Mud Client/Clear input on enter",
                                            "Clear input on enter",
                                            false};
        ConfigValue<bool> autoResizeTerminal{"Integrated Mud Client/Auto resize terminal",
                                             "Auto resize terminal",
                                             true};
        ConfigValue<int> linesOfPeekPreview{"Integrated Mud Client/Lines of peek preview",
                                            "Peek preview lines",
                                            7};
        ConfigValue<bool> audibleBell{"Integrated Mud Client/Bell audible", "Audible bell", true};
        ConfigValue<bool> visualBell{"Integrated Mud Client/Bell visual", "Visual bell", false};
        ConfigValue<bool> useCommandSeparator{"Integrated Mud Client/Use command separator",
                                              "Use command separator",
                                              false};

        bool operator==(const IntegratedMudClientSettings &other) const = default;

    private:
        SUBGROUP();
    } integratedClient;

    struct NODISCARD RoomPanelSettings final
    {
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            geometry.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<QByteArray> geometry{"Room Panel/Window Geometry", "Window Geometry", {}};

        bool operator==(const RoomPanelSettings &other) const = default;

    private:
        SUBGROUP();
    } roomPanel;

    struct NODISCARD InfomarksDialog final
    {
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            geometry.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<QByteArray> geometry{"InfoMarks Dialog/Window Geometry", "Window Geometry", {}};

        bool operator==(const InfomarksDialog &other) const = default;

    private:
        SUBGROUP();
    } infomarksDialog;

    struct NODISCARD RoomEditDialog final
    {
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            geometry.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<QByteArray> geometry{"RoomEdit Dialog/Window Geometry", "Window Geometry", {}};

        bool operator==(const RoomEditDialog &other) const = default;

    private:
        SUBGROUP();
    } roomEditDialog;

    struct NODISCARD FindRoomsDialog final
    {
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback) const
        {
            geometry.registerChangeCallback(lifetime, callback);
        }

        ConfigValue<QByteArray> geometry{"FindRooms Dialog/Window Geometry", "Window Geometry", {}};

        bool operator==(const FindRoomsDialog &other) const = default;

    private:
        SUBGROUP();
    } findRoomsDialog;

    GroupConfig hotkeys;

public:
    DELETE_MOVE_CTOR(Configuration);
    DELETE_MOVE_ASSIGN_OP(Configuration);

private:
    Signal2Lifetime m_internalLifetime;

public:
    Configuration();
    void setupGlobalCallbacks();
    void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                const ChangeMonitor::Function &callback) const;
    Configuration(const Configuration &other);
    Configuration &operator=(const Configuration &other);
    bool operator==(const Configuration &other) const;
    bool operator!=(const Configuration &other) const { return !(*this == other); }
    friend Configuration &setConfig();
};

#undef SUBGROUP

/// Must be called before you can call setConfig() or getConfig().
/// Please don't try to cheat it. Only call this function from main().
void setEnteredMain();
/// Returns a reference to the application configuration object.
NODISCARD Configuration &setConfig();
NODISCARD const Configuration &getConfig();

using SharedCanvasNamedColorOptions
    = std::shared_ptr<const Configuration::ResolvedCanvasNamedColorOptions>;
using SharedNamedColorOptions = std::shared_ptr<const Configuration::ResolvedNamedColorOptions>;

const Configuration::ResolvedCanvasNamedColorOptions &getCanvasNamedColorOptions();
const Configuration::ResolvedNamedColorOptions &getNamedColorOptions();

struct NODISCARD ThreadLocalNamedColorRaii final
{
    explicit ThreadLocalNamedColorRaii(SharedCanvasNamedColorOptions canvasNamedColorOptions,
                                       SharedNamedColorOptions namedColorOptions);
    ~ThreadLocalNamedColorRaii();
};
