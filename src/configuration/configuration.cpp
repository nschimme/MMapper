// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "configuration.h"

#include "../global/utils.h"

#include <cassert>
#include <mutex>
#include <optional>
#include <thread>

#include <QByteArray>
#include <QChar>
#include <QDir>
#include <QHostInfo>
#include <QSslSocket>
#include <QString>
#include <QStringList>

#undef TRANSPARENT // Bad dog, Microsoft; bad dog!!!

namespace { // anonymous

std::thread::id g_thread{};
std::atomic_bool g_config_enteredMain{false};

thread_local SharedCanvasNamedColorOptions tl_canvas_named_color_options;
thread_local SharedNamedColorOptions tl_named_color_options;

NODISCARD const char *getPlatformEditor()
{
    switch (CURRENT_PLATFORM) {
    case PlatformEnum::Windows:
        return "notepad";

    case PlatformEnum::Mac:
        return "open -W -n -t";

    case PlatformEnum::Linux:
        return "gedit";

    case PlatformEnum::Wasm:
    case PlatformEnum::Unknown:
    default:
        return "";
    }
}

} // namespace

#define ConstString static constexpr const char *const
ConstString SETTINGS_ORGANIZATION = "MUME";
ConstString OLD_SETTINGS_ORGANIZATION = "Caligor soft";
ConstString SETTINGS_APPLICATION = "MMapper2";
ConstString SETTINGS_FIRST_TIME_KEY = "General/Run first time";

class NODISCARD Settings final
{
private:
    static constexpr const char *const MMAPPER_PROFILE_PATH = "MMAPPER_PROFILE_PATH";

private:
    std::optional<QSettings> m_settings;

private:
    NODISCARD static bool isValid(const QFile &file)
    {
        const QFileInfo info{file};
        return !info.isDir() && info.exists() && info.isReadable() && info.isWritable();
    }

    NODISCARD static bool isValid(const QString &fileName)
    {
        const QFile file{fileName};
        return isValid(file);
    }

    static void tryCopyOldSettings();

private:
    void initSettings();

public:
    DELETE_CTORS_AND_ASSIGN_OPS(Settings);
    Settings() { initSettings(); }
    ~Settings() = default;
    explicit operator QSettings &()
    {
        if (!m_settings) {
            throw std::runtime_error("object does not exist");
        }
        return m_settings.value();
    }
};

void Settings::initSettings()
{
    if (m_settings) {
        throw std::runtime_error("object already exists");
    }

    static std::mutex g_mutex;
    std::lock_guard<std::mutex> lock{g_mutex};

    static auto g_path = qgetenv(MMAPPER_PROFILE_PATH);

    if (g_path != nullptr) {
        const QString pathString{g_path};

        static std::once_flag attempt_flag;
        std::call_once(attempt_flag, [&pathString] {
            qInfo() << "Attempting to use settings from" << pathString
                    << "(specified by environment variable" << QString{MMAPPER_PROFILE_PATH}
                    << ")...";
        });

        if (!isValid(pathString)) {
            qWarning() << "Falling back to default settings path because" << pathString
                       << "is not a writable file.";
            g_path = nullptr;
        } else {
            try {
                m_settings.emplace(pathString, QSettings::IniFormat);
            } catch (...) {
                qInfo() << "Exception loading settings for " << pathString
                        << "; falling back to default settings...";
                g_path = nullptr;
            }
        }
    }

    if (!m_settings) {
        tryCopyOldSettings();
        m_settings.emplace(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
    }

    static std::once_flag success_flag;
    std::call_once(success_flag, [this] {
        auto &&info = qInfo();
        info << "Using settings from" << QString{static_cast<QSettings &>(*this).fileName()};
        if (g_path == nullptr) {
            info << "(Hint: Environment variable" << QString{MMAPPER_PROFILE_PATH}
                 << "overrides the default).";
        } else {
            info << ".";
        }
    });
}

#define SETTINGS(conf)                                                                                 Settings settings;                                                                                 QSettings &conf = static_cast<QSettings &>(settings)

ConstString GRP_ADVENTURE_PANEL = "Adventure Panel";
ConstString GRP_AUDIO = "Audio";
ConstString GRP_ACCOUNT = "Account";
ConstString GRP_AUTO_LOAD_WORLD = "Auto load world";
ConstString GRP_AUTO_LOG = "Auto log";
ConstString GRP_CANVAS = "Canvas";
ConstString GRP_CONNECTION = "Connection";
ConstString GRP_FINDROOMS_DIALOG = "FindRooms Dialog";
ConstString GRP_GENERAL = "General";
ConstString GRP_GROUP_MANAGER = "Group Manager";
ConstString GRP_HOTKEYS = "Hotkeys";
ConstString GRP_INFOMARKS_DIALOG = "InfoMarks Dialog";
ConstString GRP_INTEGRATED_MUD_CLIENT = "Integrated Mud Client";
ConstString GRP_MUME_CLIENT_PROTOCOL = "Mume client protocol";
ConstString GRP_MUME_CLOCK = "Mume Clock";
ConstString GRP_MUME_NATIVE = "Mume native";
ConstString GRP_PARSER = "Parser";
ConstString GRP_PATH_MACHINE = "Path Machine";
ConstString GRP_ROOM_PANEL = "Room Panel";
ConstString GRP_ROOMEDIT_DIALOG = "RoomEdit Dialog";

Configuration::Configuration()
    : hotkeys(GRP_HOTKEYS)
{
    read();
    setupGlobalCallbacks();
}

bool Configuration::operator==(const Configuration &other) const
{
    return general == other.general && connection == other.connection && canvas == other.canvas
           && account == other.account && autoLoad == other.autoLoad && autoLog == other.autoLog
           && parser == other.parser && mumeClientProtocol == other.mumeClientProtocol
           && mumeNative == other.mumeNative && pathMachine == other.pathMachine
           && groupManager == other.groupManager && mumeClock == other.mumeClock
           && adventurePanel == other.adventurePanel && audio == other.audio
           && integratedClient == other.integratedClient && infomarksDialog == other.infomarksDialog
           && roomEditDialog == other.roomEditDialog && roomPanel == other.roomPanel
           && findRoomsDialog == other.findRoomsDialog && colorSettings == other.colorSettings
           && hotkeys == other.hotkeys;
}

Configuration::Configuration(const Configuration &other)
    : hotkeys(GRP_HOTKEYS)
{
    *this = other;
}

Configuration &Configuration::operator=(const Configuration &other)
{
    if (this == &other) {
        return *this;
    }

    general = other.general;
    connection = other.connection;
    canvas = other.canvas;
    account = other.account;
    autoLoad = other.autoLoad;
    autoLog = other.autoLog;
    parser = other.parser;
    mumeClientProtocol = other.mumeClientProtocol;
    mumeNative = other.mumeNative;
    pathMachine = other.pathMachine;
    groupManager = other.groupManager;
    mumeClock = other.mumeClock;
    adventurePanel = other.adventurePanel;
    audio = other.audio;
    integratedClient = other.integratedClient;
    infomarksDialog = other.infomarksDialog;
    roomEditDialog = other.roomEditDialog;
    roomPanel = other.roomPanel;
    findRoomsDialog = other.findRoomsDialog;
    colorSettings = other.colorSettings;
    hotkeys = other.hotkeys;

    return *this;
}

bool Configuration::ColorSettings::operator==(const ColorSettings &other) const
{
    return static_cast<const NamedColorOptions &>(*this)
           == static_cast<const NamedColorOptions &>(other);
}

void Settings::tryCopyOldSettings()
{
    QSettings sNew(SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
    if (!sNew.allKeys().contains(SETTINGS_FIRST_TIME_KEY)) {
        const QSettings sOld(OLD_SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
        if (!sOld.allKeys().isEmpty()) {
            qInfo() << "Copying old config" << sOld.fileName() << "to" << sNew.fileName() << "...";
            for (const QString &key : sOld.allKeys()) {
                sNew.setValue(key, sOld.value(key));
            }
        }
    }
    sNew.beginGroup(GRP_CONNECTION);
    if (sNew.value("Server name", "").toString().contains("pvv.org")) {
        sNew.setValue("Server name", "mume.org");
    }
    sNew.endGroup();
}

#define GROUP_CALLBACK(callback, name, ref)                                                            do {                                                                                                   conf.beginGroup(name);                                                                             ref.callback(conf);                                                                                conf.endGroup();                                                                               } while (false)

#define FOREACH_CONFIG_GROUP(callback)                                                                 do {                                                                                                   GROUP_CALLBACK(callback, GRP_GENERAL, general);                                                    GROUP_CALLBACK(callback, GRP_CONNECTION, connection);                                              GROUP_CALLBACK(callback, GRP_CANVAS, canvas);                                                      GROUP_CALLBACK(callback, GRP_ACCOUNT, account);                                                    GROUP_CALLBACK(callback, GRP_AUTO_LOAD_WORLD, autoLoad);                                           GROUP_CALLBACK(callback, GRP_AUTO_LOG, autoLog);                                                   GROUP_CALLBACK(callback, GRP_PARSER, parser);                                                      GROUP_CALLBACK(callback, GRP_MUME_CLIENT_PROTOCOL, mumeClientProtocol);                            GROUP_CALLBACK(callback, GRP_MUME_NATIVE, mumeNative);                                             GROUP_CALLBACK(callback, GRP_PATH_MACHINE, pathMachine);                                           GROUP_CALLBACK(callback, GRP_GROUP_MANAGER, groupManager);                                         GROUP_CALLBACK(callback, GRP_MUME_CLOCK, mumeClock);                                               GROUP_CALLBACK(callback, GRP_ADVENTURE_PANEL, adventurePanel);                                     GROUP_CALLBACK(callback, GRP_AUDIO, audio);                                                        GROUP_CALLBACK(callback, GRP_INTEGRATED_MUD_CLIENT, integratedClient);                             GROUP_CALLBACK(callback, GRP_INFOMARKS_DIALOG, infomarksDialog);                                   GROUP_CALLBACK(callback, GRP_ROOMEDIT_DIALOG, roomEditDialog);                                     GROUP_CALLBACK(callback, GRP_ROOM_PANEL, roomPanel);                                               GROUP_CALLBACK(callback, GRP_FINDROOMS_DIALOG, findRoomsDialog);                                   GROUP_CALLBACK(callback, GRP_HOTKEYS, hotkeys);                                                } while (false)

void Configuration::read()
{
    SETTINGS(conf);
    readFrom(conf);
}

void Configuration::write() const
{
    SETTINGS(conf);
    writeTo(conf);
}

void Configuration::readFrom(QSettings &conf)
{
    colorSettings.resetToDefaults();

    FOREACH_CONFIG_GROUP(read);

    if (general.firstRun) {
        canvas.advanced.use3D.set(true);
        autoLog.autoLog = (CURRENT_PLATFORM != PlatformEnum::Wasm);
        hotkeys.resetToDefault();
    }
}

void Configuration::writeTo(QSettings &conf) const
{
    FOREACH_CONFIG_GROUP(write);
}

void Configuration::reset()
{
    {
        QSettings oldConf(OLD_SETTINGS_ORGANIZATION, SETTINGS_APPLICATION);
        oldConf.clear();
    }
    {
        SETTINGS(conf);
        conf.clear();
    }
    read();
}

#undef FOREACH_CONFIG_GROUP
#undef GROUP_CALLBACK

ConstString DEFAULT_MMAPPER_SUBDIR = "/MMapper";
ConstString DEFAULT_LOGS_SUBDIR = "/Logs";
ConstString DEFAULT_RESOURCES_SUBDIR = "/Resources";

NODISCARD static QString getDefaultDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).absolutePath();
}

NODISCARD static bool isValidAnsi(const QString &input)
{
    static constexpr const auto MAX = static_cast<uint32_t>(std::numeric_limits<uint8_t>::max());

    if (!input.startsWith("[") || !input.endsWith("m")) {
        return false;
    }

    for (const auto &part : input.mid(1, input.length() - 2).split(";")) {
        for (const QChar c : part) {
            if (!c.isDigit()) {
                return false;
            }
        }
        bool ok = false;
        const auto n = part.toUInt(&ok, 10);
        if (!ok || n > MAX) {
            return false;
        }
    }

    return true;
}

NODISCARD static bool isValidMapMode(const MapModeEnum mode)
{
    switch (mode) {
    case MapModeEnum::PLAY:
    case MapModeEnum::MAP:
    case MapModeEnum::OFFLINE:
        return true;
    }
    return false;
}

NODISCARD static bool isValidTheme(const ThemeEnum theme)
{
    switch (theme) {
    case ThemeEnum::System:
    case ThemeEnum::Dark:
    case ThemeEnum::Light:
        return true;
    }
    return false;
}

NODISCARD static bool isValidCharacterEncoding(const CharacterEncodingEnum encoding)
{
    switch (encoding) {
    case CharacterEncodingEnum::ASCII:
    case CharacterEncodingEnum::LATIN1:
    case CharacterEncodingEnum::UTF8:
        return true;
    }
    return false;
}

NODISCARD static bool isValidAutoLoggerState(const AutoLoggerEnum strategy)
{
    switch (strategy) {
    case AutoLoggerEnum::DeleteDays:
    case AutoLoggerEnum::DeleteSize:
    case AutoLoggerEnum::KeepForever:
        return true;
    }
    return false;
}

void Configuration::GeneralSettings::read(const QSettings &conf)
{
    firstRun.read(conf);
    windowGeometry.read(conf);
    windowState.read(conf);
    alwaysOnTop.read(conf);
    showStatusBar.read(conf);
    showScrollBars.read(conf);
    showMenuBar.read(conf);

    mapMode.m_validator = [](MapModeEnum val) {
        return isValidMapMode(val) ? val : MapModeEnum::PLAY;
    };
    mapMode.read(conf);

    checkForUpdate.read(conf);

    characterEncoding.m_validator = [](CharacterEncodingEnum val) {
        return isValidCharacterEncoding(val) ? val : CharacterEncodingEnum::LATIN1;
    };
    characterEncoding.read(conf);

    theme.m_validator = [](ThemeEnum val) {
        return isValidTheme(val) ? val : ThemeEnum::System;
    };
    theme.read(conf);
}

void Configuration::ConnectionSettings::read(const QSettings &conf)
{
    remoteServerName.read(conf);
    remotePort.read(conf);
    localPort.read(conf);
#ifndef Q_OS_WASM
    tlsEncryption.read(conf);
    if (!QSslSocket::supportsSsl()) {
        tlsEncryption.set(false);
    }
#else
    tlsEncryption.set(true);
#endif
    proxyConnectionStatus.read(conf);
    proxyListensOnAnyInterface.read(conf);
}

static constexpr const std::string_view DEFAULT_BGCOLOR = "#2E3436";
static constexpr const std::string_view DEFAULT_DARK_COLOR = "#A19494";
static constexpr const std::string_view DEFAULT_NO_SUNDEATH_COLOR = "#D4C7C7";

void Configuration::CanvasSettings::read(const QSettings &conf)
{
    resourcesDirectory.set(conf.value("canvas.resourcesDir",
                                      getDefaultDirectory()
                                          .append(DEFAULT_MMAPPER_SUBDIR)
                                          .append(DEFAULT_RESOURCES_SUBDIR))
                               .toString());
    antialiasingSamples.read(conf);
    trilinearFiltering.read(conf);
    showMissingMapId.read(conf);
    showUnsavedChanges.read(conf);
    showUnmappedExits.read(conf);
    drawUpperLayersTextured.read(conf);
    drawDoorNames.read(conf);
    softwareOpenGL.read(conf);

    backgroundColor.read(conf);
    connectionNormalColor.read(conf);
    roomDarkColor.read(conf);
    roomDarkLitColor.read(conf);

    advanced.use3D.read(conf);
    advanced.autoTilt.read(conf);
    advanced.printPerfStats.read(conf);
    advanced.maximumFps.read(conf);
    advanced.fov.read(conf);
    advanced.verticalAngle.read(conf);
    advanced.horizontalAngle.read(conf);
    advanced.layerHeight.read(conf);

    weatherAtmosphereIntensity.read(conf);
    weatherPrecipitationIntensity.read(conf);
    weatherTimeOfDayIntensity.read(conf);
}

void Configuration::AccountSettings::read(const QSettings &conf)
{
    accountName.read(conf);
    accountPassword.read(conf);
    rememberLogin.read(conf);
    if constexpr (NO_QTKEYCHAIN) {
        rememberLogin.set(false);
    }
}

void Configuration::AutoLoadSettings::read(const QSettings &conf)
{
    autoLoadMap.read(conf);
    fileName.read(conf);
    lastMapDirectory.set(conf.value("Last map load directory",
                                    getDefaultDirectory().append(DEFAULT_MMAPPER_SUBDIR))
                             .toString());
}

void Configuration::AutoLogSettings::read(const QSettings &conf)
{
    autoLogDirectory.set(
        conf.value("Auto log directory",
                   getDefaultDirectory().append(DEFAULT_MMAPPER_SUBDIR).append(DEFAULT_LOGS_SUBDIR))
            .toString());
    autoLog.read(conf);
    rotateWhenLogsReachBytes.read(conf);
    askDelete.read(conf);

    cleanupStrategy.m_validator = [](AutoLoggerEnum val) {
        return isValidAutoLoggerState(val) ? val : AutoLoggerEnum::DeleteDays;
    };
    cleanupStrategy.read(conf);

    deleteWhenLogsReachDays.read(conf);
    deleteWhenLogsReachBytes.read(conf);
}

void Configuration::ParserSettings::read(const QSettings &conf)
{
    roomNameColor.m_validator = [](QString val) {
        return isValidAnsi(val) ? val : "[32m";
    };
    roomNameColor.read(conf);

    roomDescColor.m_validator = [](QString val) {
        return isValidAnsi(val) ? val : "[0m";
    };
    roomDescColor.read(conf);

    prefixChar.read(conf);
    encodeEmoji.read(conf);
    decodeEmoji.read(conf);
}

void Configuration::MumeClientProtocolSettings::read(const QSettings &conf)
{
    internalRemoteEditor.read(conf);
    externalRemoteEditorCommand.set(
        conf.value("External editor command", getPlatformEditor()).toString());
}

void Configuration::MumeNativeSettings::read(const QSettings &conf)
{
    emulatedExits.read(conf);
    showHiddenExitFlags.read(conf);
    showNotes.read(conf);
}

void Configuration::PathMachineSettings::read(const QSettings &conf)
{
    acceptBestRelative.read(conf);
    acceptBestAbsolute.read(conf);
    newRoomPenalty.read(conf);
    correctPositionBonus.read(conf);
    multipleConnectionsPenalty.read(conf);
    maxPaths.read(conf);
    matchingTolerance.read(conf);
}

void Configuration::GroupManagerSettings::read(const QSettings &conf)
{
    color.read(conf);
    npcColor.read(conf);
    npcColorOverride.read(conf);
    npcHide.read(conf);
    npcSortBottom.read(conf);
}

void Configuration::MumeClockSettings::read(const QSettings &conf)
{
    startEpoch.read(conf);
    display.read(conf);
}

void Configuration::AdventurePanelSettings::read(const QSettings &conf)
{
    displayXPStatus.read(conf);
}

void Configuration::AudioSettings::read(const QSettings &conf)
{
    unlocked.read(conf);
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Wasm) {
        unlocked.set(false);
    }
    musicVolume.read(conf);
    soundVolume.read(conf);
    outputDeviceId.read(conf);
}

void Configuration::IntegratedMudClientSettings::read(const QSettings &conf)
{
    font.read(conf);
    backgroundColor.read(conf);
    foregroundColor.read(conf);
    columns.read(conf);
    rows.read(conf);
    linesOfScrollback.read(conf);
    linesOfInputHistory.read(conf);
    tabCompletionDictionarySize.read(conf);
    clearInputOnEnter.read(conf);
    autoResizeTerminal.read(conf);
    linesOfPeekPreview.read(conf);
    audibleBell.read(conf);
    visualBell.read(conf);
    useCommandSeparator.read(conf);
    commandSeparator.read(conf);
}

void Configuration::RoomPanelSettings::read(const QSettings &conf)
{
    geometry.read(conf);
}

void Configuration::InfomarksDialog::read(const QSettings &conf)
{
    geometry.read(conf);
}

void Configuration::RoomEditDialog::read(const QSettings &conf)
{
    geometry.read(conf);
}

void Configuration::FindRoomsDialog::read(const QSettings &conf)
{
    geometry.read(conf);
}

void Configuration::GeneralSettings::write(QSettings &conf) const
{
    const_cast<ConfigValue<bool> &>(firstRun).set(false);
    firstRun.write(conf);
    windowGeometry.write(conf);
    windowState.write(conf);
    alwaysOnTop.write(conf);
    showStatusBar.write(conf);
    showScrollBars.write(conf);
    showMenuBar.write(conf);
    mapMode.write(conf);
    checkForUpdate.write(conf);
    characterEncoding.write(conf);
    theme.write(conf);
}

void Configuration::ConnectionSettings::write(QSettings &conf) const
{
    remoteServerName.write(conf);
    remotePort.write(conf);
    localPort.write(conf);
    tlsEncryption.write(conf);
    proxyConnectionStatus.write(conf);
    proxyListensOnAnyInterface.write(conf);
}

void Configuration::CanvasSettings::write(QSettings &conf) const
{
    resourcesDirectory.write(conf);
    antialiasingSamples.write(conf);
    trilinearFiltering.write(conf);
    showMissingMapId.write(conf);
    showUnsavedChanges.write(conf);
    showUnmappedExits.write(conf);
    drawUpperLayersTextured.write(conf);
    drawDoorNames.write(conf);
    softwareOpenGL.write(conf);

    backgroundColor.write(conf);
    connectionNormalColor.write(conf);
    roomDarkColor.write(conf);
    roomDarkLitColor.write(conf);

    advanced.use3D.write(conf);
    advanced.autoTilt.write(conf);
    advanced.printPerfStats.write(conf);
    advanced.maximumFps.write(conf);
    advanced.fov.write(conf);
    advanced.verticalAngle.write(conf);
    advanced.horizontalAngle.write(conf);
    advanced.layerHeight.write(conf);

    weatherAtmosphereIntensity.write(conf);
    weatherPrecipitationIntensity.write(conf);
    weatherTimeOfDayIntensity.write(conf);
}

void Configuration::AccountSettings::write(QSettings &conf) const
{
    accountName.write(conf);
    accountPassword.write(conf);
    rememberLogin.write(conf);
}

void Configuration::AutoLoadSettings::write(QSettings &conf) const
{
    autoLoadMap.write(conf);
    fileName.write(conf);
    lastMapDirectory.write(conf);
}

void Configuration::AutoLogSettings::write(QSettings &conf) const
{
    autoLog.write(conf);
    cleanupStrategy.write(conf);
    autoLogDirectory.write(conf);
    rotateWhenLogsReachBytes.write(conf);
    askDelete.write(conf);
    deleteWhenLogsReachDays.write(conf);
    deleteWhenLogsReachBytes.write(conf);
}

void Configuration::ParserSettings::write(QSettings &conf) const
{
    roomNameColor.write(conf);
    roomDescColor.write(conf);
    prefixChar.write(conf);
    encodeEmoji.write(conf);
    decodeEmoji.write(conf);
}

void Configuration::MumeNativeSettings::write(QSettings &conf) const
{
    emulatedExits.write(conf);
    showHiddenExitFlags.write(conf);
    showNotes.write(conf);
}

void Configuration::MumeClientProtocolSettings::write(QSettings &conf) const
{
    internalRemoteEditor.write(conf);
    externalRemoteEditorCommand.write(conf);
}

void Configuration::PathMachineSettings::write(QSettings &conf) const
{
    acceptBestRelative.write(conf);
    acceptBestAbsolute.write(conf);
    newRoomPenalty.write(conf);
    correctPositionBonus.write(conf);
    maxPaths.write(conf);
    matchingTolerance.write(conf);
    multipleConnectionsPenalty.write(conf);
}

void Configuration::GroupManagerSettings::write(QSettings &conf) const
{
    color.write(conf);
    npcColor.write(conf);
    npcColorOverride.write(conf);
    npcHide.write(conf);
    npcSortBottom.write(conf);
}

void Configuration::MumeClockSettings::write(QSettings &conf) const
{
    conf.setValue(QString::fromStdString(startEpoch.getKey()),
                  static_cast<qlonglong>(startEpoch.get()));
    display.write(conf);
}

void Configuration::AdventurePanelSettings::write(QSettings &conf) const
{
    displayXPStatus.write(conf);
}

void Configuration::AudioSettings::write(QSettings &conf) const
{
    if constexpr (CURRENT_PLATFORM != PlatformEnum::Wasm) {
        unlocked.write(conf);
    }
    musicVolume.write(conf);
    soundVolume.write(conf);
    outputDeviceId.write(conf);
}

void Configuration::IntegratedMudClientSettings::write(QSettings &conf) const
{
    font.write(conf);
    backgroundColor.write(conf);
    foregroundColor.write(conf);
    columns.write(conf);
    rows.write(conf);
    linesOfScrollback.write(conf);
    linesOfInputHistory.write(conf);
    tabCompletionDictionarySize.write(conf);
    clearInputOnEnter.write(conf);
    autoResizeTerminal.write(conf);
    linesOfPeekPreview.write(conf);
    audibleBell.write(conf);
    visualBell.write(conf);
    useCommandSeparator.write(conf);
    commandSeparator.write(conf);
}

void Configuration::RoomPanelSettings::write(QSettings &conf) const
{
    geometry.write(conf);
}

void Configuration::InfomarksDialog::write(QSettings &conf) const
{
    geometry.write(conf);
}

void Configuration::RoomEditDialog::write(QSettings &conf) const
{
    geometry.write(conf);
}

void Configuration::FindRoomsDialog::write(QSettings &conf) const
{
    geometry.write(conf);
}

Configuration &setConfig()
{
    assert(g_config_enteredMain);
    assert(g_thread == std::this_thread::get_id());
    static Configuration conf;
    return conf;
}

const Configuration &getConfig()
{
    return setConfig();
}

Configuration::NamedColorOptions::NamedColorOptions()
{
#define X_INIT_COLOR(_id, _name) _id = ConfigValue<Color>("Colors/" _name, _name, Color());
    XFOREACH_NAMED_COLOR_OPTIONS(X_INIT_COLOR)
#undef X_INIT_COLOR
}

void Configuration::NamedColorOptions::resetToDefaults()
{
    static const auto fromHashHex = [](std::string_view sv) {
        assert(sv.length() == 7 && sv[0] == char_consts::C_POUND_SIGN);
        return Color::fromHex(sv.substr(1));
    };

    static const Color background = fromHashHex(DEFAULT_BGCOLOR);
    static const Color darkRoom = fromHashHex(DEFAULT_DARK_COLOR);
    static const Color noSundeath = fromHashHex(DEFAULT_NO_SUNDEATH_COLOR);
    static const Color road{140, 83, 58};
    static const Color special{204, 25, 204};
    static const Color noflee{123, 63, 0};
    static const Color water{76, 216, 255};

    BACKGROUND = background;
    CONNECTION_NORMAL = Colors::white;
    HIGHLIGHT_UNSAVED = Colors::cyan;
    HIGHLIGHT_TEMPORARY = Colors::red;
    HIGHLIGHT_NEEDS_SERVER_ID = Colors::yellow;
    INFOMARK_COMMENT = Colors::gray75;
    INFOMARK_HERB = Colors::green;
    INFOMARK_MOB = Colors::red;
    INFOMARK_OBJECT = Colors::yellow;
    INFOMARK_RIVER = water;
    INFOMARK_ROAD = road;
    ROOM_DARK = darkRoom;
    ROOM_NO_SUNDEATH = noSundeath;
    STREAM = water;
    TRANSPARENT = Color(0, 0, 0, 0);
    VERTICAL_COLOR_CLIMB = Colors::webGray;
    VERTICAL_COLOR_REGULAR_EXIT = Colors::white;
    WALL_COLOR_BUG_WALL_DOOR = Colors::red20;
    WALL_COLOR_CLIMB = Colors::gray70;
    WALL_COLOR_FALL_DAMAGE = Colors::cyan;
    WALL_COLOR_GUARDED = Colors::yellow;
    WALL_COLOR_NO_FLEE = noflee;
    WALL_COLOR_NO_MATCH = Colors::blue;
    WALL_COLOR_NOT_MAPPED = Colors::darkOrange1;
    WALL_COLOR_RANDOM = Colors::red;
    WALL_COLOR_REGULAR_EXIT = Colors::black;
    WALL_COLOR_SPECIAL = special;
}

Configuration::CanvasSettings::Advanced::Advanced()
{
    for (auto *const it : {(ConfigValue<bool> *) &use3D, (ConfigValue<bool> *) &autoTilt,
                           (ConfigValue<bool> *) &printPerfStats}) {
        const char *const name = it->getKey().c_str();
        if (std::optional<bool> opt = utils::getEnvBool(name)) {
            it->set(opt.value());
        }
    }
}

void Configuration::CanvasSettings::Advanced::registerChangeCallback(
    const ChangeMonitor::Lifetime &lifetime, const ChangeMonitor::Function &callback) const
{
    use3D.registerChangeCallback(lifetime, callback);
    autoTilt.registerChangeCallback(lifetime, callback);
    printPerfStats.registerChangeCallback(lifetime, callback);
    maximumFps.registerChangeCallback(lifetime, callback);
    fov.registerChangeCallback(lifetime, callback);
    verticalAngle.registerChangeCallback(lifetime, callback);
    horizontalAngle.registerChangeCallback(lifetime, callback);
    layerHeight.registerChangeCallback(lifetime, callback);
}

void Configuration::CanvasSettings::registerChangeCallback(
    const ChangeMonitor::Lifetime &lifetime, const ChangeMonitor::Function &callback) const
{
    backgroundColor.registerChangeCallback(lifetime, callback);
    connectionNormalColor.registerChangeCallback(lifetime, callback);
    roomDarkColor.registerChangeCallback(lifetime, callback);
    roomDarkLitColor.registerChangeCallback(lifetime, callback);

    antialiasingSamples.registerChangeCallback(lifetime, callback);
    trilinearFiltering.registerChangeCallback(lifetime, callback);
    showMissingMapId.registerChangeCallback(lifetime, callback);
    showUnsavedChanges.registerChangeCallback(lifetime, callback);
    showUnmappedExits.registerChangeCallback(lifetime, callback);
    drawUpperLayersTextured.registerChangeCallback(lifetime, callback);
    drawDoorNames.registerChangeCallback(lifetime, callback);
    softwareOpenGL.registerChangeCallback(lifetime, callback);
    resourcesDirectory.registerChangeCallback(lifetime, callback);
    drawCharBeacons.registerChangeCallback(lifetime, callback);
    charBeaconScaleCutoff.registerChangeCallback(lifetime, callback);
    doorNameScaleCutoff.registerChangeCallback(lifetime, callback);
    infomarkScaleCutoff.registerChangeCallback(lifetime, callback);
    extraDetailScaleCutoff.registerChangeCallback(lifetime, callback);
    mapRadius.registerChangeCallback(lifetime, callback);
    weatherAtmosphereIntensity.registerChangeCallback(lifetime, callback);
    weatherPrecipitationIntensity.registerChangeCallback(lifetime, callback);
    weatherTimeOfDayIntensity.registerChangeCallback(lifetime, callback);
    advanced.registerChangeCallback(lifetime, callback);
}

void Configuration::NamedColorOptions::registerChangeCallback(
    const ChangeMonitor::Lifetime &lifetime, const ChangeMonitor::Function &callback) const
{
#define X_REG_COLOR(_id, _name) _id.registerChangeCallback(lifetime, callback);
    XFOREACH_NAMED_COLOR_OPTIONS(X_REG_COLOR)
#undef X_REG_COLOR
}

void Configuration::setupGlobalCallbacks()
{
#define X_SYNC_COLOR(_id, _name)                                                                       XNamedColor(NamedColorEnum::_id).setColor(colorSettings._id.get());                                 colorSettings._id.registerChangeCallback(m_internalLifetime, []() {                                    XNamedColor(NamedColorEnum::_id).setColor(setConfig().colorSettings._id.get());                });
    XFOREACH_NAMED_COLOR_OPTIONS(X_SYNC_COLOR)
#undef X_SYNC_COLOR
}

void Configuration::registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                                           const ChangeMonitor::Function &callback) const
{
    general.registerChangeCallback(lifetime, callback);
    connection.registerChangeCallback(lifetime, callback);
    parser.registerChangeCallback(lifetime, callback);
    mumeClientProtocol.registerChangeCallback(lifetime, callback);
    mumeNative.registerChangeCallback(lifetime, callback);
    canvas.registerChangeCallback(lifetime, callback);
    colorSettings.registerChangeCallback(lifetime, callback);
    account.registerChangeCallback(lifetime, callback);
    autoLoad.registerChangeCallback(lifetime, callback);
    autoLog.registerChangeCallback(lifetime, callback);
    pathMachine.registerChangeCallback(lifetime, callback);
    groupManager.registerChangeCallback(lifetime, callback);
    mumeClock.registerChangeCallback(lifetime, callback);
    adventurePanel.registerChangeCallback(lifetime, callback);
    audio.registerChangeCallback(lifetime, callback);
    integratedClient.registerChangeCallback(lifetime, callback);
    roomPanel.registerChangeCallback(lifetime, callback);
    infomarksDialog.registerChangeCallback(lifetime, callback);
    roomEditDialog.registerChangeCallback(lifetime, callback);
    findRoomsDialog.registerChangeCallback(lifetime, callback);
    hotkeys.registerChangeCallback(lifetime, callback);
}

void setEnteredMain()
{
    g_thread = std::this_thread::get_id();
    g_config_enteredMain = true;
}

void Configuration::ResolvedNamedColorOptions::setFrom(const NamedColorOptions &from)
{
#define X_CLONE(_id, _name) (this->_id) = (from._id);
    XFOREACH_NAMED_COLOR_OPTIONS(X_CLONE)
#undef X_CLONE
}

void Configuration::ResolvedCanvasNamedColorOptions::setFrom(const CanvasNamedColorOptions &from)
{
#define X_CLONE(_id, _name) (this->_id) = (from._id);
    XFOREACH_CANVAS_NAMED_COLOR_OPTIONS(X_CLONE)
#undef X_CLONE
}

const Configuration::ResolvedNamedColorOptions &getNamedColorOptions()
{
    assert(g_config_enteredMain);
    if (g_thread == std::this_thread::get_id()) {
        thread_local Configuration::ResolvedNamedColorOptions tl_res;
        tl_res.setFrom(getConfig().colorSettings);
        return tl_res;
    }
    return deref(tl_named_color_options);
}

const Configuration::ResolvedCanvasNamedColorOptions &getCanvasNamedColorOptions()
{
    assert(g_config_enteredMain);
    if (g_thread == std::this_thread::get_id()) {
        thread_local Configuration::ResolvedCanvasNamedColorOptions tl_res;
        tl_res.setFrom(getConfig().canvas);
        return tl_res;
    }
    return deref(tl_canvas_named_color_options);
}

ThreadLocalNamedColorRaii::ThreadLocalNamedColorRaii(
    SharedCanvasNamedColorOptions canvasNamedColorOptions, SharedNamedColorOptions namedColorOptions)
{
    assert(g_config_enteredMain);
    assert(g_thread != std::this_thread::get_id());
    tl_canvas_named_color_options = std::move(canvasNamedColorOptions);
    tl_named_color_options = std::move(namedColorOptions);
}

ThreadLocalNamedColorRaii::~ThreadLocalNamedColorRaii()
{
    tl_canvas_named_color_options.reset();
    tl_named_color_options.reset();
}
