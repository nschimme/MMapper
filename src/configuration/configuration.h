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
#include "GroupConfig.h"
#include "NamedConfig.h"

#include <map>
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

    NODISCARD INamedConfig *lookup(const std::string &name) const;
    NODISCARD const std::map<std::string, INamedConfig *> &getRegistry() const;

private:
    void registerConfig(INamedConfig *config);
    std::map<std::string, INamedConfig *> m_registry;

public:
    struct NODISCARD GeneralSettings final
    {
    private:
        ChangeMonitor m_changeMonitor;
        const std::shared_ptr<std::function<void()>> m_notifier = std::make_shared<std::function<void()>>([this]() { m_changeMonitor.notifyAll(); });

    public:
        explicit GeneralSettings() = default;
        ~GeneralSettings() = default;
        DELETE_CTORS_AND_ASSIGN_OPS(GeneralSettings);

    public:
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback)
        {
            return m_changeMonitor.registerChangeCallback(lifetime, callback);
        }

        void notifyChanged() { m_changeMonitor.notifyAll(); }

        NODISCARD ThemeEnum getTheme() const { return static_cast<ThemeEnum>(theme.get()); }
        void setTheme(const ThemeEnum t) { theme.set(static_cast<int>(t)); }

    public:
        NamedConfig<bool> firstRun{"GENERAL_FIRST_RUN", false, m_notifier};
        NamedConfig<QByteArray> windowGeometry{"GENERAL_WINDOW_GEOMETRY", QByteArray(), m_notifier};
        NamedConfig<QByteArray> windowState{"GENERAL_WINDOW_STATE", QByteArray(), m_notifier};
        NamedConfig<bool> alwaysOnTop{"GENERAL_ALWAYS_ON_TOP", false, m_notifier};
        NamedConfig<bool> showStatusBar{"GENERAL_SHOW_STATUS_BAR", true, m_notifier};
        NamedConfig<bool> showScrollBars{"GENERAL_SHOW_SCROLL_BARS", true, m_notifier};
        NamedConfig<bool> showMenuBar{"GENERAL_SHOW_MENU_BAR", true, m_notifier};
        NamedConfig<int> mapMode{"GENERAL_MAP_MODE", static_cast<int>(MapModeEnum::PLAY), m_notifier};
        NamedConfig<bool> checkForUpdate{"GENERAL_CHECK_FOR_UPDATE", true, m_notifier};
        NamedConfig<int> characterEncoding{"GENERAL_CHARACTER_ENCODING", static_cast<int>(CharacterEncodingEnum::LATIN1), m_notifier};
        NamedConfig<int> theme{"GENERAL_THEME", static_cast<int>(ThemeEnum::System), m_notifier};

    private:
        SUBGROUP();
    } general;

    struct NODISCARD ConnectionSettings final
    {
    private:
        ChangeMonitor m_changeMonitor;
        const std::shared_ptr<std::function<void()>> m_notifier = std::make_shared<std::function<void()>>([this]() { m_changeMonitor.notifyAll(); });

    public:
        explicit ConnectionSettings() = default;
        ~ConnectionSettings() = default;
        DELETE_CTORS_AND_ASSIGN_OPS(ConnectionSettings);

    public:
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback)
        {
            return m_changeMonitor.registerChangeCallback(lifetime, callback);
        }

    public:
        NamedConfig<QString> remoteServerName{"CONNECTION_REMOTE_SERVER_NAME", "mume.org", m_notifier};
        NamedConfig<int> remotePort{"CONNECTION_REMOTE_PORT", 4242, m_notifier};
        NamedConfig<int> localPort{"CONNECTION_LOCAL_PORT", 4242, m_notifier};
        NamedConfig<bool> tlsEncryption{"CONNECTION_TLS_ENCRYPTION", false, m_notifier};
        NamedConfig<bool> proxyConnectionStatus{"CONNECTION_PROXY_CONNECTION_STATUS", false, m_notifier};
        NamedConfig<bool> proxyListensOnAnyInterface{"CONNECTION_PROXY_LISTENS_ON_ANY_INTERFACE", false, m_notifier};

    private:
        SUBGROUP();
    } connection;

    struct NODISCARD ParserSettings final
    {
    private:
        ChangeMonitor m_changeMonitor;
        const std::shared_ptr<std::function<void()>> m_notifier = std::make_shared<std::function<void()>>([this]() { m_changeMonitor.notifyAll(); });

    public:
        explicit ParserSettings() = default;
        ~ParserSettings() = default;
        DELETE_CTORS_AND_ASSIGN_OPS(ParserSettings);

    public:
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback)
        {
            return m_changeMonitor.registerChangeCallback(lifetime, callback);
        }

    public:
        NamedConfig<QString> roomNameColor{"PARSER_ROOM_NAME_COLOR", "[32m", m_notifier};
        NamedConfig<QString> roomDescColor{"PARSER_ROOM_DESC_COLOR", "[0m", m_notifier};
        NamedConfig<int> prefixChar{"PARSER_PREFIX_CHAR", static_cast<int>(char_consts::C_UNDERSCORE), m_notifier};
        NamedConfig<bool> encodeEmoji{"PARSER_ENCODE_EMOJI", true, m_notifier};
        NamedConfig<bool> decodeEmoji{"PARSER_DECODE_EMOJI", true, m_notifier};

    private:
        SUBGROUP();
    } parser;

    struct NODISCARD MumeClientProtocolSettings final
    {
    private:
        ChangeMonitor m_changeMonitor;
        const std::shared_ptr<std::function<void()>> m_notifier = std::make_shared<std::function<void()>>([this]() { m_changeMonitor.notifyAll(); });

    public:
        explicit MumeClientProtocolSettings() = default;
        ~MumeClientProtocolSettings() = default;
        DELETE_CTORS_AND_ASSIGN_OPS(MumeClientProtocolSettings);

    public:
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback)
        {
            return m_changeMonitor.registerChangeCallback(lifetime, callback);
        }

    public:
        NamedConfig<bool> internalRemoteEditor{"MUME_PROTOCOL_INTERNAL_REMOTE_EDITOR", false, m_notifier};
        NamedConfig<QString> externalRemoteEditorCommand{"MUME_PROTOCOL_EXTERNAL_REMOTE_EDITOR_COMMAND", "", m_notifier};

    private:
        SUBGROUP();
    } mumeClientProtocol;

    struct NODISCARD MumeNativeSettings final
    {
    private:
        ChangeMonitor m_changeMonitor;
        const std::shared_ptr<std::function<void()>> m_notifier = std::make_shared<std::function<void()>>([this]() { m_changeMonitor.notifyAll(); });

    public:
        explicit MumeNativeSettings() = default;
        ~MumeNativeSettings() = default;
        DELETE_CTORS_AND_ASSIGN_OPS(MumeNativeSettings);

    public:
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback)
        {
            return m_changeMonitor.registerChangeCallback(lifetime, callback);
        }

    public:
        NamedConfig<bool> emulatedExits{"MUME_NATIVE_EMULATED_EXITS", false, m_notifier};
        NamedConfig<bool> showHiddenExitFlags{"MUME_NATIVE_SHOW_HIDDEN_EXIT_FLAGS", false, m_notifier};
        NamedConfig<bool> showNotes{"MUME_NATIVE_SHOW_NOTES", false, m_notifier};

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
#define X_DECL(_id, _name) NamedConfig<Color> _id{"CANVAS_COLOR_" #_name, Colors::white};
        XFOREACH_CANVAS_NAMED_COLOR_OPTIONS(X_DECL)
#undef X_DECL

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
        friend struct Advanced;

    private:
        ChangeMonitor m_changeMonitor;
        const std::shared_ptr<std::function<void()>> m_notifier = std::make_shared<std::function<void()>>([this]() { m_changeMonitor.notifyAll(); });

    public:
        explicit CanvasSettings();
        ~CanvasSettings() = default;
        DELETE_CTORS_AND_ASSIGN_OPS(CanvasSettings);

    public:
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback)
        {
            return m_changeMonitor.registerChangeCallback(lifetime, callback);
        }

    public:
        NamedConfig<int> antialiasingSamples{"ANTIALIASING_SAMPLES", 0, m_notifier};
        NamedConfig<bool> trilinearFiltering{"TRILINEAR_FILTERING", true, m_notifier};
        NamedConfig<bool> showMissingMapId{"SHOW_MISSING_MAPID", false, m_notifier};
        NamedConfig<bool> showUnsavedChanges{"SHOW_UNSAVED_CHANGES", false, m_notifier};
        NamedConfig<bool> showUnmappedExits{"SHOW_UNMAPPED_EXITS", false, m_notifier};
        NamedConfig<bool> drawUpperLayersTextured{"CANVAS_DRAW_UPPER_LAYERS_TEXTURED", false, m_notifier};
        NamedConfig<bool> drawDoorNames{"CANVAS_DRAW_DOOR_NAMES", false, m_notifier};
        NamedConfig<bool> softwareOpenGL{"CANVAS_SOFTWARE_OPENGL", false, m_notifier};
        NamedConfig<QString> resourcesDirectory{"CANVAS_RESOURCES_DIRECTORY", "", m_notifier};

        // not saved yet:
        NamedConfig<bool> drawCharBeacons{"CANVAS_DRAW_CHAR_BEACONS", true, m_notifier};
        NamedConfig<float> charBeaconScaleCutoff{"CANVAS_CHAR_BEACON_SCALE_CUTOFF", 0.4f, m_notifier};
        NamedConfig<float> doorNameScaleCutoff{"CANVAS_DOOR_NAME_SCALE_CUTOFF", 0.4f, m_notifier};
        NamedConfig<float> infomarkScaleCutoff{"CANVAS_INFOMARK_SCALE_CUTOFF", 0.25f, m_notifier};
        NamedConfig<float> extraDetailScaleCutoff{"CANVAS_EXTRA_DETAIL_SCALE_CUTOFF", 0.15f, m_notifier};

        MMapper::Array<int, 3> mapRadius{100, 100, 100};

        NamedConfig<int> weatherAtmosphereIntensity{"WEATHER_ATMOSPHERE_INTENSITY", 50, m_notifier};
        NamedConfig<int> weatherPrecipitationIntensity{"WEATHER_PRECIPITATION_INTENSITY", 50, m_notifier};
        NamedConfig<int> weatherTimeOfDayIntensity{"WEATHER_TIME_OF_DAY_INTENSITY", 50, m_notifier};

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
                                        const ChangeMonitor::Function &callback);

            Advanced();
        } advanced;

    private:
        SUBGROUP();
    } canvas;

    struct NODISCARD NamedColorOptions;
    struct NODISCARD ResolvedNamedColorOptions final
    {
#define X_DECL(_id, _name) XNamedColor _id{NamedColorEnum::_id};
        XFOREACH_NAMED_COLOR_OPTIONS(X_DECL)
#undef X_DECL

        void setFrom(const NamedColorOptions &);
    };

    struct NODISCARD NamedColorOptions
    {
#define X_DECL(_id, _name) XNamedColor _id{NamedColorEnum::_id};
        XFOREACH_NAMED_COLOR_OPTIONS(X_DECL)
#undef X_DECL
        NamedColorOptions() = default;
        void resetToDefaults();

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

    private:
        SUBGROUP();
    } colorSettings;

    struct NODISCARD AccountSettings final
    {
        NamedConfig<QString> accountName{"ACCOUNT_NAME", ""};
        NamedConfig<bool> accountPassword{"ACCOUNT_PASSWORD", false};
        NamedConfig<bool> rememberLogin{"ACCOUNT_REMEMBER_LOGIN", false};

    private:
        SUBGROUP();
    } account;

    struct NODISCARD AutoLoadSettings final
    {
        NamedConfig<bool> autoLoadMap{"AUTO_LOAD_MAP", false};
        NamedConfig<QString> fileName{"AUTO_LOAD_FILE_NAME", ""};
        NamedConfig<QString> lastMapDirectory{"AUTO_LOAD_LAST_MAP_DIRECTORY", ""};

    private:
        SUBGROUP();
    } autoLoad;

    struct NODISCARD AutoLogSettings final
    {
        NamedConfig<QString> autoLogDirectory{"AUTO_LOG_DIRECTORY", ""};
        NamedConfig<bool> autoLog{"AUTO_LOG_ENABLED", false};
        NamedConfig<int> cleanupStrategy{"AUTO_LOG_CLEANUP_STRATEGY", static_cast<int>(AutoLoggerEnum::DeleteDays)};
        NamedConfig<int> deleteWhenLogsReachDays{"AUTO_LOG_DELETE_WHEN_REACH_DAYS", 0};
        NamedConfig<int> deleteWhenLogsReachBytes{"AUTO_LOG_DELETE_WHEN_REACH_BYTES", 0};
        NamedConfig<bool> askDelete{"AUTO_LOG_ASK_DELETE", false};
        NamedConfig<int> rotateWhenLogsReachBytes{"AUTO_LOG_ROTATE_WHEN_REACH_BYTES", 0};

    private:
        SUBGROUP();
    } autoLog;

    struct NODISCARD PathMachineSettings final
    {
    private:
        ChangeMonitor m_changeMonitor;
        const std::shared_ptr<std::function<void()>> m_notifier = std::make_shared<std::function<void()>>([this]() { m_changeMonitor.notifyAll(); });

    public:
        explicit PathMachineSettings() = default;
        ~PathMachineSettings() = default;
        DELETE_CTORS_AND_ASSIGN_OPS(PathMachineSettings);

    public:
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback)
        {
            return m_changeMonitor.registerChangeCallback(lifetime, callback);
        }

    public:
        NamedConfig<double> acceptBestRelative{"PATH_MACHINE_ACCEPT_BEST_RELATIVE", 0.0, m_notifier};
        NamedConfig<double> acceptBestAbsolute{"PATH_MACHINE_ACCEPT_BEST_ABSOLUTE", 0.0, m_notifier};
        NamedConfig<double> newRoomPenalty{"PATH_MACHINE_NEW_ROOM_PENALTY", 0.0, m_notifier};
        NamedConfig<double> multipleConnectionsPenalty{"PATH_MACHINE_MULTIPLE_CONNECTIONS_PENALTY", 0.0, m_notifier};
        NamedConfig<double> correctPositionBonus{"PATH_MACHINE_CORRECT_POSITION_BONUS", 0.0, m_notifier};
        NamedConfig<int> maxPaths{"PATH_MACHINE_MAX_PATHS", 0, m_notifier};
        NamedConfig<int> matchingTolerance{"PATH_MACHINE_MATCHING_TOLERANCE", 0, m_notifier};

    private:
        SUBGROUP();
    } pathMachine;

    struct NODISCARD GroupManagerSettings final
    {
    private:
        ChangeMonitor m_changeMonitor;
        const std::shared_ptr<std::function<void()>> m_notifier = std::make_shared<std::function<void()>>([this]() { m_changeMonitor.notifyAll(); });

    public:
        explicit GroupManagerSettings() = default;
        ~GroupManagerSettings() = default;
        DELETE_CTORS_AND_ASSIGN_OPS(GroupManagerSettings);

    public:
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback)
        {
            return m_changeMonitor.registerChangeCallback(lifetime, callback);
        }

    public:
        NamedConfig<QColor> color{"GROUP_MANAGER_COLOR", QColor(Qt::yellow), m_notifier};
        NamedConfig<QColor> npcColor{"GROUP_MANAGER_NPC_COLOR", QColor(Qt::lightGray), m_notifier};
        NamedConfig<bool> npcColorOverride{"GROUP_MANAGER_NPC_COLOR_OVERRIDE", false, m_notifier};
        NamedConfig<bool> npcSortBottom{"GROUP_MANAGER_NPC_SORT_BOTTOM", false, m_notifier};
        NamedConfig<bool> npcHide{"GROUP_MANAGER_NPC_HIDE", false, m_notifier};

    private:
        SUBGROUP();
    } groupManager;

    struct NODISCARD MumeClockSettings final
    {
        NamedConfig<int64_t> startEpoch{"MUME_CLOCK_START_EPOCH", 0};
        NamedConfig<bool> display{"MUME_CLOCK_DISPLAY", false};

    private:
        SUBGROUP();
    } mumeClock;

    struct NODISCARD AdventurePanelSettings final
    {
    private:
        ChangeMonitor m_changeMonitor;
        const std::shared_ptr<std::function<void()>> m_notifier = std::make_shared<std::function<void()>>([this]() { m_changeMonitor.notifyAll(); });

    public:
        explicit AdventurePanelSettings() = default;
        ~AdventurePanelSettings() = default;
        DELETE_CTORS_AND_ASSIGN_OPS(AdventurePanelSettings);

    public:
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback)
        {
            return m_changeMonitor.registerChangeCallback(lifetime, callback);
        }

        void notifyChanged() { m_changeMonitor.notifyAll(); }

        NODISCARD bool getDisplayXPStatus() const { return displayXPStatus.get(); }
        void setDisplayXPStatus(const bool display) { displayXPStatus.set(display); }

    public:
        NamedConfig<bool> displayXPStatus{"ADVENTURE_PANEL_DISPLAY_XP_STATUS", false, m_notifier};

    private:
        SUBGROUP();
    } adventurePanel;

    struct NODISCARD AudioSettings final
    {
    private:
        ChangeMonitor m_changeMonitor;
        const std::shared_ptr<std::function<void()>> m_notifier = std::make_shared<std::function<void()>>([this]() { m_changeMonitor.notifyAll(); });

    public:
        explicit AudioSettings() = default;
        ~AudioSettings() = default;
        DELETE_CTORS_AND_ASSIGN_OPS(AudioSettings);

    public:
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                    const ChangeMonitor::Function &callback)
        {
            return m_changeMonitor.registerChangeCallback(lifetime, callback);
        }

        void notifyChanged() { m_changeMonitor.notifyAll(); }

        NODISCARD int getMusicVolume() const { return musicVolume.get(); }
        void setMusicVolume(const int volume) { musicVolume.set(volume); }

        NODISCARD int getSoundVolume() const { return soundVolume.get(); }
        void setSoundVolume(const int volume) { soundVolume.set(volume); }

        NODISCARD const QByteArray &getOutputDeviceId() const { return outputDeviceId.get(); }
        void setOutputDeviceId(const QByteArray &id) { outputDeviceId.set(id); }

        NODISCARD bool isUnlocked() const { return unlocked.get(); }
        void setUnlocked() { unlocked.set(true); }

    public:
        NamedConfig<int> musicVolume{"AUDIO_MUSIC_VOLUME", 50, m_notifier};
        NamedConfig<int> soundVolume{"AUDIO_SOUND_VOLUME", 50, m_notifier};
        NamedConfig<QByteArray> outputDeviceId{"AUDIO_OUTPUT_DEVICE_ID", QByteArray(), m_notifier};
        NamedConfig<bool> unlocked{"AUDIO_UNLOCKED", false, m_notifier};

    private:
        SUBGROUP();
    } audio;

    struct NODISCARD IntegratedMudClientSettings final
    {
        NamedConfig<QString> font{"INTEGRATED_CLIENT_FONT", ""};
        NamedConfig<QColor> foregroundColor{"INTEGRATED_CLIENT_FOREGROUND_COLOR", QColor(Qt::lightGray)};
        NamedConfig<QColor> backgroundColor{"INTEGRATED_CLIENT_BACKGROUND_COLOR", QColor(Qt::black)};
        NamedConfig<QString> commandSeparator{"INTEGRATED_CLIENT_COMMAND_SEPARATOR", ""};
        NamedConfig<int> columns{"INTEGRATED_CLIENT_COLUMNS", 0};
        NamedConfig<int> rows{"INTEGRATED_CLIENT_ROWS", 0};
        NamedConfig<int> linesOfScrollback{"INTEGRATED_CLIENT_LINES_OF_SCROLLBACK", 0};
        NamedConfig<int> linesOfInputHistory{"INTEGRATED_CLIENT_LINES_OF_INPUT_HISTORY", 0};
        NamedConfig<int> tabCompletionDictionarySize{"INTEGRATED_CLIENT_TAB_COMPLETION_DICTIONARY_SIZE", 0};
        NamedConfig<bool> clearInputOnEnter{"INTEGRATED_CLIENT_CLEAR_INPUT_ON_ENTER", false};
        NamedConfig<bool> autoResizeTerminal{"INTEGRATED_CLIENT_AUTO_RESIZE_TERMINAL", false};
        NamedConfig<int> linesOfPeekPreview{"INTEGRATED_CLIENT_LINES_OF_PEEK_PREVIEW", 0};
        NamedConfig<bool> audibleBell{"INTEGRATED_CLIENT_AUDIBLE_BELL", false};
        NamedConfig<bool> visualBell{"INTEGRATED_CLIENT_VISUAL_BELL", false};
        NamedConfig<bool> useCommandSeparator{"INTEGRATED_CLIENT_USE_COMMAND_SEPARATOR", false};

    private:
        SUBGROUP();
    } integratedClient;

    struct NODISCARD RoomPanelSettings final
    {
        NamedConfig<QByteArray> geometry{"ROOM_PANEL_GEOMETRY", QByteArray()};

    private:
        SUBGROUP();
    } roomPanel;

    struct NODISCARD InfomarksDialog final
    {
        NamedConfig<QByteArray> geometry{"INFOMARKS_DIALOG_GEOMETRY", QByteArray()};

    private:
        SUBGROUP();
    } infomarksDialog;

    struct NODISCARD RoomEditDialog final
    {
        NamedConfig<QByteArray> geometry{"ROOM_EDIT_DIALOG_GEOMETRY", QByteArray()};

    private:
        SUBGROUP();
    } roomEditDialog;

    struct NODISCARD FindRoomsDialog final
    {
        NamedConfig<QByteArray> geometry{"FIND_ROOMS_DIALOG_GEOMETRY", QByteArray()};

    private:
        SUBGROUP();
    } findRoomsDialog;

    GroupConfig hotkeys;

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
