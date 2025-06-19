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
        bool noClientPanel = false;
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
    X(backgroundColor, BACKGROUND_NAME) \
    X(connectionNormalColor, CONNECTION_NORMAL_NAME) \
    X(roomDarkColor, ROOM_DARK_NAME) \
    X(roomDarkLitColor, ROOM_NO_SUNDEATH_NAME)

    struct NODISCARD CanvasNamedColorOptions
    {
#define X_DECL(_id, _name) XNamedColor _id{_name};
        XFOREACH_CANVAS_NAMED_COLOR_OPTIONS(X_DECL)
#undef X_DECL

        NODISCARD std::shared_ptr<const CanvasNamedColorOptions> clone() const
        {
            auto pResult = std::make_shared<CanvasNamedColorOptions>();
            auto &result = deref(pResult);
#define X_CLONE(_id, _name) result._id = this->_id.getColor();
            XFOREACH_CANVAS_NAMED_COLOR_OPTIONS(X_CLONE)
#undef X_CLONE
            return pResult;
        }
    };

    struct NODISCARD CanvasSettings final : public CanvasNamedColorOptions
    {
    private:
        ChangeMonitor m_canvasChangeMonitor;

        // Original plain members converted to private
        bool m_drawUpperLayersTextured = false;
        bool m_drawDoorNames = false;
        int m_antialiasingSamples = 0;
        bool m_trilinearFiltering = false;
        bool m_softwareOpenGL = false;
        QString m_resourcesDirectory = "";

    public:
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime, ChangeMonitor::Function callback) {
            m_canvasChangeMonitor.registerChangeCallback(lifetime, std::move(callback));
        }

        NODISCARD bool getDrawUpperLayersTextured() const { return m_drawUpperLayersTextured; }
        void setDrawUpperLayersTextured(bool value) {
            if (m_drawUpperLayersTextured != value) {
                m_drawUpperLayersTextured = value;
                m_canvasChangeMonitor.notifyAll();
            }
        }

        NODISCARD bool getDrawDoorNames() const { return m_drawDoorNames; }
        void setDrawDoorNames(bool value) {
            if (m_drawDoorNames != value) {
                m_drawDoorNames = value;
                m_canvasChangeMonitor.notifyAll();
            }
        }

        NODISCARD int getAntialiasingSamples() const { return m_antialiasingSamples; }
        void setAntialiasingSamples(int value) {
            if (m_antialiasingSamples != value) {
                m_antialiasingSamples = value;
                m_canvasChangeMonitor.notifyAll();
            }
        }

        NODISCARD bool getTrilinearFiltering() const { return m_trilinearFiltering; }
        void setTrilinearFiltering(bool value) {
            if (m_trilinearFiltering != value) {
                m_trilinearFiltering = value;
                m_canvasChangeMonitor.notifyAll();
            }
        }

        NODISCARD bool getSoftwareOpenGL() const { return m_softwareOpenGL; }
        void setSoftwareOpenGL(bool value) {
            if (m_softwareOpenGL != value) {
                m_softwareOpenGL = value;
                m_canvasChangeMonitor.notifyAll();
            }
        }

        NODISCARD QString getResourcesDirectory() const { return m_resourcesDirectory; }
        void setResourcesDirectory(const QString& value) {
            if (m_resourcesDirectory != value) {
                m_resourcesDirectory = value;
                m_canvasChangeMonitor.notifyAll();
            }
        }

        // NamedConfig members remain public as they have their own notification system (if any)
        // or are handled differently by Q_PROPERTY if used.
        NamedConfig<bool> showMissingMapId{"SHOW_MISSING_MAPID", false};
        NamedConfig<bool> showUnsavedChanges{"SHOW_UNSAVED_CHANGES", false};
        NamedConfig<bool> showUnmappedExits{"SHOW_UNMAPPED_EXITS", false};

        // not saved yet: (these remain as they are, not converted)
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

#define XFOREACH_NAMED_COLOR_OPTIONS(X) \
    X(BACKGROUND, BACKGROUND_NAME) \
    X(INFOMARK_COMMENT, "infomark-comment") \
    X(INFOMARK_HERB, "infomark-herb") \
    X(INFOMARK_MOB, "infomark-mob") \
    X(INFOMARK_OBJECT, "infomark-object") \
    X(INFOMARK_RIVER, "infomark-river") \
    X(INFOMARK_ROAD, "infomark-road") \
    X(CONNECTION_NORMAL, CONNECTION_NORMAL_NAME) \
    X(ROOM_DARK, ROOM_DARK_NAME) \
    X(ROOM_NO_SUNDEATH, ROOM_NO_SUNDEATH_NAME) \
    X(STREAM, "stream") \
    X(TRANSPARENT, ".transparent") \
    X(VERTICAL_COLOR_CLIMB, "vertical-climb") \
    X(VERTICAL_COLOR_REGULAR_EXIT, "vertical-regular") \
    X(WALL_COLOR_BUG_WALL_DOOR, "wall-bug-wall-door") \
    X(WALL_COLOR_CLIMB, "wall-climb") \
    X(WALL_COLOR_FALL_DAMAGE, "wall-fall-damage") \
    X(WALL_COLOR_GUARDED, "wall-guarded") \
    X(WALL_COLOR_NO_FLEE, "wall-no-flee") \
    X(WALL_COLOR_NO_MATCH, "wall-no-match") \
    X(WALL_COLOR_NOT_MAPPED, "wall-not-mapped") \
    X(WALL_COLOR_RANDOM, "wall-random") \
    X(WALL_COLOR_REGULAR_EXIT, "wall-regular-exit") \
    X(WALL_COLOR_SPECIAL, "wall-special")

    struct NODISCARD NamedColorOptions
    {
#define X_DECL(_id, _name) XNamedColor _id{_name};
        XFOREACH_NAMED_COLOR_OPTIONS(X_DECL)
#undef X_DECL
        NamedColorOptions() = default;
        void resetToDefaults();

        NODISCARD std::shared_ptr<const NamedColorOptions> clone() const
        {
            auto pResult = std::make_shared<NamedColorOptions>();
            auto &result = deref(pResult);
#define X_CLONE(_id, _name) result._id = this->_id.getColor();
            XFOREACH_NAMED_COLOR_OPTIONS(X_CLONE)
#undef X_CLONE
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
    private:
        ChangeMonitor m_groupManagerChangeMonitor;
        QColor m_color; // Default QColor is invalid, or use Qt::black/white if specific default desired
        QColor m_npcColor;
        bool m_npcColorOverride = false;
        bool m_npcSortBottom = false;
        bool m_npcHide = false;

    public:
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime, ChangeMonitor::Function callback) {
            m_groupManagerChangeMonitor.registerChangeCallback(lifetime, std::move(callback));
        }

        NODISCARD QColor getColor() const { return m_color; }
        void setColor(const QColor& value) {
            if (m_color != value) {
                m_color = value;
                m_groupManagerChangeMonitor.notifyAll();
            }
        }

        NODISCARD QColor getNpcColor() const { return m_npcColor; }
        void setNpcColor(const QColor& value) {
            if (m_npcColor != value) {
                m_npcColor = value;
                m_groupManagerChangeMonitor.notifyAll();
            }
        }

        NODISCARD bool getNpcColorOverride() const { return m_npcColorOverride; }
        void setNpcColorOverride(bool value) {
            if (m_npcColorOverride != value) {
                m_npcColorOverride = value;
                m_groupManagerChangeMonitor.notifyAll();
            }
        }

        NODISCARD bool getNpcSortBottom() const { return m_npcSortBottom; }
        void setNpcSortBottom(bool value) {
            if (m_npcSortBottom != value) {
                m_npcSortBottom = value;
                m_groupManagerChangeMonitor.notifyAll();
            }
        }

        NODISCARD bool getNpcHide() const { return m_npcHide; }
        void setNpcHide(bool value) {
            if (m_npcHide != value) {
                m_npcHide = value;
                m_groupManagerChangeMonitor.notifyAll();
            }
        }

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
    private:
        ChangeMonitor m_clientSettingsChangeMonitor;
        QString m_font;
        QColor m_foregroundColor; // Default QColor is invalid, consider Qt::lightGray
        QColor m_backgroundColor; // Default QColor is invalid, consider Qt::black
        int m_columns = 80; // Typical default
        int m_rows = 24;    // Typical default
        int m_linesOfScrollback = 10000;
        int m_linesOfInputHistory = 100;
        int m_tabCompletionDictionarySize = 100;
        bool m_clearInputOnEnter = true; // Common default
        bool m_autoResizeTerminal = true; // Common default
        int m_linesOfPeekPreview = 7;

    public:
        void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime, ChangeMonitor::Function callback) {
            m_clientSettingsChangeMonitor.registerChangeCallback(lifetime, std::move(callback));
        }

        NODISCARD const QString& getFont() const { return m_font; }
        void setFont(const QString& value) {
            if (m_font != value) {
                m_font = value;
                m_clientSettingsChangeMonitor.notifyAll();
            }
        }

        NODISCARD const QColor& getForegroundColor() const { return m_foregroundColor; }
        void setForegroundColor(const QColor& value) {
            if (m_foregroundColor != value) {
                m_foregroundColor = value;
                m_clientSettingsChangeMonitor.notifyAll();
            }
        }

        NODISCARD const QColor& getBackgroundColor() const { return m_backgroundColor; }
        void setBackgroundColor(const QColor& value) {
            if (m_backgroundColor != value) {
                m_backgroundColor = value;
                m_clientSettingsChangeMonitor.notifyAll();
            }
        }

        NODISCARD int getColumns() const { return m_columns; }
        void setColumns(int value) {
            if (m_columns != value) {
                m_columns = value;
                m_clientSettingsChangeMonitor.notifyAll();
            }
        }

        NODISCARD int getRows() const { return m_rows; }
        void setRows(int value) {
            if (m_rows != value) {
                m_rows = value;
                m_clientSettingsChangeMonitor.notifyAll();
            }
        }

        NODISCARD int getLinesOfScrollback() const { return m_linesOfScrollback; }
        void setLinesOfScrollback(int value) {
            if (m_linesOfScrollback != value) {
                m_linesOfScrollback = value;
                m_clientSettingsChangeMonitor.notifyAll();
            }
        }

        NODISCARD int getLinesOfInputHistory() const { return m_linesOfInputHistory; }
        void setLinesOfInputHistory(int value) {
            if (m_linesOfInputHistory != value) {
                m_linesOfInputHistory = value;
                m_clientSettingsChangeMonitor.notifyAll();
            }
        }

        NODISCARD int getTabCompletionDictionarySize() const { return m_tabCompletionDictionarySize; }
        void setTabCompletionDictionarySize(int value) {
            if (m_tabCompletionDictionarySize != value) {
                m_tabCompletionDictionarySize = value;
                m_clientSettingsChangeMonitor.notifyAll();
            }
        }

        NODISCARD bool getClearInputOnEnter() const { return m_clearInputOnEnter; }
        void setClearInputOnEnter(bool value) {
            if (m_clearInputOnEnter != value) {
                m_clearInputOnEnter = value;
                m_clientSettingsChangeMonitor.notifyAll();
            }
        }

        NODISCARD bool getAutoResizeTerminal() const { return m_autoResizeTerminal; }
        void setAutoResizeTerminal(bool value) {
            if (m_autoResizeTerminal != value) {
                m_autoResizeTerminal = value;
                m_clientSettingsChangeMonitor.notifyAll();
            }
        }

        NODISCARD int getLinesOfPeekPreview() const { return m_linesOfPeekPreview; }
        void setLinesOfPeekPreview(int value) {
            if (m_linesOfPeekPreview != value) {
                m_linesOfPeekPreview = value;
                m_clientSettingsChangeMonitor.notifyAll();
            }
        }

    private:
        SUBGROUP();
    } integratedClient;

    struct NODISCARD RoomPanelSettings final
    {
        QByteArray geometry;

    private:
        SUBGROUP();
    } roomPanel;

    struct NODISCARD InfoMarksDialog final
    {
        QByteArray geometry;

    private:
        SUBGROUP();
    } infoMarksDialog;

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
using SharedNamedColorOptions = std::shared_ptr<const Configuration::NamedColorOptions>;

const Configuration::CanvasNamedColorOptions &getCanvasNamedColorOptions();
const Configuration::NamedColorOptions &getNamedColorOptions();

struct NODISCARD ThreadLocalNamedColorRaii final
{
    explicit ThreadLocalNamedColorRaii(SharedCanvasNamedColorOptions canvasNamedColorOptions,
                                       SharedNamedColorOptions namedColorOptions);
    ~ThreadLocalNamedColorRaii();
};
