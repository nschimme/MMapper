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

        ConfigValue<ThemeEnum> theme{ThemeEnum::System};
        ConfigValue<bool> firstRun{false};
        ConfigValue<QByteArray> windowGeometry;
        ConfigValue<QByteArray> windowState;
        ConfigValue<bool> alwaysOnTop{false};
        ConfigValue<bool> showStatusBar{true};
        ConfigValue<bool> showScrollBars{true};
        ConfigValue<bool> showMenuBar{true};
        ConfigValue<MapModeEnum> mapMode{MapModeEnum::PLAY};
        ConfigValue<bool> checkForUpdate{true};
        ConfigValue<CharacterEncodingEnum> characterEncoding{CharacterEncodingEnum::LATIN1};

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

        ConfigValue<QString> remoteServerName; /// Remote host and port settings
        ConfigValue<uint16_t> remotePort{0u};
        ConfigValue<uint16_t> localPort{0u}; /// Port to bind to on local machine
        ConfigValue<bool> tlsEncryption{false};
        ConfigValue<bool> proxyConnectionStatus{false};
        ConfigValue<bool> proxyListensOnAnyInterface{false};

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

        ConfigValue<QString> roomNameColor; // ANSI room name color
        ConfigValue<QString> roomDescColor; // ANSI room descriptions color
        ConfigValue<char> prefixChar{char_consts::C_UNDERSCORE};
        ConfigValue<bool> encodeEmoji{true};
        ConfigValue<bool> decodeEmoji{true};

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

        ConfigValue<bool> internalRemoteEditor{false};
        ConfigValue<QString> externalRemoteEditorCommand;

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
        ConfigValue<bool> emulatedExits{false};
        ConfigValue<bool> showHiddenExitFlags{false};
        ConfigValue<bool> showNotes{false};

        bool operator==(const MumeNativeSettings &other) const = default;

    private:
        SUBGROUP();
    } mumeNative;

#define XFOREACH_CANVAS_NAMED_COLOR_OPTIONS(X) \
    X(backgroundColor, BACKGROUND) \
    X(connectionNormalColor, CONNECTION_NORMAL) \
    X(roomDarkColor, ROOM_DARK) \
    X(roomDarkLitColor, ROOM_NO_SUNDEATH)

    struct CanvasNamedColorOptions;
    struct NODISCARD ResolvedCanvasNamedColorOptions final
    {
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

        NamedConfig<int> antialiasingSamples{"ANTIALIASING_SAMPLES", 0};
        NamedConfig<bool> trilinearFiltering{"TRILINEAR_FILTERING", true};
        NamedConfig<bool> showMissingMapId{"SHOW_MISSING_MAPID", false};
        NamedConfig<bool> showUnsavedChanges{"SHOW_UNSAVED_CHANGES", false};
        NamedConfig<bool> showUnmappedExits{"SHOW_UNMAPPED_EXITS", false};
        ConfigValue<bool> drawUpperLayersTextured{false};
        ConfigValue<bool> drawDoorNames{false};
        ConfigValue<bool> softwareOpenGL{false};
        ConfigValue<QString> resourcesDirectory;

        // not saved yet:
        ConfigValue<bool> drawCharBeacons{true};
        ConfigValue<float> charBeaconScaleCutoff{0.4f};
        ConfigValue<float> doorNameScaleCutoff{0.4f};
        ConfigValue<float> infomarkScaleCutoff{0.25f};
        ConfigValue<float> extraDetailScaleCutoff{0.15f};

        ConfigValue<MMapper::Array<int, 3>> mapRadius{MMapper::Array<int, 3>{100, 100, 100}};

        NamedConfig<int> weatherAtmosphereIntensity{"WEATHER_ATMOSPHERE_INTENSITY", 50};
        NamedConfig<int> weatherPrecipitationIntensity{"WEATHER_PRECIPITATION_INTENSITY", 50};
        NamedConfig<int> weatherTimeOfDayIntensity{"WEATHER_TIME_OF_DAY_INTENSITY", 50};

        bool operator==(const CanvasSettings &other) const = default;

        struct NODISCARD Advanced final
        {
            NamedConfig<bool> use3D{"MMAPPER_3D", true};
            NamedConfig<bool> autoTilt{"MMAPPER_AUTO_TILT", true};
            NamedConfig<bool> printPerfStats{"MMAPPER_GL_PERFSTATS", IS_DEBUG_BUILD};
            FixedPoint<0> maximumFps{4, 240, 60};

            // 5..90 degrees
            FixedPoint<1> fov{50, 900, 765};
            // 0..90 degrees
            FixedPoint<1> verticalAngle{0, 900, 450};
            // -180..180 degrees
            FixedPoint<1> horizontalAngle{-1800, 1800, 0};
            // 1..10 rooms
            FixedPoint<1> layerHeight{10, 100, 15};

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
        NamedColorOptions() = default;
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

        ConfigValue<QString> accountName;
        ConfigValue<bool> accountPassword{false};
        ConfigValue<bool> rememberLogin{false};

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

        ConfigValue<bool> autoLoadMap{false};
        ConfigValue<QString> fileName;
        ConfigValue<QString> lastMapDirectory;

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

        ConfigValue<QString> autoLogDirectory;
        ConfigValue<bool> autoLog{false};
        ConfigValue<AutoLoggerEnum> cleanupStrategy{AutoLoggerEnum::DeleteDays};
        ConfigValue<int> deleteWhenLogsReachDays{0};
        ConfigValue<int> deleteWhenLogsReachBytes{0};
        ConfigValue<bool> askDelete{false};
        ConfigValue<int> rotateWhenLogsReachBytes{0};

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

        ConfigValue<double> acceptBestRelative{0.0};
        ConfigValue<double> acceptBestAbsolute{0.0};
        ConfigValue<double> newRoomPenalty{0.0};
        ConfigValue<double> multipleConnectionsPenalty{0.0};
        ConfigValue<double> correctPositionBonus{0.0};
        ConfigValue<int> maxPaths{0};
        ConfigValue<int> matchingTolerance{0};

        bool operator==(const PathMachineSettings &other) const = default;

    private:
        SUBGROUP();
    } pathMachine;

    struct NODISCARD GroupManagerSettings final
    {
        ConfigValue<QColor> color;
        ConfigValue<QColor> npcColor;
        ConfigValue<bool> npcColorOverride{false};
        ConfigValue<bool> npcSortBottom{false};
        ConfigValue<bool> npcHide{false};

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

        ConfigValue<int64_t> startEpoch{0};
        ConfigValue<bool> display{false};

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

        ConfigValue<bool> displayXPStatus{false};

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

        ConfigValue<int> musicVolume{50};
        ConfigValue<int> soundVolume{50};
        ConfigValue<QByteArray> outputDeviceId;
        ConfigValue<bool> unlocked{false};

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

        ConfigValue<QString> font;
        ConfigValue<QColor> foregroundColor;
        ConfigValue<QColor> backgroundColor;
        ConfigValue<QString> commandSeparator;
        ConfigValue<int> columns{0};
        ConfigValue<int> rows{0};
        ConfigValue<int> linesOfScrollback{0};
        ConfigValue<int> linesOfInputHistory{0};
        ConfigValue<int> tabCompletionDictionarySize{0};
        ConfigValue<bool> clearInputOnEnter{false};
        ConfigValue<bool> autoResizeTerminal{false};
        ConfigValue<int> linesOfPeekPreview{0};
        ConfigValue<bool> audibleBell{false};
        ConfigValue<bool> visualBell{false};
        ConfigValue<bool> useCommandSeparator{false};

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

        ConfigValue<QByteArray> geometry;

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

        ConfigValue<QByteArray> geometry;

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

        ConfigValue<QByteArray> geometry;

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

        ConfigValue<QByteArray> geometry;

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
