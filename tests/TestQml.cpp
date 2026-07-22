// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TestQml.h"

#include "../src/adventure/AdventureLogModel.h"
#include "../src/adventure/XpStatusAdapter.h"
#include "../src/adventure/adventuretracker.h"
#include "../src/client/ClientController.h"
#include "../src/client/ClientLineModel.h"
#include "../src/client/HotkeyManager.h"
#include "../src/clock/ClockAdapter.h"
#include "../src/clock/mumeclock.h"
#include "../src/configuration/configuration.h"
#include "../src/display/Filenames.h"
#include "../src/display/MapViewModel.h"
#include "../src/global/AnsiHtml.h"
#include "../src/global/AsyncTasks.h"
#include "../src/global/progresscounter.h"
#include "../src/group/CGroupChar.h"
#include "../src/group/GroupModel.h"
#include "../src/mainwindow/AboutInfo.h"
#include "../src/mainwindow/AnsiColorPickerController.h"
#include "../src/mainwindow/AudioVolumeController.h"
#include "../src/mainwindow/CheckableFlagModel.h"
#include "../src/mainwindow/CommandRegistry.h"
#include "../src/mainwindow/FindRoomsModel.h"
#include "../src/mainwindow/IoTaskController.h"
#include "../src/mainwindow/LogModel.h"
#include "../src/mainwindow/TasksModel.h"
#include "../src/mainwindow/UiCommand.h"
#include "../src/mainwindow/UpdateChecker.h"
#include "../src/map/Map.h"
#include "../src/map/RawRoom.h"
#include "../src/map/RoomHandle.h"
#include "../src/map/coordinate.h"
#include "../src/map/mmapper2room.h"
#include "../src/media/DescriptionAdapter.h"
#include "../src/media/MediaLibrary.h"
#include "../src/mpi/RemoteEditController.h"
#include "../src/mpi/remoteedit.h"
#include "../src/mpi/remoteeditsession.h"
#include "../src/observer/gameobserver.h"
#include "../src/preferences/PasswordDialogController.h"
#include "../src/preferences/PreferencesController.h"
#include "../src/proxy/GmcpMessage.h"
#include "../src/qml/DescriptionImageProvider.h"
#include "../src/qml/DockLayoutController.h"
#include "../src/qml/QmlConfig.h"
#include "../src/qml/QmlDialog.h"
#include "../src/qml/QmlDockWidget.h"
#include "../src/qml/RemoteEditDialogHost.h"
#include "../src/qml/ToolbarLayoutController.h"
#include "../src/roompanel/RoomManager.h"
#include "../src/roompanel/RoomModel.h"
#include "../src/timers/CTimers.h"
#include "../src/timers/TimerController.h"
#include "../src/timers/TimerModel.h"
#include "../src/viewers/AnsiViewContent.h"
#include "FakeClientBackend.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFontMetricsF>
#include <QImage>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlIncubationController>
#include <QQuickItem>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QScopedPointer>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolTip>
#include <QWidget>
#include <QtMath>
#include <QtTest/QtTest>

namespace { // anonymous

// GroupController's real constructor needs a live Mmapper2Group and MapData,
// and MapData drags in mapfrontend/parser/mapstorage, none of which are
// linked into TestQml's small executable (unlike TestMainWindow, no test
// binary currently links MapData; see mainwindow_SRCS in
// tests/CMakeLists.txt, which only exercises AudioVolumeSlider/UpdateDialog).
// This stub exposes GroupController's exact Q_INVOKABLE surface so
// GroupPanel.qml's context-menu bindings (which call groupController.
// canCenter()/centerOnCharacter()/recolorCharacter()) resolve normally,
// without needing a real map to center on.
class NODISCARD_QOBJECT GroupControllerStub final : public QObject
{
    Q_OBJECT

public:
    explicit GroupControllerStub(QObject *const parent)
        : QObject(parent)
    {}

public:
    Q_INVOKABLE void centerOnCharacter(int /*proxyRow*/) {}
    Q_INVOKABLE void recolorCharacter(int /*proxyRow*/) {}
    Q_INVOKABLE void moveCharacter(int /*fromProxyRow*/, int /*toProxyRow*/) {}
    Q_INVOKABLE void refreshFilter() {}
    NODISCARD Q_INVOKABLE bool canCenter(int /*proxyRow*/) const { return false; }
};

// InfomarkEditController's real constructor needs setInfomarkSelection() to
// be called with a live MapData (see ../src/mainwindow/InfomarkEditController.h),
// and MapData drags in mapfrontend/parser/mapstorage, none of which are
// linked into TestQml's small executable (same tradeoff as
// GroupControllerStub above). This stub exposes InfomarkEditController's
// exact Q_PROPERTY/Q_INVOKABLE surface (see that header's QML-contract doc
// comment) so InfomarkEditDialog.qml's bindings resolve normally;
// loadInfomarkEditDialog() below drives it directly to exercise the QML's
// enable/disable wiring without needing a real map.
class NODISCARD_QOBJECT InfomarkEditControllerStub final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList markerNames READ getMarkerNames NOTIFY sig_markerNamesChanged)
    Q_PROPERTY(
        int currentIndex READ getCurrentIndex WRITE setCurrentIndex NOTIFY sig_currentIndexChanged)
    Q_PROPERTY(int type READ getType NOTIFY sig_typeChanged)
    Q_PROPERTY(int markerClass READ getMarkerClass NOTIFY sig_markerClassChanged)
    Q_PROPERTY(QString text READ getText NOTIFY sig_textChanged)
    Q_PROPERTY(int x1 READ getX1 NOTIFY sig_x1Changed)
    Q_PROPERTY(int y1 READ getY1 NOTIFY sig_y1Changed)
    Q_PROPERTY(int x2 READ getX2 NOTIFY sig_x2Changed)
    Q_PROPERTY(int y2 READ getY2 NOTIFY sig_y2Changed)
    Q_PROPERTY(int layer READ getLayer NOTIFY sig_layerChanged)
    Q_PROPERTY(int angle READ getAngle NOTIFY sig_angleChanged)
    Q_PROPERTY(bool canModify READ getCanModify NOTIFY sig_canModifyChanged)

public:
    explicit InfomarkEditControllerStub(QObject *const parent)
        : QObject(parent)
    {
        m_markerNames << QStringLiteral("Create New Marker") << QStringLiteral("MarkerOne");
    }

public:
    NODISCARD QStringList getMarkerNames() const { return m_markerNames; }
    NODISCARD int getCurrentIndex() const { return m_currentIndex; }
    NODISCARD int getType() const { return m_type; }
    NODISCARD int getMarkerClass() const { return m_markerClass; }
    NODISCARD QString getText() const { return m_text; }
    NODISCARD int getX1() const { return m_x1; }
    NODISCARD int getY1() const { return m_y1; }
    NODISCARD int getX2() const { return m_x2; }
    NODISCARD int getY2() const { return m_y2; }
    NODISCARD int getLayer() const { return m_layer; }
    NODISCARD int getAngle() const { return m_angle; }
    NODISCARD bool getCanModify() const { return m_canModify; }

    void setCurrentIndex(int index)
    {
        m_currentIndex = index;
        m_canModify = index > 0;
        emit sig_currentIndexChanged();
        emit sig_canModifyChanged();
        emit sig_refreshed();
    }

    Q_INVOKABLE void setType(int type)
    {
        m_type = type;
        emit sig_typeChanged();
        emit sig_refreshed();
    }
    Q_INVOKABLE void setMarkerClass(int cls)
    {
        m_markerClass = cls;
        emit sig_markerClassChanged();
    }
    Q_INVOKABLE void create(int, int, const QString &, int, int, int, int, int, int) {}
    Q_INVOKABLE void modify(int, int, const QString &, int, int, int, int, int, int) {}

signals:
    void sig_markerNamesChanged();
    void sig_currentIndexChanged();
    void sig_typeChanged();
    void sig_markerClassChanged();
    void sig_textChanged();
    void sig_x1Changed();
    void sig_y1Changed();
    void sig_x2Changed();
    void sig_y2Changed();
    void sig_layerChanged();
    void sig_angleChanged();
    void sig_canModifyChanged();
    void sig_refreshed();
    void sig_error(const QString &);

private:
    QStringList m_markerNames;
    int m_currentIndex = 0;
    int m_type = 0;
    int m_markerClass = 0;
    QString m_text;
    int m_x1 = 0;
    int m_y1 = 0;
    int m_x2 = 0;
    int m_y2 = 0;
    int m_layer = 0;
    int m_angle = 0;
    bool m_canModify = false;
};

// RoomEditController's real constructor is fine to link (it takes no
// MapData), but setRoomSelection() needs a live MapData/SharedRoomSelection,
// which drags in mapfrontend/parser/mapstorage the same way
// InfomarkEditController's setInfomarkSelection() does (see
// InfomarkEditControllerStub above). This stub exposes the controller's
// exact Q_PROPERTY/Q_INVOKABLE surface (see
// ../src/mainwindow/RoomEditController.h's doc comment) using four REAL
// CheckableFlagModel instances (seeded with a handful of rows, including one
// PartiallyChecked row so loadRoomEditDialog() can assert tri-state
// rendering), so RoomEditDialog.qml's bindings resolve normally without
// needing a real map.
class NODISCARD_QOBJECT RoomEditControllerStub final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList roomNames READ getRoomNames NOTIFY sig_roomNamesChanged)
    Q_PROPERTY(int currentRoomIndex READ getCurrentRoomIndex WRITE setCurrentRoomIndex NOTIFY
                   sig_currentRoomIndexChanged)
    Q_PROPERTY(bool hasSelectedRoom READ getHasSelectedRoom NOTIFY sig_hasSelectedRoomChanged)

    Q_PROPERTY(CheckableFlagModel *mobFlagsModel READ getMobFlagsModel CONSTANT)
    Q_PROPERTY(CheckableFlagModel *loadFlagsModel READ getLoadFlagsModel CONSTANT)
    Q_PROPERTY(CheckableFlagModel *exitFlagsModel READ getExitFlagsModel CONSTANT)
    Q_PROPERTY(CheckableFlagModel *doorFlagsModel READ getDoorFlagsModel CONSTANT)

    Q_PROPERTY(int selectedExitDir READ getSelectedExitDir WRITE setSelectedExitDir NOTIFY
                   sig_selectedExitDirChanged)
    Q_PROPERTY(QString doorName READ getDoorName WRITE setDoorName NOTIFY sig_doorNameChanged)
    Q_PROPERTY(bool doorFieldsEnabled READ getDoorFieldsEnabled NOTIFY sig_doorFieldsEnabledChanged)

    Q_PROPERTY(int align READ getAlign WRITE setAlign NOTIFY sig_alignChanged)
    Q_PROPERTY(int portable READ getPortable WRITE setPortable NOTIFY sig_portableChanged)
    Q_PROPERTY(int rideable READ getRideable WRITE setRideable NOTIFY sig_rideableChanged)
    Q_PROPERTY(int light READ getLight WRITE setLight NOTIFY sig_lightChanged)
    Q_PROPERTY(int sundeath READ getSundeath WRITE setSundeath NOTIFY sig_sundeathChanged)
    Q_PROPERTY(int terrainType READ getTerrainType WRITE setTerrain NOTIFY sig_terrainTypeChanged)
    Q_PROPERTY(
        QUrl terrainPreviewIcon READ getTerrainPreviewIcon NOTIFY sig_terrainPreviewIconChanged)

    Q_PROPERTY(QString noteText READ getNoteText WRITE setNoteText NOTIFY sig_noteTextChanged)
    Q_PROPERTY(bool noteDirty READ getNoteDirty NOTIFY sig_noteDirtyChanged)

    Q_PROPERTY(QString descriptionHtml READ getDescriptionHtml NOTIFY sig_descriptionHtmlChanged)
    Q_PROPERTY(QString statsHtml READ getStatsHtml NOTIFY sig_statsHtmlChanged)
    Q_PROPERTY(QString diffHtml READ getDiffHtml NOTIFY sig_diffHtmlChanged)
    Q_PROPERTY(bool revertEnabled READ getRevertEnabled NOTIFY sig_revertEnabledChanged)

public:
    explicit RoomEditControllerStub(QObject *const parent)
        : QObject(parent)
        , m_mobFlagsModel(new CheckableFlagModel(this))
        , m_loadFlagsModel(new CheckableFlagModel(this))
        , m_exitFlagsModel(new CheckableFlagModel(this))
        , m_doorFlagsModel(new CheckableFlagModel(this))
    {
        m_roomNames << QStringLiteral("Room 1: Test Room");

        {
            std::vector<CheckableFlagModel::Row> rows;
            CheckableFlagModel::Row r1;
            r1.flagValue = 0;
            r1.name = QStringLiteral("Rent");
            r1.state = Qt::Checked;
            r1.checkable = true;
            rows.push_back(r1);
            CheckableFlagModel::Row r2;
            r2.flagValue = 1;
            r2.name = QStringLiteral("Shop");
            // A PartiallyChecked row so loadRoomEditDialog() can assert
            // tri-state rendering makes it through to the delegate.
            r2.state = Qt::PartiallyChecked;
            r2.checkable = true;
            rows.push_back(r2);
            m_mobFlagsModel->setRows(rows);
            m_loadFlagsModel->setRows(rows);
        }
        {
            std::vector<CheckableFlagModel::Row> rows;
            CheckableFlagModel::Row r1;
            r1.flagValue = 0;
            r1.name = QStringLiteral("Exit");
            r1.state = Qt::Unchecked;
            r1.checkable = true;
            rows.push_back(r1);
            CheckableFlagModel::Row r2;
            r2.flagValue = 1;
            r2.name = QStringLiteral("Door");
            r2.state = Qt::PartiallyChecked;
            r2.checkable = true;
            rows.push_back(r2);
            m_exitFlagsModel->setRows(rows);
            m_doorFlagsModel->setRows(rows);
        }
    }

public:
    NODISCARD QStringList getRoomNames() const { return m_roomNames; }
    NODISCARD int getCurrentRoomIndex() const { return m_currentRoomIndex; }
    NODISCARD bool getHasSelectedRoom() const { return m_hasSelectedRoom; }

    NODISCARD CheckableFlagModel *getMobFlagsModel() const { return m_mobFlagsModel; }
    NODISCARD CheckableFlagModel *getLoadFlagsModel() const { return m_loadFlagsModel; }
    NODISCARD CheckableFlagModel *getExitFlagsModel() const { return m_exitFlagsModel; }
    NODISCARD CheckableFlagModel *getDoorFlagsModel() const { return m_doorFlagsModel; }

    NODISCARD int getSelectedExitDir() const { return m_selectedExitDir; }
    NODISCARD QString getDoorName() const { return m_doorName; }
    NODISCARD bool getDoorFieldsEnabled() const { return m_doorFieldsEnabled; }

    NODISCARD int getAlign() const { return m_align; }
    NODISCARD int getPortable() const { return m_portable; }
    NODISCARD int getRideable() const { return m_rideable; }
    NODISCARD int getLight() const { return m_light; }
    NODISCARD int getSundeath() const { return m_sundeath; }
    NODISCARD int getTerrainType() const { return m_terrainType; }
    NODISCARD QUrl getTerrainPreviewIcon() const { return m_terrainPreviewIcon; }

    NODISCARD QString getNoteText() const { return m_noteText; }
    NODISCARD bool getNoteDirty() const { return m_noteDirty; }

    NODISCARD QString getDescriptionHtml() const { return m_descriptionHtml; }
    NODISCARD QString getStatsHtml() const { return m_statsHtml; }
    NODISCARD QString getDiffHtml() const { return m_diffHtml; }
    NODISCARD bool getRevertEnabled() const { return m_revertEnabled; }

    void setCurrentRoomIndex(int index)
    {
        m_currentRoomIndex = index;
        emit sig_currentRoomIndexChanged();
        emit sig_refreshed();
    }
    void setSelectedExitDir(int dir)
    {
        m_selectedExitDir = dir;
        emit sig_selectedExitDirChanged();
        emit sig_refreshed();
    }
    void setDoorName(const QString &name)
    {
        m_doorName = name;
        emit sig_doorNameChanged();
    }

    void setAlign(int value)
    {
        m_align = value;
        emit sig_alignChanged();
    }
    void setPortable(int value)
    {
        m_portable = value;
        emit sig_portableChanged();
    }
    void setRideable(int value)
    {
        m_rideable = value;
        emit sig_rideableChanged();
    }
    void setLight(int value)
    {
        m_light = value;
        emit sig_lightChanged();
    }
    void setSundeath(int value)
    {
        m_sundeath = value;
        emit sig_sundeathChanged();
    }
    void setTerrain(int value)
    {
        m_terrainType = value;
        emit sig_terrainTypeChanged();
    }

    void setNoteText(const QString &text)
    {
        m_noteText = text;
        emit sig_noteTextChanged();
        m_noteDirty = true;
        emit sig_noteDirtyChanged();
    }

    Q_INVOKABLE void toggleMobFlag(int row) { m_lastToggledMobRow = row; }
    Q_INVOKABLE void toggleLoadFlag(int row) { m_lastToggledLoadRow = row; }
    Q_INVOKABLE void toggleExitFlag(int row) { m_lastToggledExitRow = row; }
    Q_INVOKABLE void toggleDoorFlag(int row) { m_lastToggledDoorRow = row; }
    Q_INVOKABLE void toggleHiddenDoor() { ++m_hiddenDoorToggleCount; }

    NODISCARD Q_INVOKABLE QUrl terrainIcon(int /*type*/) const { return QUrl(); }

    Q_INVOKABLE void applyNote()
    {
        m_noteDirty = false;
        emit sig_noteDirtyChanged();
        emit sig_refreshed();
    }
    Q_INVOKABLE void revertNote()
    {
        m_noteDirty = false;
        emit sig_noteDirtyChanged();
    }
    Q_INVOKABLE void clearNote()
    {
        m_noteText.clear();
        emit sig_noteTextChanged();
        m_noteDirty = false;
        emit sig_noteDirtyChanged();
    }

    Q_INVOKABLE void revertDiff() { ++m_revertDiffCount; }

public:
    // Call-recording, inspected by loadRoomEditDialog().
    int m_lastToggledMobRow = -1;
    int m_lastToggledLoadRow = -1;
    int m_lastToggledExitRow = -1;
    int m_lastToggledDoorRow = -1;
    int m_hiddenDoorToggleCount = 0;
    int m_revertDiffCount = 0;

signals:
    void sig_requestUpdate();
    void sig_error(const QString &);

    void sig_roomNamesChanged();
    void sig_currentRoomIndexChanged();
    void sig_hasSelectedRoomChanged();

    void sig_selectedExitDirChanged();
    void sig_doorNameChanged();
    void sig_doorFieldsEnabledChanged();

    void sig_alignChanged();
    void sig_portableChanged();
    void sig_rideableChanged();
    void sig_lightChanged();
    void sig_sundeathChanged();
    void sig_terrainTypeChanged();
    void sig_terrainPreviewIconChanged();

    void sig_noteTextChanged();
    void sig_noteDirtyChanged();

    void sig_descriptionHtmlChanged();
    void sig_statsHtmlChanged();
    void sig_diffHtmlChanged();
    void sig_revertEnabledChanged();

    void sig_refreshed();

private:
    QStringList m_roomNames;
    int m_currentRoomIndex = 0;
    bool m_hasSelectedRoom = true;

    CheckableFlagModel *m_mobFlagsModel = nullptr;
    CheckableFlagModel *m_loadFlagsModel = nullptr;
    CheckableFlagModel *m_exitFlagsModel = nullptr;
    CheckableFlagModel *m_doorFlagsModel = nullptr;

    int m_selectedExitDir = 0;
    QString m_doorName;
    bool m_doorFieldsEnabled = true;

    int m_align = -1;
    int m_portable = -1;
    int m_rideable = -1;
    int m_light = -1;
    int m_sundeath = -1;
    int m_terrainType = -1;
    QUrl m_terrainPreviewIcon;

    QString m_noteText;
    bool m_noteDirty = false;

    QString m_descriptionHtml;
    QString m_statsHtml;
    QString m_diffHtml;
    bool m_revertEnabled = false;
};

// The real "MapCanvasItem" QML type is MapCanvasQuickItem (see
// ../src/display/MapCanvasQuickItem.h), a QQuickFramebufferObject that
// implements ICanvasHost and holds a `MapCanvasCore *core` property.
// Constructing (or even just registering, via qmlRegisterType<T>, which
// instantiates a QQmlPrivate::QQmlElement<T>) that real class pulls in
// MapCanvasCore's vtable/RTTI -- needed for Qt's automatic
// QMetaType<MapCanvasCore*> registration on the `core` property, even
// though this binary would never actually construct a MapCanvasCore -- and
// that cascades into MapCanvasCore's OpenGL/rendering member types
// (FrameManager, GLWeather, OpenGL, GLFont, MapCanvasViewport,
// MapCanvasInputState, ...), the same mapfrontend/parser/mapstorage-style
// dependency graph GroupControllerStub/InfomarkEditControllerStub above
// deliberately avoid.
//
// This stub exposes just enough of MapCanvasQuickItem's QML-visible surface
// (a plain QQuickItem so MapView.qml's `anchors.fill: parent` etc. work, and
// a `core` property so `core: root.core` binds without error) for
// loadMapView() to exercise MapView.qml's chrome/layout -- the thing this
// commit's QML file actually adds. It does NOT exercise
// MapCanvasQuickItem's own null-core render/event-handling logic; that's
// covered by code review and by the fact that the real class compiles and
// links cleanly into the production "mmapper" binary (see
// ../src/CMakeLists.txt's mm_qml block).
class NODISCARD_QOBJECT MapCanvasItemStub : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QObject *core READ getCore WRITE setCore NOTIFY coreChanged)

public:
    explicit MapCanvasItemStub(QQuickItem *const parent = nullptr)
        : QQuickItem(parent)
    {}

public:
    NODISCARD QObject *getCore() const { return m_core; }
    void setCore(QObject *const core)
    {
        if (m_core == core) {
            return;
        }
        m_core = core;
        emit coreChanged();
    }

signals:
    void coreChanged();

private:
    QObject *m_core = nullptr;
};

// The real "MapCanvasUnderlay" QML type is MapCanvasUnderlayItem (see
// ../src/display/MapCanvasUnderlayItem.h), a plain QQuickItem that hooks
// QQuickWindow::beforeRenderPassRecording() to draw MapCanvasCore directly
// into the window's own framebuffer -- the default canvas host MapView.qml
// selects (see its "useCanvasUnderlay" doc comment) unless
// MMAPPER_CANVAS_FBO=1 is set. Same reasoning as MapCanvasItemStub above for
// why the real class isn't linked into this binary: this stub is a second,
// separately-typed twin (rather than reusing MapCanvasItemStub under a
// second qmlRegisterType<>() name) specifically so loadMapView()/
// loadMapViewFboFallback() can each assert -- via findChild<T>() -- which of
// the two canvas types MapView.qml actually instantiated for a given
// "useCanvasUnderlay" context property value.
class NODISCARD_QOBJECT MapCanvasUnderlayItemStub : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QObject *core READ getCore WRITE setCore NOTIFY coreChanged)

public:
    explicit MapCanvasUnderlayItemStub(QQuickItem *const parent = nullptr)
        : QQuickItem(parent)
    {}

public:
    NODISCARD QObject *getCore() const { return m_core; }
    void setCore(QObject *const core)
    {
        if (m_core == core) {
            return;
        }
        m_core = core;
        emit coreChanged();
    }

signals:
    void coreChanged();

private:
    QObject *m_core = nullptr;
};

// Stands in for MapCanvasCore's userPressedEscape(Q_INVOKABLE, see
// ../src/display/MapCanvasCore.h) as the "mapCore" context property for
// mainShellEscapeShortcutForwards() below: a real MapCanvasCore needs OpenGL
// (see command_registry_SRCS's/loadMainShell()'s comments on why this small
// binary never links it), so this stub only exposes the one invokable
// MainShell.qml's Shortcut{sequence: "Escape"} calls, recording how many
// times (and with what argument) it was invoked.
class NODISCARD_QOBJECT MapCoreEscapeStub final : public QObject
{
    Q_OBJECT

public:
    explicit MapCoreEscapeStub(QObject *const parent = nullptr)
        : QObject(parent)
    {}

public:
    Q_INVOKABLE void userPressedEscape(bool pressed)
    {
        ++callCount;
        lastPressed = pressed;
    }

    int callCount = 0;
    bool lastPressed = false;
};

// Stands in for the "shell" context property's selection-state Q_PROPERTYs
// (see ../src/mainwindow/QmlShellWindow.h's Q_PROPERTY block:
// hasRoomSelection/roomSelectionEmpty/hasConnectionSelection/
// hasInfomarkSelection/infomarkSelectionEmpty) for
// loadMapContextMenu()/mapContextMenuSelectionSections() below:
// QmlShellWindow itself isn't linkable into this small test binary (same
// tradeoff as MapCoreEscapeStub above -- it drags in the OpenGL-backed
// MapCanvasCore), so this stub exposes just the five booleans
// MapContextMenu.qml (../src/qml/shell/MapContextMenu.qml) reads, settable
// directly from the test to drive each menu section's visibility.
class NODISCARD_QOBJECT ShellSelectionStub final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool hasRoomSelection MEMBER m_hasRoomSelection NOTIFY sig_changed)
    Q_PROPERTY(bool roomSelectionEmpty MEMBER m_roomSelectionEmpty NOTIFY sig_changed)
    Q_PROPERTY(bool hasConnectionSelection MEMBER m_hasConnectionSelection NOTIFY sig_changed)
    Q_PROPERTY(bool hasInfomarkSelection MEMBER m_hasInfomarkSelection NOTIFY sig_changed)
    Q_PROPERTY(bool infomarkSelectionEmpty MEMBER m_infomarkSelectionEmpty NOTIFY sig_changed)

public:
    explicit ShellSelectionStub(QObject *const parent = nullptr)
        : QObject(parent)
    {}

    void setRoomSelection(bool has, bool empty)
    {
        m_hasRoomSelection = has;
        m_roomSelectionEmpty = empty;
        emit sig_changed();
    }
    void setConnectionSelection(bool has)
    {
        m_hasConnectionSelection = has;
        emit sig_changed();
    }
    void setInfomarkSelection(bool has, bool empty)
    {
        m_hasInfomarkSelection = has;
        m_infomarkSelectionEmpty = empty;
        emit sig_changed();
    }

signals:
    void sig_changed();

private:
    bool m_hasRoomSelection = false;
    bool m_roomSelectionEmpty = true;
    bool m_hasConnectionSelection = false;
    bool m_hasInfomarkSelection = false;
    bool m_infomarkSelectionEmpty = true;
};

// Stands in for MapCanvasCore's sig_customContextMenuRequested(QPoint)/
// sig_dismissContextMenu() (see ../src/display/MapCanvasCore.h) as the
// "core" property MapContextMenu.qml's `Connections { target: root.core }`
// binds against -- same OpenGL-avoidance tradeoff as MapCoreEscapeStub
// above.
class NODISCARD_QOBJECT MapCoreContextMenuStub final : public QObject
{
    Q_OBJECT

public:
    explicit MapCoreContextMenuStub(QObject *const parent = nullptr)
        : QObject(parent)
    {}

signals:
    void sig_customContextMenuRequested(QPoint pos);
    void sig_dismissContextMenu();
};

} // namespace

// MediaLibrary.cpp calls getAssetsPath() (declared in display/Filenames.h),
// but linking display/Filenames.cpp would drag getParserCommandName() and
// with it most of the parser/mapdata dependency graph into this small test
// binary (the same problem loadGroupPanel() avoids with GroupIconProvider).
// Provide the one symbol MediaLibrary needs; an empty path simply means no
// sideloaded assets directory is scanned.
QString getAssetsPath()
{
    return QString();
}

// UpdateChecker.cpp uses the generated Version.cpp's symbols; stub them the
// same way TestMainWindow.cpp does so this test binary needs no generated
// sources.
const char *getMMapperVersion()
{
    return "v0.0.0-test";
}

const char *getMMapperBranch()
{
    return "test";
}

bool isMMapperBeta()
{
    return false;
}

TestQml::TestQml() = default;

TestQml::~TestQml() = default;

void TestQml::initTestCase()
{
    // Allows QQuickWidget to run without a GPU under the offscreen platform.
    // This also now matches production: main.cpp pins the QML scene graph to the
    // software backend for every platform (see the QQuickWindow::setGraphicsApi()
    // call there), so this test exercises the same renderer the shipped app uses
    // for its QQuickWidget dock panels, not just a headless-CI workaround.
    // (Setting QQuickWindow::setGraphicsApi() here directly isn't an option: by the
    // time initTestCase() runs, QTEST_MAIN has already constructed QApplication, and
    // the graphics API must be fixed before the QGuiApplication is instantiated.)
    qputenv("QT_QUICK_BACKEND", "software");
    // Required before any getConfig()/setConfig() call (see
    // qmlConfigRoundTrip()); see also TestMainWindow and TestClock.
    setEnteredMain();

    // async_tasks:: is a process-global singleton (see AsyncTasks.cpp's
    // g_tasks/g_cleaning_up statics) that can only be initialized once per
    // process: init() aborts if called again after cleanup() has run, so
    // unlike getConfig()/setConfig() this can't be scoped per-test with a
    // local init()/cleanup() pair. Initialize it once here for the whole
    // binary; cleanupTestCase() tears it down after the last test runs.
    // Required by tasksModelEmpty(), tasksModelHoldRemovalsRoundTrip(), and
    // tasksModelLifecycle(), all of which construct a TasksModel whose
    // internal QTimer calls async_tasks::for_each() on every tick.
    async_tasks::init();

    // Registers QML-only stand-ins for MapCanvasItem/MapCanvasUnderlay (the
    // real, production MapCanvasQuickItem/MapCanvasUnderlayItem C++ classes
    // -- see ../src/display/MapCanvasQuickItem.h,
    // ../src/display/MapCanvasUnderlayItem.h, and ../src/qml/QmlTypes.cpp)
    // so loadMapView()/loadMapViewFboFallback() can instantiate MapView.qml
    // with either "useCanvasUnderlay" value. See MapCanvasItemStub's/
    // MapCanvasUnderlayItemStub's own comments (below) and this file's
    // display_map_view_SRCS CMake comment for why the real classes aren't
    // linked into this binary.
    qmlRegisterType<MapCanvasItemStub>("MMapper", 1, 0, "MapCanvasItem");
    qmlRegisterType<MapCanvasUnderlayItemStub>("MMapper", 1, 0, "MapCanvasUnderlay");
}

void TestQml::cleanupTestCase()
{
    async_tasks::cleanup();
}

void TestQml::loadPanelFrame()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/MMapper/PanelFrame.qml"_qs));

    while (component.isLoading()) {
        QCoreApplication::processEvents();
    }

    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> object(component.create(engine.rootContext()));
    QVERIFY(object != nullptr);
}

void TestQml::loadPanelHeaderRow()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);

    // Instantiate PanelHeaderRow directly with a literal columns array (a
    // couple of fixed-width columns plus a fillWidth column, mirroring
    // GroupPanel.qml's/TimerPanel.qml's usage) rather than injecting the
    // columns via QObject::setProperty() after creation: writing a
    // QVariantList to a "property var" that way crashes inside QV4's
    // ExecutionEngine::fromData() when the item has no owning QQuickWindow,
    // whereas a literal QML binding goes through the normal compiled
    // binding path and works fine.
    component.setData(QByteArrayLiteral("import QtQuick\n"
                                        "import MMapper\n"
                                        "PanelHeaderRow {\n"
                                        "    width: 400\n"
                                        "    columns: [\n"
                                        "        { text: \"Name\", width: 140 },\n"
                                        "        { text: \"Room Name\", fillWidth: true }\n"
                                        "    ]\n"
                                        "}\n"),
                      QUrl(u"qrc:/qt/qml/MMapper/PanelHeaderRowTest.qml"_qs));

    while (component.isLoading()) {
        QCoreApplication::processEvents();
    }

    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> object(component.create(engine.rootContext()));
    QVERIFY(object != nullptr);
    QCoreApplication::processEvents();

    QCOMPARE(object->property("columns").toList().size(), 2);
    QVERIFY(object->property("implicitHeight").toReal() > 0);
}

void TestQml::qmlDockWidgetFallback()
{
    QmlDockWidget dock("t", "TestDock", nullptr);
    QVERIFY(dock.quickWidget() != nullptr);

    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/DoesNotExist.qml"_qs));

    while (dock.quickWidget() != nullptr && dock.quickWidget()->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    // Give the queued statusChanged connection a chance to run.
    QCoreApplication::processEvents();

    // The helper must not crash; either the QQuickWidget reports an Error
    // status, or it has already been swapped out for a fallback QLabel.
    if (dock.quickWidget() != nullptr) {
        QCOMPARE(dock.quickWidget()->status(), QQuickWidget::Error);
    } else {
        auto *const label = qobject_cast<QLabel *>(dock.widget());
        QVERIFY(label != nullptr);
    }
}

void TestQml::qmlDockWidgetLoads()
{
    QmlDockWidget dock("t", "TestDock2", nullptr);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/PanelFrame.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);
}

void TestQml::qmlDockWidgetMinimumFromImplicitSize()
{
    // Regression test for QML dock panels having no minimum size: unlike
    // the widget-era panels (GroupWidget::sizeHint(),
    // DisplayWidget::minimumSizeHint(), ...), QQuickWidget never reads its
    // root item's implicitWidth/implicitHeight on its own, so a QML dock
    // could be resized down to a sliver. QmlDockWidget::syncMinimumSize()
    // (see QmlDockWidget.cpp) copies the root item's implicit size onto the
    // QQuickWidget's minimumSize() once the root becomes Ready.
    CTimers timers(nullptr);
    timers.addCountdown("test-timer", "", 60000);

    TimerModel model(timers, nullptr);
    TimerController controller(timers, model, nullptr);

    QmlDockWidget dock("t", "TestDockMinSize", nullptr);
    dock.setContextProperty("timerModel", &model);
    dock.setContextProperty("timerController", &controller);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/TimerPanel.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCOMPARE(quick->status(), QQuickWidget::Ready);

    QQuickItem *const rootItem = quick->rootObject();
    QVERIFY(rootItem != nullptr);
    QVERIFY(rootItem->implicitWidth() > 0);
    QVERIFY(rootItem->implicitHeight() > 0);

    // The Ready->minimumSize() sync happens inside a queued statusChanged
    // handler, so poll rather than asserting immediately.
    QTRY_VERIFY(quick->minimumSize().width() > 0);
    QTRY_VERIFY(quick->minimumSize().height() > 0);

    QCOMPARE(quick->minimumSize().width(), qCeil(rootItem->implicitWidth()));
    QCOMPARE(quick->minimumSize().height(), qCeil(rootItem->implicitHeight()));
}

void TestQml::loadTimerPanel()
{
    CTimers timers(nullptr);
    timers.addCountdown("test-timer", "", 60000);

    TimerModel model(timers, nullptr);
    TimerController controller(timers, model, nullptr);

    QmlDockWidget dock("t", "TestDockTimers", nullptr);
    dock.setContextProperty("timerModel", &model);
    dock.setContextProperty("timerController", &controller);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/TimerPanel.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    // Let the delegate finish instantiating against the seeded timer.
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);
}

void TestQml::timerModelTickUpdatesAllRoles()
{
    // Regression test: TimerModel's periodic refresh lambda used to emit
    // dataChanged() with {Qt::DisplayRole, ProgressRole} as the changed
    // roles, but TimerPanel.qml's delegate binds model.time/model.name/
    // model.expired, which resolve to the custom TimeRole/NameRole/
    // ExpiredRole (see TimerModel::roleNames()), not Qt::DisplayRole. Those
    // QML bindings never re-evaluated, so a running count-up timer's
    // on-screen time looked frozen even though the underlying data (and the
    // widget-based TimerWidget, which repaints regardless of roles) kept
    // updating. See TimerModel.cpp's refresh lambda: it must emit an
    // *empty* roles vector (meaning "all roles changed").
    CTimers timers(nullptr);
    timers.addTimer("count-up", "");

    TimerModel model(timers, nullptr);
    const QModelIndex idx = model.index(0, TimerModel::ColTime);

    QSignalSpy spy(&model, &TimerModel::dataChanged);
    const QVariant before = model.data(idx, TimerModel::TimeRole);

    // The refresh timer fires every 50ms; wait for at least one tick.
    QTRY_VERIFY_WITH_TIMEOUT(!spy.isEmpty(), 2000);

    const QList<QVariant> lastSignal = spy.constLast();
    const auto roles = lastSignal.at(2).value<QList<int>>();
    QVERIFY2(roles.isEmpty(),
             "dataChanged() must report an empty roles list (\"all roles changed\") so QML's "
             "custom-role bindings (model.time/model.name/model.expired) re-evaluate");

    // formatMs() only has second granularity, so give it long enough for
    // the displayed elapsed time to actually roll over and confirm the
    // underlying data really is live, not just the signal shape.
    QTRY_VERIFY_WITH_TIMEOUT(model.data(idx, TimerModel::TimeRole) != before, 2000);
}

namespace {
// Fetches row 0's progress Rectangle (objectName "timerProgressRect") out of
// TimerPanel.qml's ListView. findChild() from the root down doesn't see
// delegate items directly (QQuickListView reparents delegates onto its
// contentItem outside the QObject-child walk findChildren() follows), so go
// through ListView's own itemAtIndex() instead, which is what QML/JS uses to
// look up a concrete delegate instance.
NODISCARD QQuickItem *timerPanelProgressRect(QQuickWidget &quick)
{
    auto *const listView = quick.rootObject()->findChild<QQuickItem *>(
        QStringLiteral("timerListView"));
    if (listView == nullptr) {
        return nullptr;
    }
    QQuickItem *delegateItem = nullptr;
    QMetaObject::invokeMethod(listView,
                              "itemAtIndex",
                              Q_RETURN_ARG(QQuickItem *, delegateItem),
                              Q_ARG(int, 0));
    if (delegateItem == nullptr) {
        return nullptr;
    }
    return delegateItem->findChild<QQuickItem *>(QStringLiteral("timerProgressRect"));
}
} // namespace

void TestQml::timerPanelProgressColor()
{
    // Ports TimerDelegate::paint()'s progress-bar coloring
    // (TimerDelegate.cpp): green fading to yellow above 50% remaining,
    // yellow fading to red below it. See TimerPanel.qml's progress
    // Rectangle (objectName "timerProgressRect") and ListView (objectName
    // "timerListView").
    {
        // Freshly-created long countdown: remaining/duration is ~1.0, so
        // this should land solidly in the "green" end of the gradient.
        CTimers timers(nullptr);
        timers.addCountdown("fresh", "", 1000000);

        TimerModel model(timers, nullptr);
        TimerController controller(timers, model, nullptr);

        QmlDockWidget dock("t", "TestDockTimerColorHigh", nullptr);
        dock.setContextProperty("timerModel", &model);
        dock.setContextProperty("timerController", &controller);
        dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/TimerPanel.qml"_qs));

        QQuickWidget *const quick = dock.quickWidget();
        QVERIFY(quick != nullptr);
        while (quick->status() == QQuickWidget::Loading) {
            QCoreApplication::processEvents();
        }
        QCoreApplication::processEvents();
        QCOMPARE(quick->status(), QQuickWidget::Ready);

        // ListView needs a non-zero size to instantiate any delegates at
        // all; give the dock a real size like the other panel tests do
        // (e.g. loadDescriptionPanel()).
        dock.resize(300, 200);
        dock.show();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();

        QQuickItem *rect = nullptr;
        QTRY_VERIFY((rect = timerPanelProgressRect(*quick)) != nullptr);
        const QColor color = rect->property("color").value<QColor>();
        QCOMPARE(color.greenF(), 1.0f);
        QVERIFY(color.redF() < 0.1f);
    }
    {
        // Countdown that has already run out: remaining/duration is 0, so
        // this should land solidly in the "red" end of the gradient.
        CTimers timers(nullptr);
        timers.addCountdown("expiring", "", 1);
        QTest::qWait(20);

        TimerModel model(timers, nullptr);
        TimerController controller(timers, model, nullptr);

        QmlDockWidget dock("t", "TestDockTimerColorLow", nullptr);
        dock.setContextProperty("timerModel", &model);
        dock.setContextProperty("timerController", &controller);
        dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/TimerPanel.qml"_qs));

        QQuickWidget *const quick = dock.quickWidget();
        QVERIFY(quick != nullptr);
        while (quick->status() == QQuickWidget::Loading) {
            QCoreApplication::processEvents();
        }
        QCoreApplication::processEvents();
        QCOMPARE(quick->status(), QQuickWidget::Ready);

        dock.resize(300, 200);
        dock.show();
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();

        QQuickItem *rect = nullptr;
        QTRY_VERIFY((rect = timerPanelProgressRect(*quick)) != nullptr);
        const QColor color = rect->property("color").value<QColor>();
        QCOMPARE(color.redF(), 1.0f);
        QVERIFY(color.greenF() < 0.1f);
    }
}

void TestQml::loadAdventurePanel()
{
    GameObserver observer;
    AdventureTracker tracker(observer, nullptr);
    AdventureLogModel model(tracker, nullptr);

    // AdventurePanel.qml themes its single TextArea from `config`
    // (integratedClient background/foreground/font, mirroring
    // AdventureWidget's ctor), same as loadGroupPanel()/loadClientPanel().
    QmlConfig config;

    QmlDockWidget dock("t", "TestDockAdventure", nullptr);
    dock.setContextProperty("adventureLogModel", &model);
    dock.setContextProperty("config", &config);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/AdventurePanel.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    // Let the delegate finish instantiating against the seeded log entry.
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);

    // The text control must expose the joined log content selectably (see
    // AdventureLogModel::text), replacing the old non-selectable ListView
    // delegate.
    auto *const textArea = quick->rootObject()->findChild<QQuickItem *>(
        QStringLiteral("adventureLogTextArea"));
    QVERIFY(textArea != nullptr);
    QVERIFY(textArea->property("selectByMouse").toBool());
    QVERIFY(textArea->property("readOnly").toBool());
    QVERIFY(textArea->property("text").toString().contains(
        QStringLiteral("Your adventures in Middle Earth will be tracked here!")));
}

void TestQml::roomModelLongestTextInColumn()
{
    RoomManager manager(nullptr);
    RoomModel model(nullptr, manager.getRoom());

    // Out-of-range columns must be bounds-checked even on the placeholder
    // (mob-less) model.
    QCOMPARE(model.longestTextInColumn(-1), QString());
    QCOMPARE(model.longestTextInColumn(static_cast<int>(RoomModel::ColumnTypeEnum::MOUNT) + 1),
             QString());

    const QJsonObject shortMobObj{{"id", 10}, {"name", "an orc"}, {"position", "standing"}};
    const QJsonObject longMobObj{{"id", 11},
                                 {"name", "a truly enormous and terrifying cave troll"},
                                 {"position", "standing"}};
    for (const QJsonObject &obj : {shortMobObj, longMobObj}) {
        const auto jsonStr = QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
        GmcpMessage addMessage(GmcpMessageTypeEnum::ROOM_CHARS_ADD, GmcpJson{jsonStr});
        manager.slot_parseGmcpInput(addMessage);
    }
    model.update();

    QCOMPARE(model.longestTextInColumn(static_cast<int>(RoomModel::ColumnTypeEnum::NAME)),
             QStringLiteral("a truly enormous and terrifying cave troll"));
    QCOMPARE(model.longestTextInColumn(-1), QString());
    QCOMPARE(model.longestTextInColumn(static_cast<int>(RoomModel::ColumnTypeEnum::MOUNT) + 1),
             QString());
}

void TestQml::loadRoomPanel()
{
    RoomManager manager(nullptr);

    const QJsonObject shortMobObj{{"id", 10}, {"name", "an orc"}, {"position", "standing"}};
    const QJsonObject longMobObj{{"id", 11},
                                 {"name", "a truly enormous and terrifying cave troll"},
                                 {"position", "standing"}};
    for (const QJsonObject &obj : {shortMobObj, longMobObj}) {
        const auto jsonStr = QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
        GmcpMessage addMessage(GmcpMessageTypeEnum::ROOM_CHARS_ADD, GmcpJson{jsonStr});
        manager.slot_parseGmcpInput(addMessage);
    }

    RoomModel model(nullptr, manager.getRoom());
    model.update();

    QmlDockWidget dock("t", "TestDockRoom", nullptr);
    dock.setContextProperty("roomModel", &model);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/RoomPanel.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();
    // Let the Qt.callLater(forceLayout) queued by the RoomModel Connections
    // handlers run so column widths are recomputed against seeded data.
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);

    auto *const tableView = quick->rootObject()->findChild<QQuickItem *>(
        QStringLiteral("roomTableView"));
    QVERIFY(tableView != nullptr);

    // The NAME column must be wide enough to fit the header text "Name";
    // this is a CI-verifiable proxy for the columnWidthProvider working
    // (we can't inspect rendered pixels under the offscreen platform).
    const QVariant columnWidthProvider = tableView->property("columnWidthProvider");
    QVERIFY(columnWidthProvider.isValid());

    QFontMetricsF metrics{QFont()};
    const qreal headerTextWidth = metrics.horizontalAdvance(QStringLiteral("Name"));

    QJSEngine *const jsEngine = qmlEngine(tableView);
    QVERIFY(jsEngine != nullptr);
    QJSValue provider = columnWidthProvider.value<QJSValue>();
    QVERIFY(provider.isCallable());
    QJSValue result = provider.call(QJSValueList{QJSValue(0)});
    QVERIFY(!result.isError());
    QVERIFY(result.toNumber() >= headerTextWidth);
}

void TestQml::loadLogPanel()
{
    LogModel model(nullptr);
    model.append("Test", "hello world");

    QmlDockWidget dock("t", "TestDockLog", nullptr);
    dock.setContextProperty("logModel", &model);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/LogPanel.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    // Let the delegate finish instantiating against the seeded log entry.
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);

    // The panel must expose a selectable read-only text control bound to
    // LogModel::text (rather than the old non-selectable ListView delegate),
    // restoring the widget-era QTextBrowser's drag-select + copy.
    auto *const textArea = quick->rootObject()->findChild<QQuickItem *>(
        QStringLiteral("logTextArea"));
    QVERIFY(textArea != nullptr);
    QVERIFY(textArea->property("selectByMouse").toBool());
    QVERIFY(textArea->property("readOnly").toBool());
    QCOMPARE(textArea->property("text").toString(), QStringLiteral("[Test] hello world"));
}

void TestQml::logModelCap()
{
    LogModel model(nullptr);

    for (int i = 1; i <= LogModel::MAX_LINES + 1; ++i) {
        model.append("Test", QString("line %1").arg(i));
    }

    QCOMPARE(model.rowCount(), LogModel::MAX_LINES);
    QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole).toString(),
             QStringLiteral("[Test] line 2"));

    model.clear();
    QCOMPARE(model.rowCount(), 0);
}

void TestQml::qmlConfigRoundTrip()
{
    const bool originalNpcHide = getConfig().groupManager.npcHide;
    const QColor originalGroupColor = getConfig().groupManager.color;

    // TestQml mutates the global Configuration singleton, which is shared
    // across all tests in this binary; always restore it, following the
    // precedent in TestMainWindow::audioToolbarTest().
    auto cleanup = qScopeGuard([=]() {
        setConfig().groupManager.npcHide = originalNpcHide;
        setConfig().groupManager.color = originalGroupColor;
    });

    QmlConfig qmlConfig;

    // setProperty()-driven write must go through the QmlConfig setter and
    // land in the live Configuration.
    QSignalSpy npcHideSpy(&qmlConfig, &QmlConfig::npcHideChanged);
    QVERIFY(qmlConfig.setProperty("npcHide", true));
    QCOMPARE(getConfig().groupManager.npcHide, true);
    QCOMPARE(npcHideSpy.count(), 1);

    // An external writer (e.g. the preferences dialog) bypasses QmlConfig's
    // setters entirely, so the façade goes stale until reload() is called.
    setConfig().groupManager.npcHide = false;
    QCOMPARE(qmlConfig.getNpcHide(), false); // getters always read live config
    qmlConfig.reload();
    QCOMPARE(npcHideSpy.count(), 2);
    QCOMPARE(qmlConfig.property("npcHide").toBool(), false);

    // Spot-check the same pattern for groupColor.
    QSignalSpy groupColorSpy(&qmlConfig, &QmlConfig::groupColorChanged);
    const QColor newColor{Qt::green};
    QVERIFY(qmlConfig.setProperty("groupColor", newColor));
    QCOMPARE(getConfig().groupManager.color, newColor);
    QCOMPARE(groupColorSpy.count(), 1);

    const QColor externalColor{Qt::blue};
    setConfig().groupManager.color = externalColor;
    QCOMPARE(qmlConfig.getGroupColor(), externalColor);
    qmlConfig.reload();
    QCOMPARE(groupColorSpy.count(), 2);
    QCOMPARE(qmlConfig.property("groupColor").value<QColor>(), externalColor);
}

void TestQml::loadGroupPanel()
{
    GroupModel model;
    GroupProxyModel proxy;
    proxy.setSourceModel(&model);
    GroupControllerStub controller(nullptr);

    // Seed one character directly on the model, the same fixture pattern
    // TestGroup.cpp's characterRoleDataTest() uses.
    SharedGroupChar character = CGroupChar::alloc();
    character->setId(GroupId{1});
    character->setColor(QColor(Qt::black));
    character->setPosition(CharacterPositionEnum::STANDING);
    character->setScore(/*hp=*/20,
                        /*maxhp=*/100,
                        /*mana=*/0,
                        /*maxmana=*/0,
                        /*moves=*/10,
                        /*maxmoves=*/100);
    model.insertCharacter(character);

    QmlConfig config;

    // Deliberately does not register a "groupicons" image provider: doing so
    // would pull GroupIconProvider.cpp (and, transitively via
    // display/Filenames.cpp's RoomTerrainEnum/RoomLoadFlagEnum/
    // RoomMobFlagEnum overloads, most of the parser/mapdata dependency
    // graph) into this small test binary. The seeded character's
    // "image://groupicons/..." state-icon URLs simply fail to resolve
    // against an unregistered scheme, which QQuickWidget logs and ignores
    // rather than treating as fatal.
    QmlDockWidget dock("t", "TestDockGroup", nullptr);
    dock.setContextProperty("groupModel", &model);
    dock.setContextProperty("groupProxyModel", &proxy);
    dock.setContextProperty("groupController", &controller);
    dock.setContextProperty("config", &config);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/GroupPanel.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    // Let the delegate finish instantiating against the seeded character.
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);
}

void TestQml::loadDescriptionPanel()
{
    MediaLibrary library;
    DescriptionAdapter adapter(library, nullptr);

    // No room yet: the adapter must present the empty state (no image URLs,
    // no text) that DescriptionPanel.qml renders as a bare panel.
    adapter.updateRoom(RoomHandle{});
    QCOMPARE(adapter.getImageUrl(), QUrl());
    QCOMPARE(adapter.getBlurUrl(), QUrl());
    QCOMPARE(adapter.getRoomName(), QString());
    QCOMPARE(adapter.getRoomDesc(), QString());

    QmlDockWidget dock("t", "TestDockDescription", nullptr);
    // Must be registered before setQmlSource(); the engine takes ownership.
    dock.addImageProvider("description", new DescriptionImageProvider(adapter.getStore()));
    dock.setContextProperty("adapter", &adapter);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/DescriptionPanel.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);
}

void TestQml::descriptionPanelBlurVisible()
{
    MediaLibrary library;
    DescriptionAdapter adapter(library, nullptr);

    QmlDockWidget dock("t", "TestDockDescriptionBlur", nullptr);
    // Must be registered before setQmlSource(); the engine takes ownership.
    dock.addImageProvider("description", new DescriptionImageProvider(adapter.getStore()));
    dock.setContextProperty("adapter", &adapter);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/DescriptionPanel.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);

    // A strongly patterned source image (red-dominant field with a blue
    // block) so the blurred backdrop remains distinguishable from a plain
    // window background even after downscale+blur+stretch.
    QImage source(200, 150, QImage::Format_ARGB32_Premultiplied);
    source.fill(Qt::red);
    {
        QPainter painter(&source);
        painter.fillRect(QRect(60, 40, 80, 70), Qt::blue);
    }
    adapter.setImageForTesting(source);

    dock.resize(400, 300);
    dock.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QObject *const blurImage = quick->rootObject()->findChild<QObject *>(
        QStringLiteral("blurImage"));
    QVERIFY(blurImage != nullptr);

    // Image.Ready == 1. Poll with a bounded loop: the image provider request
    // is dispatched asynchronously.
    bool ready = false;
    for (int i = 0; i < 200 && !ready; ++i) {
        QCoreApplication::processEvents();
        ready = blurImage->property("status").toInt() == 1;
        if (!ready) {
            QTest::qWait(5);
        }
    }
    QVERIFY2(ready, "blurImage never reached Image.Ready status");
    QVERIFY(blurImage->property("paintedWidth").toReal() > 0);

    const QPixmap pm = quick->grab();
    QVERIFY(!pm.isNull());
    const QImage grabbed = pm.toImage();
    QCOMPARE(grabbed.width(), 400);
    QCOMPARE(grabbed.height(), 300);

    // Sample the bottom-left corner: guaranteed to be blur-only under the
    // new layout (the text overlay occupies the top-left, and the sharp
    // image is centered/inset rather than full-bleed). PanelFrame insets its
    // content by a 4px margin, so sample a few pixels further in to land
    // inside the blur Image rather than on the frame's own background.
    const QColor sample = grabbed.pixelColor(10, grabbed.height() - 10);
    QVERIFY2(sample.alpha() > 0, "Sampled corner pixel is fully transparent");

    // Reddish: the blurred backdrop is dominated by the red field, so the
    // red channel should clearly exceed the blue channel at this corner.
    QVERIFY2(sample.red() > sample.blue(),
             qPrintable(QStringLiteral("Corner pixel not reddish: rgb(%1,%2,%3)")
                            .arg(sample.red())
                            .arg(sample.green())
                            .arg(sample.blue())));
}

namespace { // anonymous

// Shared tempdir/room fixtures for the "real production image-resolution
// path" tests (descriptionAdapterRealResolution and
// descriptionPanelRealResolutionRenders): a MediaLibrary resourcesDirectory
// with area- and serverId-keyed fixture images on disk, plus RoomHandles set
// up to resolve via each path (and one that resolves via neither, for the
// lastLookupSummary "miss" assertions). RoomHandle keeps the room data alive
// via its own internal Map (COW) handle, so the MapPair used to build them
// doesn't need to be kept alive by the caller.
struct NODISCARD DescriptionRealResolutionFixture final
{
    QTemporaryDir tempDir;
    QString originalResourcesDir;
    // "The Blue Mountains" is the room's RoomArea; DescriptionAdapter::
    // updateRoom lowercases it, strips a leading "the ", replaces spaces
    // with dashes, and ASCII-fies the result, so it must resolve to
    // areas/blue-mountains.<ext>. Fill color is blue, 200x150.
    RoomHandle areaRoom;
    // rooms/<serverId>.<ext> takes priority over the area fallback; uses a
    // deliberately non-matching area so a pass proves the serverId lookup
    // (not the area fallback) resolved the image. Fill color is red, 64x48.
    RoomHandle roomIdRoom;
    // Matches neither rooms/<serverId> nor areas/<areaKey>; used to assert
    // DescriptionAdapter::lastLookupSummary on a lookup miss.
    RoomHandle unknownRoom;

    ~DescriptionRealResolutionFixture()
    {
        setConfig().canvas.resourcesDirectory = originalResourcesDir;
    }
};

NODISCARD std::unique_ptr<DescriptionRealResolutionFixture> makeDescriptionRealResolutionFixture()
{
    auto fixture = std::make_unique<DescriptionRealResolutionFixture>();

    if (!fixture->tempDir.isValid() || !QDir(fixture->tempDir.path()).mkpath("areas")
        || !QDir(fixture->tempDir.path()).mkpath("rooms")) {
        return nullptr;
    }

    const QString areaImagePath = fixture->tempDir.path() + "/areas/blue-mountains.png";
    {
        QImage areaImage(200, 150, QImage::Format_ARGB32_Premultiplied);
        areaImage.fill(Qt::blue);
        if (!areaImage.save(areaImagePath, "PNG")) {
            return nullptr;
        }
    }

    const QString roomImagePath = fixture->tempDir.path() + "/rooms/42.png";
    {
        QImage roomImage(64, 48, QImage::Format_ARGB32_Premultiplied);
        roomImage.fill(Qt::red);
        if (!roomImage.save(roomImagePath, "PNG")) {
            return nullptr;
        }
    }

    // MediaLibrary's constructor scans directories immediately, so the
    // config override must be in place *before* the caller constructs it;
    // TestQml's getAssetsPath() stub returns an empty path (see above), so
    // resourcesDirectory is the only source scanPath() finds anything in.
    fixture->originalResourcesDir = getConfig().canvas.resourcesDirectory;
    setConfig().canvas.resourcesDirectory = fixture->tempDir.path();

    ProgressCounter pc;
    ExternalRawRoom areaRaw;
    areaRaw.setId(ExternalRoomId{1});
    areaRaw.setArea(mmqt::makeRoomArea(QStringLiteral("The Blue Mountains")));
    areaRaw.setName(mmqt::makeRoomName(QStringLiteral("A Mountain Pass")));
    areaRaw.setDescription(mmqt::makeRoomDesc(QStringLiteral("A cold wind blows.")));

    ExternalRawRoom roomIdRaw;
    roomIdRaw.setId(ExternalRoomId{2});
    roomIdRaw.setServerId(ServerRoomId{42});
    roomIdRaw.setArea(mmqt::makeRoomArea(QStringLiteral("Nowhere In Particular")));
    roomIdRaw.setName(mmqt::makeRoomName(QStringLiteral("Room 42")));
    roomIdRaw.setDescription(mmqt::makeRoomDesc(QStringLiteral("Some room.")));

    ExternalRawRoom unknownRaw;
    unknownRaw.setId(ExternalRoomId{3});
    unknownRaw.setArea(mmqt::makeRoomArea(QStringLiteral("Totally Unknown Place")));
    unknownRaw.setName(mmqt::makeRoomName(QStringLiteral("Nowhere")));
    unknownRaw.setDescription(mmqt::makeRoomDesc(QStringLiteral("You are lost.")));

    MapPair mapPair = Map::fromRooms(pc, {areaRaw, roomIdRaw, unknownRaw}, {});
    fixture->areaRoom = mapPair.modified.getRoomHandle(ExternalRoomId{1});
    fixture->roomIdRoom = mapPair.modified.getRoomHandle(ExternalRoomId{2});
    fixture->unknownRoom = mapPair.modified.getRoomHandle(ExternalRoomId{3});

    return fixture;
}

} // namespace

void TestQml::descriptionAdapterRealResolution()
{
    // Exercises the real production image-resolution path end to end:
    // MainWindow::updateDescriptionRoom -> DescriptionAdapter::updateRoom ->
    // MediaLibrary::findImage -> DescriptionAdapter::loadAndCacheImage ->
    // DescriptionImageStore -> DescriptionImageProvider::requestImage. The
    // other Description* tests all go through setImageForTesting(), which
    // bypasses this whole chain, so a regression anywhere in it (e.g. a
    // MediaLibrary scan/key mismatch, or a resourcesDirectory that isn't
    // being picked up) would ship undetected.
    auto fixture = makeDescriptionRealResolutionFixture();
    QVERIFY(fixture != nullptr);
    QVERIFY(fixture->areaRoom);
    QVERIFY(fixture->roomIdRoom);

    MediaLibrary library;
    QCOMPARE(library.numImages(), 2);

    DescriptionAdapter adapter(library, nullptr);
    DescriptionImageProvider provider(adapter.getStore());

    auto requestSharpImage = [&provider](const QUrl &imageUrl, QSize *const size) {
        // imageUrl is "image://description/sharp/<rev>"; requestImage()'s id
        // parameter is everything after "image://description/", i.e. the
        // URL's path with the leading '/' stripped.
        const QString id = imageUrl.path().mid(1);
        return provider.requestImage(id, size, QSize());
    };

    // --- Area-based resolution (no matching serverId) ---
    {
        adapter.updateRoom(fixture->areaRoom);

        const QUrl imageUrl = adapter.getImageUrl();
        QVERIFY(!imageUrl.isEmpty());
        QCOMPARE(imageUrl.toString(), QStringLiteral("image://description/sharp/1"));
        QCOMPARE(adapter.getRoomName(), QStringLiteral("A Mountain Pass"));
        // A hit must clear any previous lookup-miss diagnostic.
        QCOMPARE(adapter.getLastLookupSummary(), QString());

        QSize size;
        const QImage img = requestSharpImage(imageUrl, &size);
        QVERIFY(!img.isNull());
        QCOMPARE(size, QSize(200, 150));
        QCOMPARE(img.size(), QSize(200, 150));
    }

    // --- serverId-based resolution (rooms/<id> takes priority over area) ---
    {
        adapter.updateRoom(fixture->roomIdRoom);

        const QUrl imageUrl = adapter.getImageUrl();
        QVERIFY(!imageUrl.isEmpty());
        QCOMPARE(adapter.getLastLookupSummary(), QString());

        QSize size;
        const QImage img = requestSharpImage(imageUrl, &size);
        QVERIFY(!img.isNull());
        QCOMPARE(size, QSize(64, 48));
        QCOMPARE(img.size(), QSize(64, 48));
    }

    // --- lookup miss: lastLookupSummary must name both keys that were tried ---
    {
        QVERIFY(fixture->unknownRoom);
        adapter.updateRoom(fixture->unknownRoom);

        QVERIFY(adapter.getImageUrl().isEmpty());
        const QString summary = adapter.getLastLookupSummary();
        QVERIFY2(summary.contains(QStringLiteral("rooms/")), qPrintable(summary));
        QVERIFY2(summary.contains(QStringLiteral("areas/")), qPrintable(summary));
    }
}

void TestQml::descriptionPanelRealResolutionRenders()
{
    // Closes the last untested link in the description-image pipeline: the
    // other DescriptionPanel tests (loadDescriptionPanel,
    // descriptionPanelBlurVisible) go through setImageForTesting(), and
    // descriptionAdapterRealResolution exercises real resolution but talks
    // to DescriptionImageProvider::requestImage() directly rather than
    // through an actual QML Image element. This test wires up a REAL
    // MediaLibrary + DescriptionAdapter + REAL DescriptionImageProvider
    // registered on a QmlDockWidget loading DescriptionPanel.qml, so a
    // regression anywhere in QML Image -> image:// URL -> provider dispatch
    // under the software backend -- with a real resolved file, not a
    // synthetic in-memory image -- would be caught here.
    auto fixture = makeDescriptionRealResolutionFixture();
    QVERIFY(fixture != nullptr);
    QVERIFY(fixture->areaRoom);
    QVERIFY(fixture->unknownRoom);

    MediaLibrary library;
    QCOMPARE(library.numImages(), 2);

    DescriptionAdapter adapter(library, nullptr);

    QmlDockWidget dock("t", "TestDockDescriptionRealResolution", nullptr);
    // Must be registered before setQmlSource(); the engine takes ownership.
    dock.addImageProvider("description", new DescriptionImageProvider(adapter.getStore()));
    dock.setContextProperty("adapter", &adapter);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/DescriptionPanel.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();
    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);

    adapter.updateRoom(fixture->areaRoom);
    QCoreApplication::processEvents();

    QObject *const sharpImage = quick->rootObject()->findChild<QObject *>(
        QStringLiteral("sharpImage"));
    QVERIFY(sharpImage != nullptr);

    // Image.Ready == 1. Poll with a bounded loop: the image provider request
    // (which, unlike descriptionPanelBlurVisible's setImageForTesting() path,
    // round-trips through real MediaLibrary file I/O) is dispatched
    // asynchronously.
    bool ready = false;
    for (int i = 0; i < 200 && !ready; ++i) {
        QCoreApplication::processEvents();
        ready = sharpImage->property("status").toInt() == 1;
        if (!ready) {
            QTest::qWait(5);
        }
    }
    QVERIFY2(ready, "sharpImage never reached Image.Ready status");

    dock.resize(400, 300);
    dock.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    const QPixmap pm = quick->grab();
    QVERIFY(!pm.isNull());
    const QImage grabbed = pm.toImage();
    QCOMPARE(grabbed.width(), 400);
    QCOMPARE(grabbed.height(), 300);

    // sharpImage is centered (PreserveAspectFit) over the panel; the fixture
    // image is solid blue, so the panel's exact center must land inside it
    // and read back blue-dominant.
    const QColor sample = grabbed.pixelColor(grabbed.width() / 2, grabbed.height() / 2);
    QVERIFY2(sample.blue() > 100 && sample.blue() > sample.red() + 40
                 && sample.blue() > sample.green() + 40,
             qPrintable(QStringLiteral("Center pixel not blue-dominant: rgb(%1,%2,%3)")
                            .arg(sample.red())
                            .arg(sample.green())
                            .arg(sample.blue())));

    // lastLookupSummary text visibility parity: known room -> empty/hidden;
    // unknown room -> both lookup keys present in the on-screen diagnostic.
    QObject *const lookupSummaryText = quick->rootObject()->findChild<QObject *>(
        QStringLiteral("lookupSummaryText"));
    QVERIFY(lookupSummaryText != nullptr);
    QVERIFY(!lookupSummaryText->property("visible").toBool());
    QCOMPARE(adapter.getLastLookupSummary(), QString());

    adapter.updateRoom(fixture->unknownRoom);
    QCoreApplication::processEvents();

    QVERIFY(lookupSummaryText->property("visible").toBool());
    const QString summaryText = lookupSummaryText->property("text").toString();
    QVERIFY2(summaryText.contains(QStringLiteral("rooms/")), qPrintable(summaryText));
    QVERIFY2(summaryText.contains(QStringLiteral("areas/")), qPrintable(summaryText));
}

void TestQml::tasksModelEmpty()
{
    TasksModel model;
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.getHoldRemovals(), false);
    QCOMPARE(model.data(model.index(0), TasksModel::StatusTextRole), QVariant());
}

void TestQml::tasksModelHoldRemovalsRoundTrip()
{
    TasksModel model;
    QSignalSpy spy(&model, &TasksModel::holdRemovalsChanged);

    QCOMPARE(model.getHoldRemovals(), false);
    model.setHoldRemovals(true);
    QCOMPARE(model.getHoldRemovals(), true);
    QCOMPARE(spy.count(), 1);

    // No-op writes must not emit a spurious notify.
    model.setHoldRemovals(true);
    QCOMPARE(spy.count(), 1);

    model.setHoldRemovals(false);
    QCOMPARE(model.getHoldRemovals(), false);
    QCOMPARE(spy.count(), 2);
}

void TestQml::tasksModelLifecycle()
{
    TasksModel model;
    QCOMPARE(model.rowCount(), 0);

    // Kept running until the test explicitly releases it, so the model has
    // time to observe the task in its "still running" state before it
    // completes and is removed.
    std::atomic_bool allowFinish{false};

    MAYBE_UNUSED const auto handle = async_tasks::startAsyncTask(
        AsyncTaskTypeEnum::Task,
        AllowCancelEnum::Allow,
        "test-task",
        [&allowFinish](ProgressCounter &pc) {
            pc.setNewTask(ProgressMsg{"working"}, 10);
            while (!allowFinish) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        },
        []() {});

    // TasksModel polls the registry every 250ms on its own internal QTimer;
    // QTRY_COMPARE_WITH_TIMEOUT spins the event loop so that timer can fire.
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 1, 5000);

    const QModelIndex idx = model.index(0);
    QVERIFY(model.data(idx, TasksModel::StatusTextRole).toString().contains("Task #"));
    QCOMPARE(model.data(idx, TasksModel::CanCancelRole).toBool(), true);
    QCOMPARE(model.data(idx, TasksModel::PercentRole).toInt(), 0);

    allowFinish = true;

    // Once the background worker returns, AsyncTasks completes it on the
    // next 250ms timer tick and TasksModel's own timer removes the row on
    // its next tick after that.
    QTRY_COMPARE_WITH_TIMEOUT(model.rowCount(), 0, 5000);
}

void TestQml::loadTasksPanel()
{
    // An empty registry is fine here: TasksPanel.qml must render (and show
    // its "No running tasks" empty state) with zero rows.
    TasksModel model;

    QmlDockWidget dock("t", "TestDockTasks", nullptr);
    dock.setContextProperty("tasksModel", &model);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/TasksPanel.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);
}

void TestQml::loadXpStatusItem()
{
    GameObserver observer;
    AdventureTracker tracker(observer, nullptr);
    XpStatusAdapter adapter(tracker, nullptr);

    // These are plain status bar widgets, not dockable panels, so (mirroring
    // MainWindow::setupStatusBar()) load them in a bare QQuickWidget rather
    // than QmlDockWidget.
    QQuickWidget quick;
    quick.rootContext()->setContextProperty("xpStatusAdapter", &adapter);
    quick.setSource(QUrl(u"qrc:/qt/qml/MMapper/XpStatusItem.qml"_qs));

    while (quick.status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick.status(), QQuickWidget::Ready);
    QVERIFY(quick.rootObject() != nullptr);
}

void TestQml::loadClockStrip()
{
    GameObserver observer;
    MumeClock clock(/*mumeEpoch=*/0, observer, nullptr);
    ClockAdapter adapter(observer, clock, nullptr);

    QQuickWidget quick;
    quick.rootContext()->setContextProperty("clock", &adapter);
    quick.setSource(QUrl(u"qrc:/qt/qml/MMapper/ClockStrip.qml"_qs));

    while (quick.status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick.status(), QQuickWidget::Ready);
    QVERIFY(quick.rootObject() != nullptr);
}

void TestQml::clockAdapterNativeToolTip()
{
    // ClockStrip.qml's HoverHandlers call ClockAdapter::showToolTip()/
    // hideToolTip() (see ClockAdapter.h) instead of attached QtQuick.Controls
    // ToolTip properties, because a ToolTip popup rendered inside the tiny,
    // transparent QQuickWidget scene ends up clipped/illegible on macOS.
    // This exercises the invokables directly against a real (offscreen)
    // QApplication, reusing loadClockStrip()'s fixture construction.
    GameObserver observer;
    MumeClock clock(/*mumeEpoch=*/0, observer, nullptr);
    ClockAdapter adapter(observer, clock, nullptr);

    QVERIFY(!QToolTip::isVisible());

    // No-widget fallback: setToolTipWidget() was never called, so
    // showToolTip() must fall back to QCursor::pos() instead of
    // dereferencing a null QPointer. This is the minimum contract the
    // 3-argument invokable must uphold regardless of whether MainWindow
    // wired up a hosting widget.
    adapter.showToolTip(QStringLiteral("no widget"), 5, 5);

    bool visible = false;
    for (int i = 0; i < 100 && !visible; ++i) {
        QCoreApplication::processEvents();
        visible = QToolTip::isVisible();
        if (!visible) {
            QTest::qWait(5);
        }
    }
    if (visible) {
        QCOMPARE(QToolTip::text(), QStringLiteral("no widget"));
    } else {
        qInfo() << "[clockAdapterNativeToolTip] QToolTip never reported visible under the "
                   "offscreen platform; showToolTip()/hideToolTip() did not crash, which is "
                   "the minimum this test can verify headlessly.";
    }

    adapter.hideToolTip();
    QCoreApplication::processEvents();

    // With a hosting widget set, showToolTip()'s x/y scene coordinates must
    // be mapped through that widget's mapToGlobal() rather than falling
    // back to QCursor::pos(). There's no portable way to assert the exact
    // resulting screen point under the offscreen platform, so this only
    // verifies the widget-present path also doesn't crash and (when the
    // popup does report visible) still carries the right text.
    QWidget hostWidget;
    hostWidget.resize(200, 100);
    hostWidget.show();
    adapter.setToolTipWidget(&hostWidget);

    adapter.showToolTip(QStringLiteral("with widget"), 10, 10);

    visible = false;
    for (int i = 0; i < 100 && !visible; ++i) {
        QCoreApplication::processEvents();
        visible = QToolTip::isVisible();
        if (!visible) {
            QTest::qWait(5);
        }
    }
    if (visible) {
        QCOMPARE(QToolTip::text(), QStringLiteral("with widget"));
    } else {
        qInfo() << "[clockAdapterNativeToolTip] QToolTip never reported visible under the "
                   "offscreen platform for the widget-mapped case either; showToolTip() did "
                   "not crash, which is the minimum this test can verify headlessly.";
    }

    adapter.hideToolTip();
    QCoreApplication::processEvents();
}

void TestQml::loadClientDisplay()
{
    ClientLineModel model;
    QmlConfig config;

    // Seed a line with a green-background ANSI run before the QML component
    // is instantiated, mirroring how a real MUD line would already exist by
    // the time the dock is first shown.
    model.appendText(u"\x1b[42m text\n");

    QmlDockWidget dock("t", "TestDockClient", nullptr);
    dock.setContextProperty("clientLineModel", &model);
    dock.setContextProperty("config", &config);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/ClientDisplay.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);

    QObject *const listView = quick->rootObject()->findChild<QObject *>(
        QStringLiteral("clientListView"));
    QVERIFY(listView != nullptr);
    QCOMPARE(listView->property("count").toInt(), 2); // seeded line + trailing partial row.

    dock.resize(300, 200);
    dock.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    const QPixmap pm = quick->grab();
    QVERIFY(!pm.isNull());
    const QImage grabbed = pm.toImage();
    QCOMPARE(grabbed.width(), 300);
    QCOMPARE(grabbed.height(), 200);

    // Scan a small region near the top-left, where the green-background run
    // ("text" prefixed by a space, all styled together) is rendered, for a
    // green-dominant pixel. The background only fills behind the run's text
    // (not the whole line width), and is glyph-independent as long as we
    // land somewhere within the run's box, so scan a small grid rather than
    // relying on one exact pixel.
    bool foundGreen = false;
    for (int y = 2; y < std::min(grabbed.height(), 24) && !foundGreen; ++y) {
        for (int x = 2; x < std::min(grabbed.width(), 60) && !foundGreen; ++x) {
            const QColor c = grabbed.pixelColor(x, y);
            if (c.green() > 100 && c.green() > c.red() + 40 && c.green() > c.blue() + 40) {
                foundGreen = true;
            }
        }
    }

    if (!foundGreen) {
        // Pixel sampling can be flaky depending on font metrics/rendering
        // under the offscreen platform; fall back to asserting on the
        // underlying html role content, which is what actually drives the
        // rendering.
        const QVariant html = model.data(model.index(0, 0),
                                         static_cast<int>(ClientLineModel::RoleEnum::Html));
        QVERIFY2(html.toString().contains(QStringLiteral("background-color:#")),
                 qPrintable(QStringLiteral("No green pixel found and html has no "
                                           "background-color; html was: %1")
                                .arg(html.toString())));
    } else {
        QVERIFY(foundGreen);
    }
}

void TestQml::clientDisplayLeadingWhitespaceRenders()
{
    // Renders two lines that only differ by a leading indent ("XX" vs
    // "  XX") and asserts the indented line's ink starts strictly to the
    // right of the unindented line's, catching a regression in
    // ClientLineModel's Html-role delivery-point wrapping (see
    // ClientLineModel::data()) even if the html role's raw string content
    // stayed superficially correct but QML's RichText subset ignored the
    // white-space override.
    ClientLineModel model;
    QmlConfig config;

    model.appendText(u"XX\n  XX\n");

    QmlDockWidget dock("t", "TestDockClientWhitespace", nullptr);
    dock.setContextProperty("clientLineModel", &model);
    dock.setContextProperty("config", &config);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/ClientDisplay.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);

    QObject *const listView = quick->rootObject()->findChild<QObject *>(
        QStringLiteral("clientListView"));
    QVERIFY(listView != nullptr);
    QCOMPARE(listView->property("count").toInt(), 3); // 2 finished lines + trailing partial row.

    dock.resize(300, 200);
    dock.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    const qreal contentHeight = listView->property("contentHeight").toReal();
    QVERIFY(contentHeight > 0);
    // All three rows use the same font, so they're all the same height;
    // dividing by the row count gives each line's vertical band.
    const qreal rowHeight = contentHeight / 3.0;
    QVERIFY(rowHeight > 1);

    const QPixmap pm = quick->grab();
    QVERIFY(!pm.isNull());
    const QImage grabbed = pm.toImage();
    QCOMPARE(grabbed.width(), 300);
    QCOMPARE(grabbed.height(), 200);

    const QColor bg = getConfig().integratedClient.backgroundColor;
    auto isBackground = [&bg](const QColor &c) {
        return qAbs(c.red() - bg.red()) <= 20 && qAbs(c.green() - bg.green()) <= 20
               && qAbs(c.blue() - bg.blue()) <= 20;
    };
    auto firstInkX = [&](const int bandTop, const int bandBottom) -> int {
        for (int x = 0; x < grabbed.width(); ++x) {
            for (int y = bandTop; y < bandBottom; ++y) {
                if (!isBackground(grabbed.pixelColor(x, y))) {
                    return x;
                }
            }
        }
        return -1;
    };

    const int line1Bottom = std::max(1, static_cast<int>(rowHeight));
    const int line2Bottom = std::min(grabbed.height(), static_cast<int>(rowHeight * 2));

    const int line1Ink = firstInkX(0, line1Bottom);
    const int line2Ink = firstInkX(line1Bottom, line2Bottom);

    QVERIFY2(line1Ink >= 0, "No ink found on line 1 (\"XX\")");
    QVERIFY2(line2Ink >= 0, "No ink found on line 2 (\"  XX\")");
    QVERIFY2(line2Ink > line1Ink,
             qPrintable(QStringLiteral("line2 ink (%1) not strictly right of line1 ink (%2)")
                            .arg(line2Ink)
                            .arg(line1Ink)));
}

void TestQml::clientDisplayStickTracking()
{
    // Exercises ClientDisplay.qml's stick/atEnd tracking. Direct contentY
    // writes from C++ set neither listView.moving nor the scrollbar's
    // pressed state, so under the gesture-gated onContentYChanged they
    // stand in for ListView's own layout-driven contentY adjustments during
    // appends -- which must NOT be able to unpin the view (that transient
    // unpinning is exactly the regression this guards against). User-driven
    // disengage/re-engage is covered via pageUp()/pageDown(), which update
    // stick explicitly (see clientDisplayPageUpDownStick for deeper paging
    // coverage).
    ClientLineModel model;
    QmlConfig config;

    QmlDockWidget dock("t", "TestDockClientStick", nullptr);
    dock.setContextProperty("clientLineModel", &model);
    dock.setContextProperty("config", &config);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/ClientDisplay.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QObject *const rootObject = quick->rootObject();
    QVERIFY(rootObject != nullptr);

    // Force a small viewport so a handful of lines already overflow it.
    dock.resize(300, 60);
    dock.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QObject *const listView = rootObject->findChild<QObject *>(QStringLiteral("clientListView"));
    QVERIFY(listView != nullptr);

    // A burst of synchronous appends outruns the ListView's own incremental
    // delegate layout under the offscreen platform: contentHeight and
    // positionViewAtEnd()'s own effect can both take several event-loop
    // turns to fully settle, so poll (QTRY_VERIFY) rather than asserting
    // synchronously after a fixed number of processEvents() calls.
    for (int i = 0; i < 30; ++i) {
        model.appendText(QStringLiteral("line %1\n").arg(i));
    }
    QTRY_VERIFY_WITH_TIMEOUT(rootObject->property("atEnd").toBool(), 2000);
    QTRY_COMPARE_WITH_TIMEOUT(listView->property("count").toInt(), 31, 2000);
    // contentHeight must have grown past the viewport for the rest of this
    // test (which relies on there being something to scroll) to be
    // meaningful.
    QTRY_VERIFY_WITH_TIMEOUT(listView->property("contentHeight").toReal()
                                 > listView->property("height").toReal(),
                             2000);

    // A direct (non-gesture) contentY write -- the C++ stand-in for
    // ListView's own layout-driven adjustments during appends -- must NOT
    // flip stick off: atEnd stays true even though the view momentarily
    // isn't at the bottom.
    listView->setProperty("contentY", 0.0);
    for (int i = 0; i < 10; ++i) {
        QCoreApplication::processEvents();
    }
    QVERIFY(rootObject->property("atEnd").toBool());

    // ... and because stick survived, the next append re-pins the view to
    // the end instead of leaving it stranded at the top.
    model.appendText(QStringLiteral("late line\n"));
    QTRY_VERIFY_WITH_TIMEOUT(rootObject->property("atEnd").toBool()
                                 && listView->property("atYEnd").toBool(),
                             2000);

    // User-driven disengage: pageUp() sets stick explicitly from the
    // post-move position.
    QVERIFY(QMetaObject::invokeMethod(rootObject, "pageUp"));
    QTRY_VERIFY_WITH_TIMEOUT(!rootObject->property("atEnd").toBool(), 2000);

    // Append-while-unstuck guard: neither contentY nor atEnd may move once
    // the user has scrolled away, even though new rows keep arriving.
    const qreal contentYAfterScroll = listView->property("contentY").toReal();
    model.appendText(QStringLiteral("later line\n"));
    for (int i = 0; i < 10; ++i) {
        QCoreApplication::processEvents();
    }

    QCOMPARE(listView->property("contentY").toReal(), contentYAfterScroll);
    QVERIFY(!rootObject->property("atEnd").toBool());

    // Page back down to the bottom -- re-engages stick, mirroring a manual
    // drag-to-bottom. One pageUp() moved one viewport height, but content
    // grew ("later line") and estimated contentHeight can also refine as
    // delegates instantiate, so allow a couple of (clamped) pages down.
    bool reEngaged = false;
    for (int i = 0; i < 5 && !reEngaged; ++i) {
        QVERIFY(QMetaObject::invokeMethod(rootObject, "pageDown"));
        QCoreApplication::processEvents();
        reEngaged = rootObject->property("atEnd").toBool();
    }
    QVERIFY2(reEngaged, "pageDown() never re-engaged stick at the bottom");

    // Appending while stuck must re-pin the view to the (new) end.
    model.appendText(QStringLiteral("final line\n"));
    QTRY_VERIFY_WITH_TIMEOUT(rootObject->property("atEnd").toBool()
                                 && listView->property("atYEnd").toBool(),
                             2000);
}

void TestQml::clientDisplayBurstAppendStaysPinned()
{
    // Regression test for the reported macOS bug: a burst of appends makes
    // ListView emit layout-driven contentY adjustments (contentHeight
    // estimate refinement, dataChanged on the trailing partial row) during
    // which atYEnd is momentarily false. Deriving stick from every contentY
    // change let those transients unpin the view mid-burst, so live output
    // silently stopped autoscrolling; with the gesture-gated
    // onContentYChanged the view must stay pinned throughout.
    ClientLineModel model;
    QmlConfig config;

    QmlDockWidget dock("t", "TestDockClientBurst", nullptr);
    dock.setContextProperty("clientLineModel", &model);
    dock.setContextProperty("config", &config);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/ClientDisplay.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QObject *const rootObject = quick->rootObject();
    QVERIFY(rootObject != nullptr);

    // Force a small viewport so a handful of lines already overflow it.
    dock.resize(300, 60);
    dock.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QObject *const listView = rootObject->findChild<QObject *>(QStringLiteral("clientListView"));
    QVERIFY(listView != nullptr);

    // Repeated bursts, with event processing interleaved so the layout
    // transients of one burst are live while the next arrives (the shape of
    // the reported failure). atEnd must hold after every burst settles.
    for (int burst = 0; burst < 5; ++burst) {
        for (int i = 0; i < 20; ++i) {
            model.appendText(QStringLiteral("burst %1 line %2\n").arg(burst).arg(i));
        }
        QTRY_VERIFY_WITH_TIMEOUT(rootObject->property("atEnd").toBool(), 2000);
        for (int i = 0; i < 5; ++i) {
            QCoreApplication::processEvents();
        }
        QVERIFY(rootObject->property("atEnd").toBool());
    }

    QTRY_COMPARE_WITH_TIMEOUT(listView->property("count").toInt(), 101, 2000);
    // Not just the stick flag: the view must actually be following the
    // bottom once everything settles.
    QTRY_VERIFY_WITH_TIMEOUT(rootObject->property("atEnd").toBool()
                                 && listView->property("atYEnd").toBool(),
                             2000);
}

void TestQml::clientDisplayPageUpDownStick()
{
    // pageUp()/pageDown() set contentY directly, which never sets
    // listView.moving or the scrollbar's pressed state, so they can't rely
    // on onContentYChanged's user-gesture gate; each must update stick
    // explicitly. Paging up disengages autoscroll, paging back down to the
    // bottom re-engages it.
    ClientLineModel model;
    QmlConfig config;

    QmlDockWidget dock("t", "TestDockClientPaging", nullptr);
    dock.setContextProperty("clientLineModel", &model);
    dock.setContextProperty("config", &config);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/ClientDisplay.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QObject *const rootObject = quick->rootObject();
    QVERIFY(rootObject != nullptr);

    dock.resize(300, 60);
    dock.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    QObject *const listView = rootObject->findChild<QObject *>(QStringLiteral("clientListView"));
    QVERIFY(listView != nullptr);

    for (int i = 0; i < 30; ++i) {
        model.appendText(QStringLiteral("line %1\n").arg(i));
    }
    QTRY_VERIFY_WITH_TIMEOUT(rootObject->property("atEnd").toBool(), 2000);
    QTRY_VERIFY_WITH_TIMEOUT(listView->property("contentHeight").toReal()
                                 > listView->property("height").toReal(),
                             2000);

    // Scroll well away from the end: several pages up.
    for (int i = 0; i < 3; ++i) {
        QVERIFY(QMetaObject::invokeMethod(rootObject, "pageUp"));
        QCoreApplication::processEvents();
    }
    QVERIFY(!rootObject->property("atEnd").toBool());

    // Page back down until the bottom re-engages stick; each call moves one
    // viewport height (clamped to the end), so a bounded number of calls
    // must get there.
    bool reEngaged = false;
    for (int i = 0; i < 10 && !reEngaged; ++i) {
        QVERIFY(QMetaObject::invokeMethod(rootObject, "pageDown"));
        QCoreApplication::processEvents();
        reEngaged = rootObject->property("atEnd").toBool();
    }
    QVERIFY2(reEngaged, "pageDown() never re-engaged stick at the bottom");
    QTRY_VERIFY_WITH_TIMEOUT(listView->property("atYEnd").toBool(), 2000);

    // Re-engaged stick means new output follows again.
    model.appendText(QStringLiteral("post-paging line\n"));
    QTRY_VERIFY_WITH_TIMEOUT(rootObject->property("atEnd").toBool()
                                 && listView->property("atYEnd").toBool(),
                             2000);
}

void TestQml::loadClientPanel()
{
    ClientLineModel model;
    HotkeyManager hotkeys;
    hotkeys.resetToDefaults();
    ClientController controller(model, hotkeys, nullptr);
    auto backend = std::make_unique<FakeBackend>();
    FakeBackend &fake = *backend;
    fake.connected = true;
    controller.setBackend(std::move(backend));
    QmlConfig config;

    QmlDockWidget dock("t", "TestDockClientPanel", nullptr);
    dock.setContextProperty("clientController", &controller);
    dock.setContextProperty("clientLineModel", &model);
    dock.setContextProperty("config", &config);
    dock.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/ClientPanel.qml"_qs));

    QQuickWidget *const quick = dock.quickWidget();
    QVERIFY(quick != nullptr);

    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);

    // Before play(), the welcome page is showing and there is no input area
    // (or it is at least not the visible one). play() flips usingClient,
    // which the QML binds the SplitView.visible / TextArea.visible off of.
    controller.play();
    QCoreApplication::processEvents();
    QVERIFY(controller.getUsingClient());

    QObject *const inputArea = quick->rootObject()->findChild<QObject *>(
        QStringLiteral("clientInputArea"));
    QVERIFY(inputArea != nullptr);
    QVERIFY(inputArea->property("visible").toBool());

    QObject *const passwordField = quick->rootObject()->findChild<QObject *>(
        QStringLiteral("clientPasswordField"));
    QVERIFY(passwordField != nullptr);
    QVERIFY(!passwordField->property("visible").toBool());

    // Toggling echo mode (as if a password prompt arrived) must swap which
    // of the two input surfaces is visible.
    controller.onEchoModeChanged(false);
    QCoreApplication::processEvents();
    QVERIFY(passwordField->property("visible").toBool());
    QVERIFY(!inputArea->property("visible").toBool());

    // Flip echo mode back on so the input area is the live one again for the
    // key-delivery smoke test below.
    controller.onEchoModeChanged(true);
    QCoreApplication::processEvents();

    // Text arriving from the "MUD" must grow the shared ClientLineModel
    // (and therefore be visible through both the display and the panel).
    const int rowsBefore = model.rowCount();
    controller.onSendToUser(QStringLiteral("hello\n"));
    QCoreApplication::processEvents();
    QVERIFY(model.rowCount() > rowsBefore);

    // Key-delivery smoke test: focus the input area and send a real F1 key
    // press through the QQuickWidget's event pipeline (not a direct
    // Q_INVOKABLE call), exercising Keys.onPressed's
    // clientController.sendHotkey() path end-to-end. F1 is a default hotkey
    // (see HotkeyManager.h's XFOREACH_DEFAULT_HOTKEYS) and, unlike letter
    // keys, isn't claimed by any QAction/shortcut in this minimal test dock,
    // so plain QTest::keyClick() delivery (no ShortcutOverride needed) is
    // sufficient to reach Keys.onPressed.
    dock.resize(300, 200);
    dock.show();
    QCoreApplication::processEvents();
    QMetaObject::invokeMethod(inputArea, "forceActiveFocus");
    QCoreApplication::processEvents();

    QTest::keyClick(quick, Qt::Key_F1);
    QCoreApplication::processEvents();

    QVERIFY2(fake.sentToMud.contains(QStringLiteral("F1\n")),
             qPrintable(QStringLiteral("sentToMud did not contain \"F1\\n\"; had: %1")
                            .arg(fake.sentToMud.join(", "))));
}

void TestQml::qmlDialogLoads()
{
    // setQmlSource() only takes a QUrl (no setData()-style inline-string
    // overload), so write a tiny fixture with explicit implicit sizes to a
    // temp file and load it via a file:// URL, exercising the same
    // statusChanged-driven resize path production QML sources go through.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("QmlDialogTest.qml"));
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArrayLiteral("import QtQuick\n"
                                     "Item {\n"
                                     "    implicitWidth: 500\n"
                                     "    implicitHeight: 400\n"
                                     "}\n"));
    }

    QmlDialog dialog("t", "TestDialog", nullptr);
    QVERIFY(dialog.quickWidget() != nullptr);
    dialog.setQmlSource(QUrl::fromLocalFile(path));

    QQuickWidget *const quick = dialog.quickWidget();
    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCOMPARE(quick->status(), QQuickWidget::Ready);
    // Give the queued statusChanged connection a chance to resize the dialog.
    QCoreApplication::processEvents();

    QVERIFY(dialog.width() >= 500);
    QVERIFY(dialog.height() >= 400);
}

void TestQml::qmlDialogFallback()
{
    QmlDialog dialog("t", "TestDialogFallback", nullptr);
    QVERIFY(dialog.quickWidget() != nullptr);

    dialog.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/DoesNotExist.qml"_qs));

    while (dialog.quickWidget() != nullptr
           && dialog.quickWidget()->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    // Give the queued statusChanged connection a chance to run.
    QCoreApplication::processEvents();

    if (dialog.quickWidget() != nullptr) {
        QCOMPARE(dialog.quickWidget()->status(), QQuickWidget::Error);
    } else {
        auto *const label = dialog.findChild<QLabel *>();
        QVERIFY(label != nullptr);
    }
}

void TestQml::qmlDialogRejectFromQml()
{
    QmlDialog dialog("t", "TestDialogReject", nullptr);
    dialog.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/PanelFrame.qml"_qs));

    QQuickWidget *const quick = dialog.quickWidget();
    QVERIFY(quick != nullptr);
    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCOMPARE(quick->status(), QQuickWidget::Ready);

    QSignalSpy rejectedSpy(&dialog, &QDialog::rejected);
    QSignalSpy finishedSpy(&dialog, &QDialog::finished);

    // Exercise the "dialog" context property exposed by the constructor:
    // call reject() the same way QML button handlers would (dialog.reject()).
    QObject *const dialogProperty = qvariant_cast<QObject *>(
        quick->rootContext()->contextProperty("dialog"));
    QVERIFY(dialogProperty != nullptr);
    QMetaObject::invokeMethod(dialogProperty, "reject");
    QCoreApplication::processEvents();

    QCOMPARE(rejectedSpy.count(), 1);
    QCOMPARE(finishedSpy.count(), 1);
}

void TestQml::loadAboutDialog()
{
    AboutInfo info(nullptr);
    QVERIFY(info.getLicenses() != nullptr);
    QVERIFY(info.getLicenses()->rowCount() > 0);

    QmlDialog dialog("t", "TestAboutDialog", nullptr);
    dialog.setContextProperty("aboutInfo", &info);
    dialog.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/AboutDialog.qml"_qs));

    QQuickWidget *const quick = dialog.quickWidget();
    QVERIFY(quick != nullptr);
    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);
}

void TestQml::loadUpdateDialog()
{
    // No network call is made here: UpdateChecker::check() is never invoked,
    // so this only exercises loading the QML against the checker's initial
    // (pre-check) property values.
    UpdateChecker checker(nullptr);

    QmlDialog dialog("t", "TestUpdateDialog", nullptr);
    dialog.setContextProperty("updateChecker", &checker);
    dialog.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/UpdateDialog.qml"_qs));

    QQuickWidget *const quick = dialog.quickWidget();
    QVERIFY(quick != nullptr);
    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);
}

void TestQml::loadInfomarkEditDialog()
{
    // InfomarkEditControllerStub starts at currentIndex 0 ("Create New
    // Marker") with type 0 (TEXT), matching InfomarkEditController's state
    // right after setInfomarkSelection() populates an empty-selection
    // "create" dialog (see infomarkseditdlg.cpp's updateDialog()).
    InfomarkEditControllerStub controller(nullptr);

    QmlDialog dialog("t", "TestInfomarkEditDialog", nullptr);
    dialog.setContextProperty("infomarkEditController", &controller);
    dialog.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/InfomarkEditDialog.qml"_qs));

    QQuickWidget *const quick = dialog.quickWidget();
    QVERIFY(quick != nullptr);
    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QQuickItem *const root = quick->rootObject();
    QVERIFY(root != nullptr);

    QQuickItem *const modifyButton = root->findChild<QQuickItem *>("modifyButton");
    QVERIFY(modifyButton != nullptr);
    QCOMPARE(modifyButton->property("enabled").toBool(), false);

    QQuickItem *const createButton = root->findChild<QQuickItem *>("createButton");
    QVERIFY(createButton != nullptr);
    QCOMPARE(createButton->property("enabled").toBool(), true);

    QQuickItem *const x2Box = root->findChild<QQuickItem *>("x2Box");
    QQuickItem *const y2Box = root->findChild<QQuickItem *>("y2Box");
    QVERIFY(x2Box != nullptr);
    QVERIFY(y2Box != nullptr);
    // type 0 == InfomarkTypeEnum::TEXT: X2/Y2 disabled.
    QCOMPARE(x2Box->property("enabled").toBool(), false);
    QCOMPARE(y2Box->property("enabled").toBool(), false);

    controller.setType(1); // InfomarkTypeEnum::LINE
    QCoreApplication::processEvents();
    QCOMPARE(x2Box->property("enabled").toBool(), true);
    QCOMPARE(y2Box->property("enabled").toBool(), true);

    // Selecting an existing marker (index 1) flips canModify, enabling
    // Modify and disabling Create, mirroring InfomarksEditDlg::updateDialog().
    controller.setCurrentIndex(1);
    QCoreApplication::processEvents();
    QCOMPARE(modifyButton->property("enabled").toBool(), true);
    QCOMPARE(createButton->property("enabled").toBool(), false);
}

void TestQml::loadRoomEditDialog()
{
    RoomEditControllerStub controller(nullptr);

    QmlDialog dialog("t", "TestRoomEditDialog", nullptr);
    dialog.setContextProperty("roomEditController", &controller);
    dialog.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/RoomEditDialog.qml"_qs));

    QQuickWidget *const quick = dialog.quickWidget();
    QVERIFY(quick != nullptr);
    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QQuickItem *const root = quick->rootObject();
    QVERIFY(root != nullptr);

    QQuickItem *const closeButton = root->findChild<QQuickItem *>("closeButton");
    QVERIFY(closeButton != nullptr);
    QCOMPARE(closeButton->property("enabled").toBool(), true);

    // Dirtying the note (as if the user typed in the Note tab) must disable
    // Close, mirroring roomeditattrdlg.cpp's setRoomNoteDirty(); reverting
    // clears it again.
    controller.setNoteText(QStringLiteral("a pending edit"));
    QCoreApplication::processEvents();
    QTRY_COMPARE(closeButton->property("enabled").toBool(), false);

    controller.revertNote();
    QCoreApplication::processEvents();
    QTRY_COMPARE(closeButton->property("enabled").toBool(), true);

    // Tri-state rendering: mobFlagsListView's row 1 (PartiallyChecked, see
    // RoomEditControllerStub's constructor) must reach a CheckBox delegate
    // with checkState == 1 (Qt::PartiallyChecked).
    QQuickItem *const mobFlagsListView = root->findChild<QQuickItem *>("mobFlagsListView");
    QVERIFY(mobFlagsListView != nullptr);
    QQuickItem *rowItem = nullptr;
    QMetaObject::invokeMethod(mobFlagsListView,
                              "itemAtIndex",
                              Q_RETURN_ARG(QQuickItem *, rowItem),
                              Q_ARG(int, 1));
    QVERIFY(rowItem != nullptr);

    // Search the delegate row's subtree (Row -> CheckBox) for a CheckBox
    // whose checkState is PartiallyChecked (1); the delegate has exactly
    // one.
    bool foundPartial = false;
    for (QQuickItem *const candidate : rowItem->findChildren<QQuickItem *>()) {
        if (candidate->property("checkState").isValid()
            && candidate->property("checkState").toInt() == 1) {
            foundPartial = true;
            break;
        }
    }
    QVERIFY2(foundPartial, "expected a PartiallyChecked (checkState==1) CheckBox delegate");

    // Exit-direction click must call the controller's selectedExitDir
    // setter; the D-pad's South button (objectName-less, so locate it by
    // text) is exercised via the exit grid.
    QCOMPARE(controller.getSelectedExitDir(), 0); // NORTH, matching the widget's default.
    QQuickItem *const exitsFrame = root->findChild<QQuickItem *>("exitsFrame");
    QVERIFY(exitsFrame != nullptr);
    QQuickItem *southButton = nullptr;
    for (QQuickItem *const candidate : exitsFrame->findChildren<QQuickItem *>()) {
        if (candidate->property("text").toString() == QStringLiteral("S")) {
            southButton = candidate;
            break;
        }
    }
    QVERIFY(southButton != nullptr);
    QMetaObject::invokeMethod(southButton, "clicked");
    QCoreApplication::processEvents();
    QTRY_COMPARE(controller.getSelectedExitDir(), 1); // SOUTH
}

void TestQml::qmlDialogBackgroundThemed()
{
    // Regression test for the "dialogs look off with a white background"
    // report: every QML dialog root used to be a bare Item, so the hosting
    // QQuickWidget's default white clear color showed through regardless of
    // the system palette. Both QmlDialog's clear color (set from the host
    // palette in the constructor) and the dialog QML's own Rectangle root
    // (see UpdateDialog.qml's "color: sysPalette.window") must now agree
    // with QPalette::Window, so a corner pixel sampled from the rendered
    // widget should exactly match it. UpdateDialog.qml is the smallest of
    // the four dialogs, mirroring loadUpdateDialog()'s fixture.
    UpdateChecker checker(nullptr);

    QmlDialog dialog("t", "TestDialogBackgroundThemed", nullptr);
    dialog.setContextProperty("updateChecker", &checker);
    dialog.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/UpdateDialog.qml"_qs));

    QQuickWidget *const quick = dialog.quickWidget();
    QVERIFY(quick != nullptr);
    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();
    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);

    dialog.resize(420, 140);
    dialog.show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    const QPixmap pm = quick->grab();
    QVERIFY(!pm.isNull());
    const QImage grabbed = pm.toImage();
    QVERIFY(grabbed.width() > 0);
    QVERIFY(grabbed.height() > 0);

    const QColor expected = dialog.palette().color(QPalette::Window);
    const QColor sample = grabbed.pixelColor(0, 0);

    // Exact match should hold under the software backend; allow a small
    // per-channel tolerance in case antialiasing/color-management ever
    // nudges the corner pixel slightly.
    constexpr int TOLERANCE = 2;
    QVERIFY2(std::abs(sample.red() - expected.red()) <= TOLERANCE
                 && std::abs(sample.green() - expected.green()) <= TOLERANCE
                 && std::abs(sample.blue() - expected.blue()) <= TOLERANCE,
             qPrintable(QStringLiteral("Corner pixel rgb(%1,%2,%3) does not match "
                                       "QPalette::Window rgb(%4,%5,%6)")
                            .arg(sample.red())
                            .arg(sample.green())
                            .arg(sample.blue())
                            .arg(expected.red())
                            .arg(expected.green())
                            .arg(expected.blue())));
}

void TestQml::findRoomsModelBasics()
{
    // FindRoomsController needs a live MapData (mapfrontend/parser/mapstorage),
    // which no test binary currently links against TestQml (see
    // GroupControllerStub's comment above for the same tradeoff with
    // GroupController); loadFindRoomsDialog() below covers the QML-facing
    // surface with a stub controller instead. This test exercises the model
    // in isolation, the same way roomModelLongestTextInColumn() covers
    // RoomModel without a live RoomManager feed.
    FindRoomsModel model(nullptr);
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(!model.isValidRow(0));
    QCOMPARE(model.externalIdAt(0), 0u);
    QCOMPARE(model.toolTipAt(0), QString());

    std::vector<FindRoomsModel::Entry> entries;
    entries.push_back(FindRoomsModel::Entry{1u, QStringLiteral("Room One"), QStringLiteral("tip1")});
    entries.push_back(FindRoomsModel::Entry{2u, QStringLiteral("Room Two"), QStringLiteral("tip2")});

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    model.setRooms(entries);
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(model.isValidRow(0));
    QVERIFY(model.isValidRow(1));
    QVERIFY(!model.isValidRow(2));

    const QModelIndex idx0 = model.index(0, 0);
    QCOMPARE(model.data(idx0, FindRoomsModel::ExternalIdRole).toUInt(), 1u);
    QCOMPARE(model.data(idx0, FindRoomsModel::NameRole).toString(), QStringLiteral("Room One"));
    QCOMPARE(model.data(idx0, FindRoomsModel::ToolTipRole).toString(), QStringLiteral("tip1"));
    QCOMPARE(model.externalIdAt(1), 2u);
    QCOMPARE(model.toolTipAt(1), QStringLiteral("tip2"));

    const auto roles = model.roleNames();
    QCOMPARE(roles.value(FindRoomsModel::ExternalIdRole), QByteArray("externalId"));
    QCOMPARE(roles.value(FindRoomsModel::NameRole), QByteArray("name"));
    QCOMPARE(roles.value(FindRoomsModel::ToolTipRole), QByteArray("toolTip"));

    model.clear();
    QCOMPARE(model.rowCount(), 0);
    // Clearing an already-empty model must not emit a spurious reset.
    resetSpy.clear();
    model.clear();
    QCOMPARE(resetSpy.count(), 0);
}

void TestQml::loadPreferencesDialog()
{
    // dialogParent may legitimately be nullptr here: the shell never opens
    // a native picker during this test (chooseColor()/browseForEditor()/
    // browseForDirectory() are never invoked), so PreferencesController's
    // QPointer<QWidget> fields simply stay null.
    PreferencesController controller(nullptr, nullptr);

    QmlDialog dialog("t", "TestPreferencesDialog", nullptr);
    dialog.setContextProperty("preferencesController", &controller);
    dialog.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/PreferencesDialog.qml"_qs));

    QQuickWidget *const quick = dialog.quickWidget();
    QVERIFY(quick != nullptr);
    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QQuickItem *const rootItem = quick->rootObject();
    QVERIFY(rootItem != nullptr);

    auto *const navList = rootItem->findChild<QObject *>(QStringLiteral("preferencesNavList"));
    QVERIFY(navList != nullptr);
    auto *const flickable = rootItem->findChild<QQuickItem *>(
        QStringLiteral("preferencesFlickable"));
    QVERIFY(flickable != nullptr);

    // All nine pages instantiate at once inside the content column (each
    // wrapped in a Section header container), so simply reaching Ready above
    // already exercised every page adapter; confirm the section count.
    const auto sections = rootItem->findChildren<QQuickItem *>(QStringLiteral("preferencesSection"));
    QCOMPARE(sections.size(), 9);

    // Give the dialog real geometry so the flickable has a viewport to
    // scroll (the content column is far taller than 600px with all nine
    // pages stacked).
    dialog.resize(800, 600);
    dialog.show();
    QCoreApplication::processEvents();
    QTRY_VERIFY(flickable->height() > 0);
    QTRY_VERIFY(flickable->property("contentHeight").toReal() > flickable->height());

    // Nav click scrolls the flickable to the chosen section and keeps the
    // nav selection there (the programmatic-scroll guard suppresses the
    // scrollspy, like the widget's QSignalBlocker).
    QCOMPARE(navList->property("currentIndex").toInt(), 0);
    QVERIFY(QMetaObject::invokeMethod(rootItem, "scrollToSection", Q_ARG(QVariant, 8)));
    QCoreApplication::processEvents();
    QVERIFY(flickable->property("contentY").toReal() > 0);
    QCOMPARE(navList->property("currentIndex").toInt(), 8);

    // Scrollspy: scrolling back to the top must move the nav selection back
    // to the first section (slot_onScroll's "last section whose top is at or
    // above the viewport top" rule).
    flickable->setProperty("contentY", 0.0);
    QCoreApplication::processEvents();
    QTRY_COMPARE(navList->property("currentIndex").toInt(), 0);

    // And scrolling to the very bottom must clamp the selection to the last
    // section even if its header is above the viewport top.
    const qreal maxY = flickable->property("contentHeight").toReal() - flickable->height();
    flickable->setProperty("contentY", maxY);
    QCoreApplication::processEvents();
    QTRY_COMPARE(navList->property("currentIndex").toInt(), 8);
}

void TestQml::preferencesDialogSearch()
{
    // dialogParent may legitimately be nullptr here: no native picker is
    // opened during this test (see loadPreferencesDialog() above).
    PreferencesController controller(nullptr, nullptr);

    QmlDialog dialog("t", "TestPreferencesDialogSearch", nullptr);
    dialog.setContextProperty("preferencesController", &controller);
    dialog.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/PreferencesDialog.qml"_qs));

    QQuickWidget *const quick = dialog.quickWidget();
    QVERIFY(quick != nullptr);
    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QQuickItem *const rootItem = quick->rootObject();
    QVERIFY(rootItem != nullptr);

    dialog.resize(800, 600);
    dialog.show();
    QCoreApplication::processEvents();

    auto *const searchField = rootItem->findChild<QObject *>(
        QStringLiteral("preferencesSearchField"));
    QVERIFY(searchField != nullptr);
    auto *const flickable = rootItem->findChild<QQuickItem *>(
        QStringLiteral("preferencesFlickable"));
    QVERIFY(flickable != nullptr);
    auto *const resultsList = rootItem->findChild<QQuickItem *>(
        QStringLiteral("preferencesSearchResults"));
    QVERIFY(resultsList != nullptr);
    auto *const noResultsLabel = rootItem->findChild<QQuickItem *>(
        QStringLiteral("preferencesNoResultsLabel"));
    QVERIFY(noResultsLabel != nullptr);
    auto *const navList = rootItem->findChild<QObject *>(QStringLiteral("preferencesNavList"));
    QVERIFY(navList != nullptr);
    QTRY_VERIFY(flickable->height() > 0);

    // "Audible bell" appears only on the Integrated Client page, so past the
    // 50ms debounce the results list must hold exactly two rows: the bold
    // "Integrated Client" page header plus the matching checkbox text.
    searchField->setProperty("text", QStringLiteral("Audible bell"));
    QTRY_VERIFY(rootItem->property("searchActive").toBool());
    QTRY_COMPARE(resultsList->property("count").toInt(), 2);
    QVERIFY(resultsList->isVisible());
    QVERIFY(!flickable->isVisible());

    // Non-matching pages' nav entries are disabled while the search is
    // active (slot_search clearing Qt::ItemIsEnabled); only index 3
    // (Integrated Client) stays enabled.
    const QVariantList navEnabled = rootItem->property("navEnabled").toList();
    QCOMPARE(navEnabled.size(), 9);
    for (int i = 0; i < 9; ++i) {
        QCOMPARE(navEnabled.at(i).toBool(), i == 3);
    }

    // Activating the second row (the "Audible bell" checkbox itself) clears
    // the search, restores the page column, scrolls to the control, and
    // moves the nav selection to its page.
    QVERIFY(QMetaObject::invokeMethod(rootItem, "activateResultAt", Q_ARG(QVariant, 1)));
    QCoreApplication::processEvents();
    QVERIFY(!rootItem->property("searchActive").toBool());
    QCOMPARE(searchField->property("text").toString(), QString());
    QTRY_VERIFY(flickable->isVisible());
    QVERIFY(!resultsList->isVisible());
    QVERIFY(flickable->property("contentY").toReal() > 0);
    QCOMPARE(navList->property("currentIndex").toInt(), 3);
    QCOMPARE(rootItem->property("navEnabled").toList().size(), 0);

    // A search with no matches swaps in the "no matches" placeholder.
    searchField->setProperty("text", QStringLiteral("zzz-no-such-setting"));
    QTRY_VERIFY(rootItem->property("searchActive").toBool());
    QTRY_COMPARE(resultsList->property("count").toInt(), 0);
    QVERIFY(noResultsLabel->isVisible());
    QVERIFY(!resultsList->isVisible());
    QVERIFY(!flickable->isVisible());
}

void TestQml::checkableFlagModelBasics()
{
    CheckableFlagModel model(nullptr);
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(model.rowAt(0) == nullptr);

    const auto roles = model.roleNames();
    QCOMPARE(roles.value(CheckableFlagModel::NameRole), QByteArray("name"));
    QCOMPARE(roles.value(CheckableFlagModel::IconSourceRole), QByteArray("iconSource"));
    QCOMPARE(roles.value(CheckableFlagModel::CheckStateRole), QByteArray("checkState"));
    QCOMPARE(roles.value(CheckableFlagModel::CheckableRole), QByteArray("checkable"));

    std::vector<CheckableFlagModel::Row> rows;
    rows.push_back(CheckableFlagModel::Row{1,
                                           QStringLiteral("Attack"),
                                           CheckableFlagModel::iconUrl(":/icons/attack.png"),
                                           Qt::Unchecked,
                                           true});
    rows.push_back(CheckableFlagModel::Row{2, QStringLiteral("Guard"), QUrl(), Qt::Checked, true});

    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    model.setRows(rows);
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(model.rowCount(), 2);

    const QModelIndex idx0 = model.index(0, 0);
    QCOMPARE(model.data(idx0, CheckableFlagModel::NameRole).toString(), QStringLiteral("Attack"));
    QCOMPARE(model.data(idx0, CheckableFlagModel::IconSourceRole).toUrl(),
             QUrl(QStringLiteral("qrc:/icons/attack.png")));
    QCOMPARE(model.data(idx0, CheckableFlagModel::CheckStateRole).toInt(),
             static_cast<int>(Qt::Unchecked));
    QCOMPARE(model.data(idx0, CheckableFlagModel::CheckableRole).toBool(), true);

    const auto *const row0 = model.rowAt(0);
    QVERIFY(row0 != nullptr);
    QCOMPARE(row0->flagValue, 1);

    // setState() on a known flag emits a granular dataChanged(), not a reset.
    QSignalSpy dataChangedSpy(&model, &QAbstractItemModel::dataChanged);
    model.setState(1, Qt::PartiallyChecked);
    QCOMPARE(dataChangedSpy.count(), 1);
    QCOMPARE(resetSpy.count(), 1);
    {
        const QList<QVariant> args = dataChangedSpy.takeFirst();
        QCOMPARE(args.at(0).toModelIndex(), model.index(0));
        QCOMPARE(args.at(1).toModelIndex(), model.index(0));
    }
    QCOMPARE(model.data(idx0, CheckableFlagModel::CheckStateRole).toInt(),
             static_cast<int>(Qt::PartiallyChecked));

    // Redundant state is a no-op: no signal, no change.
    model.setState(1, Qt::PartiallyChecked);
    QCOMPARE(dataChangedSpy.count(), 0);

    // Unknown flagValue is a no-op.
    model.setState(999, Qt::Checked);
    QCOMPARE(dataChangedSpy.count(), 0);

    // setCheckable() likewise emits a granular dataChanged().
    model.setCheckable(2, false);
    QCOMPARE(dataChangedSpy.count(), 1);
    dataChangedSpy.clear();
    QCOMPARE(model.data(model.index(1), CheckableFlagModel::CheckableRole).toBool(), false);
    model.setCheckable(999, false);
    QCOMPARE(dataChangedSpy.count(), 0);

    // setAllStates() spans every row in a single dataChanged().
    model.setAllStates(Qt::Checked);
    QCOMPARE(dataChangedSpy.count(), 1);
    {
        const QList<QVariant> args = dataChangedSpy.takeFirst();
        QCOMPARE(args.at(0).toModelIndex(), model.index(0));
        QCOMPARE(args.at(1).toModelIndex(), model.index(1));
    }
    QCOMPARE(model.data(model.index(0), CheckableFlagModel::CheckStateRole).toInt(),
             static_cast<int>(Qt::Checked));
    QCOMPARE(model.data(model.index(1), CheckableFlagModel::CheckStateRole).toInt(),
             static_cast<int>(Qt::Checked));

    // iconUrl(): qrc-style, absolute local path, and empty.
    QCOMPARE(CheckableFlagModel::iconUrl(QStringLiteral(":/a/b.png")),
             QUrl(QStringLiteral("qrc:/a/b.png")));
    QCOMPARE(CheckableFlagModel::iconUrl(QStringLiteral("/tmp/icon.png")),
             QUrl::fromLocalFile(QStringLiteral("/tmp/icon.png")));
    QCOMPARE(CheckableFlagModel::iconUrl(QString()), QUrl());
}

void TestQml::ansiToHtmlBasics()
{
    // No QTextEdit/QTextBrowser (QtWidgets) is involved anywhere in this path;
    // ansiToHtml() renders into a bare QTextDocument (QtGui).
    const QString ansiText = QStringLiteral(
        "\x1b[31mred\x1b[0m plain \x1b[1mbold\x1b[0m\nnext line");
    const QString html = mmqt::ansiToHtml(ansiText, QColor(Qt::lightGray), QColor(Qt::black));

    QVERIFY(html.contains(QStringLiteral("red")));
    QVERIFY(html.contains(QStringLiteral("plain")));
    QVERIFY(html.contains(QStringLiteral("bold")));
    QVERIFY(html.contains(QStringLiteral("next line")));

    // Red foreground shows up as a color/style declaration in the generated markup.
    QVERIFY(html.contains(QStringLiteral("color:"), Qt::CaseInsensitive));

    // Bold text is rendered with a heavier font-weight declaration.
    QVERIFY(html.contains(QStringLiteral("font-weight"), Qt::CaseInsensitive));

    // "next line" must land in a separate block/paragraph from "plain", i.e. the newline
    // survived as a QTextDocument block break rather than being collapsed.
    const int plainPos = static_cast<int>(html.indexOf(QStringLiteral("plain")));
    const int nextLinePos = static_cast<int>(html.indexOf(QStringLiteral("next line")));
    QVERIFY(plainPos >= 0);
    QVERIFY(nextLinePos > plainPos);
    const QString between = html.mid(plainPos, nextLinePos - plainPos);
    QVERIFY(between.contains(QStringLiteral("<p"), Qt::CaseInsensitive));
}

void TestQml::loadAnsiViewDialog()
{
    // Mirrors AnsiViewWindow's constructor arguments (see
    // ../src/viewers/AnsiViewWindow.h's makeAnsiViewWindow()); AnsiViewContent
    // is the widget-free content holder qml/AnsiViewDialog.cpp builds for the
    // real dialog (see ../src/viewers/AnsiViewContent.h).
    AnsiViewContent content(QStringLiteral("MMapper Ansi Viewer"),
                            QStringLiteral("Test Viewer"),
                            "\x1b[31mred\x1b[0m https://mume.org plain",
                            nullptr);

    QmlDialog dialog("t", "TestAnsiViewDialog", nullptr);
    dialog.setContextProperty("ansiViewContent", &content);
    dialog.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/AnsiViewDialog.qml"_qs));

    QQuickWidget *const quick = dialog.quickWidget();
    QVERIFY(quick != nullptr);
    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QQuickItem *const root = quick->rootObject();
    QVERIFY(root != nullptr);

    QQuickItem *const textArea = root->findChild<QQuickItem *>("ansiViewTextArea");
    QVERIFY(textArea != nullptr);
    const QString rendered = textArea->property("text").toString();
    QVERIFY(rendered.contains(QStringLiteral("red")));
    QVERIFY(rendered.contains(QStringLiteral("plain")));
    QVERIFY(rendered.contains(QStringLiteral("href"), Qt::CaseInsensitive));
}

void TestQml::loadAnsiColorPickerDialog()
{
    // AnsiColorPickerController is widget-free and config-independent, so
    // the real controller (not a stub) is used here, mirroring
    // loadRoomEditDialog()'s use of a real CheckableFlagModel.
    AnsiColorPickerController controller(nullptr);
    controller.init(QStringLiteral("[0m"));

    QmlDialog dialog("t", "TestAnsiColorPickerDialog", nullptr);
    dialog.setContextProperty("ansiColorPickerController", &controller);
    dialog.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/AnsiColorPickerDialog.qml"_qs));

    QQuickWidget *const quick = dialog.quickWidget();
    QVERIFY(quick != nullptr);
    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QQuickItem *const root = quick->rootObject();
    QVERIFY(root != nullptr);

    QQuickItem *const fgCombo = root->findChild<QQuickItem *>("fgCombo");
    QVERIFY(fgCombo != nullptr);
    // Default entry + one row per XFOREACH_ANSI_COLOR_0_7 color, low and high.
    QCOMPARE(fgCombo->property("count").toInt(), 17);

    // Parses "[<codes>m" into the semicolon-separated code list, so "1"
    // (bold) can be checked for without also matching the "1" inside "91"
    // (bright-red fg).
    const auto codes = [](const QString &ansiString) -> QStringList {
        QString inner = ansiString;
        inner.chop(1);      // trailing 'm'
        inner.remove(0, 1); // leading '['
        return inner.split(QStringLiteral(";"));
    };

    // Selecting index 4 ("RED", see AnsiColorTables::colorTable()'s
    // low-then-high ordering: 0 none, 1 black, 2 BLACK, 3 red, 4 RED, ...)
    // must update resultAnsiString/previewFg via setFgIndex(), matching
    // AnsiColorTables::generateAnsiString()'s "91" bright-red aixterm
    // encoding (ANSI_FG_COLOR_HI == 90, red's offset is 1).
    controller.setFgIndex(4);
    QVERIFY(codes(controller.getResultAnsiString()).contains(QStringLiteral("91")));
    QCOMPARE(controller.getPreviewFg(), mmqt::toQColor(AnsiColor16Enum::RED));

    controller.setBold(true);
    QVERIFY(codes(controller.getResultAnsiString()).contains(QStringLiteral("1")));
    controller.setUnderline(true);
    QVERIFY(codes(controller.getResultAnsiString()).contains(QStringLiteral("4")));

    QQuickItem *const boldCheckBox = root->findChild<QQuickItem *>("boldCheckBox");
    QVERIFY(boldCheckBox != nullptr);
    QCOMPARE(boldCheckBox->property("checked").toBool(), true);
}

void TestQml::loadRemoteEditDialog()
{
    RemoteEditController controller(true, QStringLiteral("Test Title"), QStringLiteral("hello\n"));

    QmlDialog dialog("t", "TestRemoteEditDialogLoad", nullptr);
    dialog.setContextProperty("remoteEditController", &controller);
    dialog.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/RemoteEditDialog.qml"_qs));

    QQuickWidget *const quick = dialog.quickWidget();
    QVERIFY(quick != nullptr);
    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();
    QCOMPARE(quick->status(), QQuickWidget::Ready);

    QQuickItem *const root = quick->rootObject();
    QVERIFY(root != nullptr);

    QQuickItem *const textArea = root->findChild<QQuickItem *>("remoteTextArea");
    QVERIFY(textArea != nullptr);
    QCOMPARE(textArea->property("text").toString(), QStringLiteral("hello\n"));
    QCOMPARE(textArea->property("readOnly").toBool(), false);

    QQuickItem *const footerLabel = root->findChild<QQuickItem *>("footerLabel");
    QVERIFY(footerLabel != nullptr);
    QVERIFY(!footerLabel->property("text").toString().isEmpty());
}

void TestQml::remoteEditControllerOps()
{
    // "hello\tworld\n": h(0)e(1)l(2)l(3)o(4)\t(5)w(6)o(7)r(8)l(9)d(10)\n(11)
    RemoteEditController controller(true,
                                    QStringLiteral("Test Title"),
                                    QStringLiteral("hello\tworld\n"));

    QmlDialog dialog("t", "TestRemoteEditDialogOps", nullptr);
    dialog.setContextProperty("remoteEditController", &controller);
    dialog.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/RemoteEditDialog.qml"_qs));

    QQuickWidget *const quick = dialog.quickWidget();
    QVERIFY(quick != nullptr);
    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();
    QCOMPARE(quick->status(), QQuickWidget::Ready);

    QQuickItem *const root = quick->rootObject();
    QVERIFY(root != nullptr);
    QQuickItem *const textArea = root->findChild<QQuickItem *>("remoteTextArea");
    QVERIFY(textArea != nullptr);
    QCOMPARE(textArea->property("text").toString(), QStringLiteral("hello\tworld\n"));

    // attachDocument() ran via Component.onCompleted; dirty starts false.
    QCOMPARE(controller.getDirty(), false);

    // find(): a plain C++ call against the controller, independent of QML --
    // "world" occupies [6, 11) in the initial text.
    QVariantMap found = controller.find(QStringLiteral("world"), false, 0, 0);
    QCOMPARE(found.value("found").toBool(), true);
    QCOMPARE(found.value("start").toInt(), 6);
    QCOMPARE(found.value("end").toInt(), 11);
    QCOMPARE(found.value("message").toString(), QStringLiteral("Found: 'world'"));

    QVariantMap notFound = controller.find(QStringLiteral("xyzzy"), false, 0, 0);
    QCOMPARE(notFound.value("found").toBool(), false);
    QCOMPARE(notFound.value("message").toString(), QStringLiteral("Not found: 'xyzzy'"));

    // gotoLine(): the text is "hello\tworld\n" -- QTextDocument's trailing
    // newline creates a second (empty) block, so lines 1 and 2 both exist;
    // line 3 does not.
    QVariantMap gotoOk = controller.gotoLine(1);
    QCOMPARE(gotoOk.value("found").toBool(), true);
    QCOMPARE(gotoOk.value("start").toInt(), 0);
    QVariantMap gotoOk2 = controller.gotoLine(2);
    QCOMPARE(gotoOk2.value("found").toBool(), true);
    QVariantMap gotoBad = controller.gotoLine(3);
    QCOMPARE(gotoBad.value("found").toBool(), false);

    // toggleWhitespace() flips the property each call.
    QCOMPARE(controller.getShowingWhitespace(), false);
    controller.toggleWhitespace();
    QCOMPARE(controller.getShowingWhitespace(), true);
    controller.toggleWhitespace();
    QCOMPARE(controller.getShowingWhitespace(), false);

    // expandTabs() over the whole document removes the tab and dirties the
    // controller; the change is visible through the QML TextArea because
    // it shares the same QTextDocument attachDocument() installed.
    controller.expandTabs(0, static_cast<int>(controller.getBody().length()));
    QCoreApplication::processEvents();
    QCOMPARE(controller.getDirty(), true);
    QTRY_VERIFY(!textArea->property("text").toString().contains(QLatin1Char('\t')));

    // Undo (through the shared QTextDocument, via the TextArea's own
    // invokable undo()) restores the original text and clears dirty again --
    // this exercises attachDocument()'s contentsChanged-driven recompute.
    QMetaObject::invokeMethod(textArea, "undo");
    QCoreApplication::processEvents();
    QTRY_COMPARE(controller.getDirty(), false);
    QTRY_COMPARE(textArea->property("text").toString(), QStringLiteral("hello\tworld\n"));

    // insertAnsiReset() inserts at the given position and returns the new
    // caret position, which must land after the inserted text.
    const int posBefore = 0;
    const int posAfter = controller.insertAnsiReset(posBefore);
    QVERIFY(posAfter > posBefore);
    QCOMPARE(controller.getDirty(), true);
    QMetaObject::invokeMethod(textArea, "undo");
    QCoreApplication::processEvents();
    QTRY_COMPARE(controller.getDirty(), false);

    // submit() emits sig_save with the current document text and sets
    // submitted; mirrors RemoteEditWidget::slot_finishEdit().
    QSignalSpy saveSpy(&controller, &RemoteEditController::sig_save);
    QCOMPARE(controller.getSubmitted(), false);
    controller.submit();
    QCOMPARE(saveSpy.count(), 1);
    QCOMPARE(saveSpy.at(0).at(0).toString(), QStringLiteral("hello\tworld\n"));
    QCOMPARE(controller.getSubmitted(), true);

    // A second submit()/cancel() call is a no-op (mirrors m_submitted
    // latching in the widget).
    controller.cancel();
    QCOMPARE(saveSpy.count(), 1);
}

void TestQml::remoteEditDialogCloseGuard()
{
    // Viewer session: RemoteEditDialogHost::closeEvent() always accepts
    // immediately (no confirmation), mirroring RemoteEditWidget::
    // closeEvent()'s !m_editSession branch.
    {
        auto *const controller = new RemoteEditController(false,
                                                          QStringLiteral("View Title"),
                                                          QStringLiteral("line one\n"));
        auto *const host = new RemoteEditDialogHost(controller, nullptr);
        controller->setParent(host);
        QCoreApplication::processEvents();

        QSignalSpy destroyedSpy(host, &QObject::destroyed);
        host->close();
        // closeEvent() ran synchronously inside close(); WA_DeleteOnClose
        // only *schedules* deleteLater(), so controller/host are still alive
        // here -- read them before pumping events below actually deletes
        // them.
        QCOMPARE(controller->getSubmitted(), true);
        QTRY_VERIFY(destroyedSpy.count() > 0);
    }

    // Edit session, not dirty: also closes immediately, no prompt.
    {
        auto *const controller = new RemoteEditController(true,
                                                          QStringLiteral("Edit Title"),
                                                          QStringLiteral("line one\n"));
        auto *const host = new RemoteEditDialogHost(controller, nullptr);
        controller->setParent(host);
        QCoreApplication::processEvents();

        QSignalSpy destroyedSpy(host, &QObject::destroyed);
        host->close();
        QCOMPARE(controller->getSubmitted(), true);
        QTRY_VERIFY(destroyedSpy.count() > 0);
    }

    // Edit session, dirty: closeEvent() shows a discard-confirmation
    // QMessageBox (Discard | Cancel, Cancel default/escape). QMessageBox::
    // exec() blocks, so this drives it via a deferred QTimer that fires
    // while that nested event loop is spinning -- a standard technique for
    // testing modal dialogs -- first clicking Cancel (the close must be
    // refused, submitted stays false, the host survives), then Discard (the
    // close proceeds, submitted becomes true and sig_cancel fires).
    {
        auto *const controller = new RemoteEditController(true,
                                                          QStringLiteral("Edit Title 2"),
                                                          QStringLiteral("line one\n"));
        auto *const host = new RemoteEditDialogHost(controller, nullptr);
        controller->setParent(host);

        QQuickWidget *const quick = host->quickWidget();
        QVERIFY(quick != nullptr);
        while (quick->status() == QQuickWidget::Loading) {
            QCoreApplication::processEvents();
        }
        QCoreApplication::processEvents();
        QQuickItem *const root = quick->rootObject();
        QVERIFY(root != nullptr);
        QQuickItem *const textArea = root->findChild<QQuickItem *>("remoteTextArea");
        QVERIFY(textArea != nullptr);

        // Dirty the shared document directly through the TextArea's own
        // invokable insert(), independent of any RemoteEditController op.
        QMetaObject::invokeMethod(textArea,
                                  "insert",
                                  Q_ARG(int, 0),
                                  Q_ARG(QString, QStringLiteral("X")));
        QCoreApplication::processEvents();
        QTRY_VERIFY(controller->getDirty());

        QSignalSpy cancelSpy(controller, &RemoteEditController::sig_cancel);

        // First attempt: Cancel keeps the dialog open.
        QTimer::singleShot(0, host, [] {
            for (QWidget *const w : QApplication::topLevelWidgets()) {
                if (auto *const box = qobject_cast<QMessageBox *>(w)) {
                    box->button(QMessageBox::Cancel)->click();
                    return;
                }
            }
        });
        host->close();
        QCOMPARE(controller->getSubmitted(), false);
        QCOMPARE(cancelSpy.count(), 0);
        QVERIFY(host->isVisible());

        // Second attempt: Discard actually closes it.
        QSignalSpy destroyedSpy(host, &QObject::destroyed);
        QTimer::singleShot(0, host, [] {
            for (QWidget *const w : QApplication::topLevelWidgets()) {
                if (auto *const box = qobject_cast<QMessageBox *>(w)) {
                    box->button(QMessageBox::Discard)->click();
                    return;
                }
            }
        });
        host->close();
        // Same ordering caveat as above: check controller state before the
        // QTRY_VERIFY wait actually lets WA_DeleteOnClose's deleteLater()
        // run.
        QCOMPARE(controller->getSubmitted(), true);
        QCOMPARE(cancelSpy.count(), 1);
        QTRY_VERIFY(destroyedSpy.count() > 0);
    }
}

void TestQml::remoteEditSessionQmlPath()
{
    // RemoteEditInternalSession's QML path (mpi/remoteeditsession.cpp's
    // #ifdef MMAPPER_WITH_QML branch) constructs a RemoteEditController +
    // RemoteEditDialogHost instead of a RemoteEditWidget. Goes in through
    // RemoteEdit::slot_remoteEdit() (like the real proxy layer would) rather
    // than constructing RemoteEditInternalSession directly, so the session
    // ends up tracked in RemoteEdit's m_sessions map and save()/cancel()
    // actually erase (and thus destroy) it -- proving the session layer's
    // construction/connection/teardown matches the widget path, not just the
    // controller/dialog in isolation (already covered by
    // remoteEditControllerOps()/remoteEditDialogCloseGuard() above).
    const bool originalInternalEditor = getConfig().mumeClientProtocol.internalRemoteEditor;
    setConfig().mumeClientProtocol.internalRemoteEditor = true;
    auto configGuard = qScopeGuard([originalInternalEditor] {
        setConfig().mumeClientProtocol.internalRemoteEditor = originalInternalEditor;
    });

    // Heap-allocated (not deleted): RemoteEdit's ctor takes ownership via
    // QObject parenting, and RemoteEditInternalSession's ctor casts
    // manager->parent() down to QWidget* (checked_dynamic_downcast, which
    // throws on a null/mismatched cast), so both need a real, non-stack
    // QObject parent chain, matching production (RemoteEdit's parent is the
    // MainWindow). Deliberately leaked, like the `new RemoteEditController`/
    // `new RemoteEditDialogHost` calls above.
    auto *const mainWindowStub = new QWidget();
    auto *const manager = new RemoteEdit(mainWindowStub);

    QSignalSpy saveSpy(manager, &RemoteEdit::sig_remoteEditSave);
    QSignalSpy cancelSpy(manager, &RemoteEdit::sig_remoteEditCancel);

    const auto sessionId = RemoteSessionId{42};
    manager->slot_remoteEdit(sessionId,
                             QStringLiteral("Session Title"),
                             QStringLiteral("session body\n"));
    QCoreApplication::processEvents();

    // The controller+host pair is a real top-level QDialog, shown the same
    // way RemoteEditWidget would have been on the widget path.
    RemoteEditDialogHost *host = nullptr;
    for (QWidget *const w : QApplication::topLevelWidgets()) {
        if (auto *const h = qobject_cast<RemoteEditDialogHost *>(w)) {
            host = h;
        }
    }
    QVERIFY(host != nullptr);
    QVERIFY(host->isVisible());
    QVERIFY(host->windowTitle().contains(QStringLiteral("Session Title")));

    auto *const controller = host->findChild<RemoteEditController *>();
    QVERIFY(controller != nullptr);
    QCOMPARE(controller->getBody(), QStringLiteral("session body\n"));
    QCOMPARE(controller->getIsEditSession(), true);

    // submit() -> RemoteEditController::sig_save -> RemoteEditSession::
    // slot_onSave -> RemoteEdit::save() -> RemoteEdit::sendToMume() (session
    // is still "connected" by default), which emits sig_remoteEditSave and
    // erases the session from RemoteEdit's map, destroying the
    // RemoteEditInternalSession and (via its destructor closing m_dialog)
    // `host` as well.
    QSignalSpy hostDestroyedSpy(host, &QObject::destroyed);
    controller->submit();
    QCOMPARE(saveSpy.count(), 1);
    QCOMPARE(saveSpy.at(0).at(0).value<RemoteSessionId>(), sessionId);
    QCOMPARE(cancelSpy.count(), 0);
    QTRY_VERIFY(hostDestroyedSpy.count() > 0);
}

void TestQml::loadPasswordDialog()
{
    // PasswordDialogController is widget-free and config-independent, so the
    // real controller (not a stub) is used here, mirroring
    // loadAnsiColorPickerDialog()'s use of a real AnsiColorPickerController.
    PasswordDialogController controller(QStringLiteral("hero"),
                                        QStringLiteral("hunter2"),
                                        /*hasStoredPassword=*/true,
                                        /*showPasswordControls=*/true,
                                        nullptr);

    QmlDialog dialog("t", "TestPasswordDialog", nullptr);
    dialog.setContextProperty("passwordDialogController", &controller);
    dialog.setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/PasswordDialog.qml"_qs));

    QQuickWidget *const quick = dialog.quickWidget();
    QVERIFY(quick != nullptr);
    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QQuickItem *const root = quick->rootObject();
    QVERIFY(root != nullptr);

    QQuickItem *const passwordField = root->findChild<QQuickItem *>("passwordField");
    QVERIFY(passwordField != nullptr);
    // TextInput.Password == 2, TextInput.Normal == 0 (QQuickTextInput's
    // EchoMode enum); mirrors ManagePasswordDialog's showPassword toggle.
    QCOMPARE(passwordField->property("echoMode").toInt(), 2);

    QQuickItem *const viewButton = root->findChild<QQuickItem *>("viewButton");
    QVERIFY(viewButton != nullptr);
    viewButton->setProperty("checked", true);
    QCOMPARE(passwordField->property("echoMode").toInt(), 0);
    viewButton->setProperty("checked", false);
    QCOMPARE(passwordField->property("echoMode").toInt(), 2);

    QQuickItem *const deleteButton = root->findChild<QQuickItem *>("deleteButton");
    QVERIFY(deleteButton != nullptr);
    QCOMPARE(deleteButton->property("enabled").toBool(), true);
    QCOMPARE(controller.getHasStoredPassword(), true);

    // Clicking Delete (invoking the controller directly, mirroring how the
    // QML Button's onClicked calls requestDelete()) mirrors
    // ManagePasswordDialog's deleteButton handler: hasStoredPassword flips
    // to false and the Delete button's enabled binding follows it.
    controller.requestDelete();
    QCOMPARE(controller.getHasStoredPassword(), false);
    QCOMPARE(deleteButton->property("enabled").toBool(), false);
}

void TestQml::loadMapView()
{
    // MapView.qml instantiates either `MapCanvasUnderlay { core: root.core }`
    // or `MapCanvasItem { core: root.core }` (via a Loader keyed on the
    // "useCanvasUnderlay" root context property -- see MapView.qml's file
    // comment), with root.core left at its default (null). No
    // "useCanvasUnderlay" context property is set here, so MapView.qml's own
    // `typeof useCanvasUnderlay === "undefined"` default (true) applies --
    // matching QmlShellWindow's own default (see QmlShellWindow.cpp) -- and
    // it instantiates the MapCanvasUnderlay path. initTestCase() registered
    // MapCanvasUnderlayItemStub (see its doc comment above) under that type
    // name for exactly this test: it lets MapView.qml's chrome/layout load
    // and bind without needing the real MapCanvasUnderlayItem class (and the
    // MapCanvasCore/OpenGL dependency graph that comes with it) linked into
    // this binary. The real class's own null-core handling is exercised by
    // it simply compiling/linking cleanly into the production app (and by
    // beforeRenderPassRecording() never firing at all under the "software"
    // Qt Quick backend this test binary forces -- see initTestCase()).
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/MMapper/MapView.qml"_qs));

    while (component.isLoading()) {
        QCoreApplication::processEvents();
    }
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> object(component.create(engine.rootContext()));
    QVERIFY(object != nullptr);
    QCoreApplication::processEvents();

    auto *const canvasItem = object->findChild<MapCanvasUnderlayItemStub *>();
    QVERIFY(canvasItem != nullptr);
    QCOMPARE(canvasItem->getCore(), nullptr);
    // The FBO fallback type must NOT have been instantiated on this path.
    QVERIFY(object->findChild<MapCanvasItemStub *>() == nullptr);

    // Resizing/reparenting the null-core item must not crash.
    canvasItem->setWidth(400);
    canvasItem->setHeight(300);
    QCoreApplication::processEvents();
}

void TestQml::loadMapViewFboFallback()
{
    // Mirrors loadMapView() above, except with the "useCanvasUnderlay" root
    // context property explicitly set to false -- mirroring
    // QmlShellWindow.cpp wiring it to `false` when the MMAPPER_CANVAS_FBO=1
    // escape-hatch environment variable is set. MapView.qml's Loader must
    // then instantiate the MapCanvasItem (QQuickFramebufferObject fallback)
    // path instead of the default MapCanvasUnderlay path -- this is the
    // "context property toggle" MapView.qml's file comment describes.
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("useCanvasUnderlay", false);
    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/MMapper/MapView.qml"_qs));

    while (component.isLoading()) {
        QCoreApplication::processEvents();
    }
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> object(component.create(engine.rootContext()));
    QVERIFY(object != nullptr);
    QCoreApplication::processEvents();

    auto *const canvasItem = object->findChild<MapCanvasItemStub *>();
    QVERIFY(canvasItem != nullptr);
    QCOMPARE(canvasItem->getCore(), nullptr);
    // The underlay type must NOT have been instantiated on this path.
    QVERIFY(object->findChild<MapCanvasUnderlayItemStub *>() == nullptr);

    canvasItem->setWidth(400);
    canvasItem->setHeight(300);
    QCoreApplication::processEvents();
}

void TestQml::mapViewModelScrollMath()
{
    // Regression test for the world<->scroll-unit conversion extracted out
    // of MapWindow::KnownMapSize (mapwindow.h/.cpp) into MapViewModel.
    MapViewModel model;

    // A 10x6-unit known map (mirrors MapData::sig_mapSizeChanged's
    // min/max Coordinates, which MapWindow::slot_setScrollBars() --
    // now MapViewModel::slot_setScrollBars() -- receives).
    const Coordinate min{-2, -3, 0};
    const Coordinate max{8, 3, 0};

    QSignalSpy rangeSpy(&model, &MapViewModel::sig_rangeChanged);
    model.slot_setScrollBars(min, max);
    QCOMPARE(rangeSpy.count(), 1);

    // size() is (max - min) = (10, 6); range is size * SCROLL_SCALE.
    QCOMPARE(model.getHorizontalScrollMax(), 10 * MapViewModel::SCROLL_SCALE);
    QCOMPARE(model.getVerticalScrollMax(), 6 * MapViewModel::SCROLL_SCALE);

    // worldToScroll() must invert scrollToWorld() (modulo the integer
    // rounding baked into scroll units).
    const glm::vec2 world{1.0f, 2.0f};
    const glm::ivec2 scroll = model.worldToScroll(world);
    const glm::vec2 roundTripped = model.scrollToWorld(scroll);
    QVERIFY(std::abs(roundTripped.x - world.x) < 0.01f);
    QVERIFY(std::abs(roundTripped.y - world.y) < 0.01f);

    // Spot-check an exact known value: world == min maps to scroll (0, dims.y)
    // (Y is negated/flipped -- see KnownMapSize::worldToScroll()'s comment).
    const glm::ivec2 scrollAtMin = model.worldToScroll(glm::vec2{min.x, min.y});
    QCOMPARE(scrollAtMin.x, 0);
    QCOMPARE(scrollAtMin.y, 6 * MapViewModel::SCROLL_SCALE);

    // ... and world == max maps to scroll (dims.x, 0).
    const glm::ivec2 scrollAtMax = model.worldToScroll(glm::vec2{max.x, max.y});
    QCOMPARE(scrollAtMax.x, 10 * MapViewModel::SCROLL_SCALE);
    QCOMPARE(scrollAtMax.y, 0);

    // Continuous-scroll timer: starting a non-zero step must fire
    // sig_continuousScrollStep() periodically until stopped.
    QSignalSpy stepSpy(&model, &MapViewModel::sig_continuousScrollStep);
    model.slot_continuousScroll(1, 0);
    QTRY_VERIFY_WITH_TIMEOUT(!stepSpy.isEmpty(), 2000);
    QCOMPARE(stepSpy.constLast().at(0).toInt(), 1);
    QCOMPARE(stepSpy.constLast().at(1).toInt(), 0);

    model.slot_continuousScroll(0, 0);
    stepSpy.clear();
    QTest::qWait(250);
    QCOMPARE(stepSpy.count(), 0);
}

void TestQml::loadMainShell()
{
    // MainShell.qml (../src/qml/shell/MainShell.qml) is Shell B's top-level
    // window -- see ../src/mainwindow/QmlShellWindow.h/.cpp and
    // ../src/main.cpp's --qml-shell handling, which construct the
    // QQmlApplicationEngine that loads it in the real app. This test drives
    // it with real CommandRegistry/UiCommand/MapViewModel instances (see
    // command_registry_SRCS/display_map_view_SRCS in tests/CMakeLists.txt
    // for why those link cleanly here) standing in for QmlShellWindow's
    // context properties, plus initTestCase()'s MapCanvasItemStub
    // registration (see its own comment above) for the "MapCanvasItem" type
    // MapView.qml instantiates.
    CommandRegistry registry(nullptr);
    static constexpr std::array kLiveIds{
        "file.exit",
        "view.zoom-in",
        "view.zoom-out",
        "view.zoom-reset",
        "layer.up",
        "layer.down",
        "layer.reset",
        "world.rebuild-meshes",
        "mouse-mode.move",
        "mouse-mode.room-raypick",
        "mouse-mode.room-select",
        "mouse-mode.connection-select",
        "mouse-mode.create-room",
        "mouse-mode.create-connection",
        "mouse-mode.create-oneway-connection",
        "mouse-mode.infomark-select",
        "mouse-mode.create-infomark",
    };
    for (const char *const id : kLiveIds) {
        const bool checkable = QByteArray(id).startsWith("mouse-mode.");
        UiCommand *const cmd = registry.addCommand(QString::fromLatin1(id), checkable);
        if (checkable) {
            registry.addToGroup(cmd, QStringLiteral("mouse-mode"), true);
        }
    }
    registry.command(QStringLiteral("mouse-mode.move"))->setChecked(true);

    MapViewModel viewModel;

    // Toolbar/statusbar chrome fixtures -- see loadMainShellChrome() below
    // for the dedicated test exercising these; this test only needs them
    // present so MainShell.qml's footer (ClockStrip.qml/XpStatusItem.qml,
    // neither of which null-guards its context property) and header
    // toolbars load without a ReferenceError. mapZoom stays a null QObject*
    // (like mapCore above): MapZoomController binds directly against a real
    // MapCanvasCore, which this small test binary deliberately never links
    // (see MapZoomController.h's file comment and command_registry_SRCS's
    // comment above for the same tradeoff); MainShell.qml's zoom Slider
    // guards against that with `mapZoom ? ... : 0`.
    ToolbarLayoutController toolbarLayout;
    AudioVolumeController musicVolume(AudioVolumeController::AudioType::Music);
    AudioVolumeController soundVolume(AudioVolumeController::AudioType::Sound);
    GameObserver gameObserver;
    MumeClock mumeClock(/*mumeEpoch=*/0, gameObserver, nullptr);
    ClockAdapter clockAdapter(gameObserver, mumeClock, nullptr);
    AdventureTracker adventureTracker(gameObserver, nullptr);
    XpStatusAdapter xpStatusAdapter(adventureTracker, nullptr);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty("commands", &registry);
    engine.rootContext()->setContextProperty("mapCore", QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty("mapViewModel", &viewModel);
    engine.rootContext()->setContextProperty("statusText", QStringLiteral("test status"));
    engine.rootContext()->setContextProperty("toolbarLayout", &toolbarLayout);
    engine.rootContext()->setContextProperty("mapZoom", QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty("musicVolume", &musicVolume);
    engine.rootContext()->setContextProperty("soundVolume", &soundVolume);
    engine.rootContext()->setContextProperty("clock", &clockAdapter);
    engine.rootContext()->setContextProperty("xpStatusAdapter", &xpStatusAdapter);

    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/MMapper/MainShell.qml"_qs));

    while (component.isLoading()) {
        QCoreApplication::processEvents();
    }
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> object(component.create(engine.rootContext()));
    QVERIFY(object != nullptr);
    QCoreApplication::processEvents();

    QVERIFY(qobject_cast<QQuickWindow *>(object.data()) != nullptr);

    // ApplicationWindow's "menuBar" attached property must resolve to a
    // real MenuBar instance (the File/View/Mode menus built in
    // MainShell.qml).
    const QVariant menuBarProp = object->property("menuBar");
    QVERIFY(menuBarProp.isValid());
    QVERIFY(menuBarProp.value<QObject *>() != nullptr);

    // No "useCanvasUnderlay" context property is set above, so MapView.qml
    // defaults to the MapCanvasUnderlay path -- see loadMapView()'s comment.
    auto *const mapCanvasItem = object->findChild<MapCanvasUnderlayItemStub *>();
    QVERIFY(mapCanvasItem != nullptr);
    QCOMPARE(mapCanvasItem->getCore(), nullptr);

    auto *const statusLabel = object->findChild<QQuickItem *>(QStringLiteral("statusLabel"));
    QVERIFY(statusLabel != nullptr);
    QCOMPARE(statusLabel->property("text").toString(), QStringLiteral("test status"));

    // Triggering a live command through the registry (exactly what
    // CommandAction.qml's onTriggered does) must not crash even though
    // nothing here is actually bound to a real MapCanvasCore.
    registry.command(QStringLiteral("view.zoom-in"))->trigger();
}

void TestQml::pathMachineStatusFunnel()
{
    // QmlShellWindow itself isn't linkable into this small test binary (see
    // loadMainShell()'s comment above), so this exercises the QML side of
    // QmlShellWindow.cpp's wirePathMachine(): a synthetic "pathMachineStatus"
    // context property update -- standing in for a real
    // Mmapper2PathMachine::sig_state emission funneled through
    // `rootContext->setContextProperty("pathMachineStatus", text)` -- must
    // reach MainShell.qml's footer pathMachineLabel, mirroring
    // MainWindow::setupStatusBar()'s `connect(m_pathMachine, &sig_state,
    // pathmachineStatus, &QLabel::setText)`.
    CommandRegistry registry(nullptr);
    MapViewModel viewModel;
    ToolbarLayoutController toolbarLayout;
    AudioVolumeController musicVolume(AudioVolumeController::AudioType::Music);
    AudioVolumeController soundVolume(AudioVolumeController::AudioType::Sound);
    GameObserver gameObserver;
    MumeClock mumeClock(/*mumeEpoch=*/0, gameObserver, nullptr);
    ClockAdapter clockAdapter(gameObserver, mumeClock, nullptr);
    AdventureTracker adventureTracker(gameObserver, nullptr);
    XpStatusAdapter xpStatusAdapter(adventureTracker, nullptr);

    QQmlEngine engine;
    QQmlContext *const rootContext = engine.rootContext();
    rootContext->setContextProperty("commands", &registry);
    rootContext->setContextProperty("mapCore", QVariant::fromValue<QObject *>(nullptr));
    rootContext->setContextProperty("mapViewModel", &viewModel);
    rootContext->setContextProperty("statusText", QStringLiteral("test status"));
    rootContext->setContextProperty("toolbarLayout", &toolbarLayout);
    rootContext->setContextProperty("mapZoom", QVariant::fromValue<QObject *>(nullptr));
    rootContext->setContextProperty("musicVolume", &musicVolume);
    rootContext->setContextProperty("soundVolume", &soundVolume);
    rootContext->setContextProperty("clock", &clockAdapter);
    rootContext->setContextProperty("xpStatusAdapter", &xpStatusAdapter);
    // Starts unset, exactly like QmlShellWindow.cpp's ctor's initial
    // `setContextProperty("pathMachineStatus", QString())` before any
    // sig_state has fired.
    rootContext->setContextProperty("pathMachineStatus", QString());

    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/MMapper/MainShell.qml"_qs));
    while (component.isLoading()) {
        QCoreApplication::processEvents();
    }
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> object(component.create(rootContext));
    QVERIFY(object != nullptr);
    QCoreApplication::processEvents();

    auto *const pathMachineLabel = object->findChild<QQuickItem *>(
        QStringLiteral("pathMachineLabel"));
    QVERIFY(pathMachineLabel != nullptr);
    QCOMPARE(pathMachineLabel->property("text").toString(), QString());

    // Simulate Mmapper2PathMachine::sig_state firing.
    rootContext->setContextProperty("pathMachineStatus", QStringLiteral("Standing."));
    QCoreApplication::processEvents();
    QCOMPARE(pathMachineLabel->property("text").toString(), QStringLiteral("Standing."));
}

void TestQml::mapperModeCommandGrouping()
{
    // Mirrors the "mapper-mode" exclusive UiCommand group QmlShellWindow.cpp's
    // wireMapperModeCommand() creates (see registerCommands()): triggering
    // one of the three mapper-mode commands must check it and uncheck the
    // other two, exactly like the pre-existing "mouse-mode" group already
    // covered by loadMainShell()'s trigger smoke test above. This is the
    // "command enable states" coverage called for since the real
    // QmlShellWindow (and therefore its ConnectionListener/Proxy-touching
    // setMapperMode()) isn't linkable into this test binary.
    CommandRegistry registry(nullptr);
    UiCommand *const playCmd = registry.addCommand(QStringLiteral("mapper-mode.play"), true);
    UiCommand *const mapCmd = registry.addCommand(QStringLiteral("mapper-mode.map"), true);
    UiCommand *const offlineCmd = registry.addCommand(QStringLiteral("mapper-mode.offline"), true);
    registry.addToGroup(playCmd, QStringLiteral("mapper-mode"), true);
    registry.addToGroup(mapCmd, QStringLiteral("mapper-mode"), true);
    registry.addToGroup(offlineCmd, QStringLiteral("mapper-mode"), true);

    playCmd->setChecked(true);
    QVERIFY(playCmd->isChecked());
    QVERIFY(!mapCmd->isChecked());
    QVERIFY(!offlineCmd->isChecked());

    mapCmd->setChecked(true);
    QVERIFY(!playCmd->isChecked());
    QVERIFY(mapCmd->isChecked());
    QVERIFY(!offlineCmd->isChecked());

    offlineCmd->setChecked(true);
    QVERIFY(!playCmd->isChecked());
    QVERIFY(!mapCmd->isChecked());
    QVERIFY(offlineCmd->isChecked());
}

void TestQml::dockLayoutControllerAreas()
{
    // Covers the "move panels between dock areas" feature: DockLayoutController's
    // leftDockIds/topDockIds/bottomDockIds/rightDockIds (see
    // ../src/qml/DockLayoutController.h) and its setDockArea()/dockArea()
    // invokables, exercised directly against the controller (no QML/engine
    // needed -- MainShell.qml's Repeaters just bind straight to these
    // lists, covered separately by loadMainShellDocks()/
    // dockContainerCollapsesWhenEmpty() above).
    DockLayoutController dockLayout;

    // Defaults mirror mainwindow.cpp's dock construction (see
    // DockLayoutController.h's Q_PROPERTY comment): Client left/visible,
    // Group top/visible, Description right/visible; Log/Room/Adventure/
    // Tasks/Timers all start hidden, so bottom starts empty despite 4 docks
    // defaulting there.
    QCOMPARE(dockLayout.property("leftDockIds").toStringList(), QStringList{"client"});
    QCOMPARE(dockLayout.property("topDockIds").toStringList(), QStringList{"group"});
    QCOMPARE(dockLayout.property("rightDockIds").toStringList(), QStringList{"description"});
    QCOMPARE(dockLayout.property("bottomDockIds").toStringList(), QStringList{});

    // Showing 2 of the bottom-area-default docks populates bottomDockIds in
    // canonical id order (client, group, log, room, adventure, tasks,
    // description, timers -- see DockLayoutController.h's file comment),
    // not insertion order.
    dockLayout.setProperty("logVisible", true);
    dockLayout.setProperty("roomVisible", true);
    QCOMPARE(dockLayout.property("bottomDockIds").toStringList(), (QStringList{"log", "room"}));

    // Moving "log" to the left area: leftDockIds gains it (after "client",
    // per canonical order), bottomDockIds loses it.
    dockLayout.setDockArea(QStringLiteral("log"), QStringLiteral("left"));
    QCOMPARE(dockLayout.property("leftDockIds").toStringList(), (QStringList{"client", "log"}));
    QCOMPARE(dockLayout.property("bottomDockIds").toStringList(), QStringList{"room"});

    // Moving "group" (currently top, visible) to bottom: topDockIds empties
    // out, bottomDockIds gains it in canonical id order -- "group" (index 1)
    // sorts before "room" (index 3), so it lands ahead of the "room" that's
    // already there.
    dockLayout.setDockArea(QStringLiteral("group"), QStringLiteral("bottom"));
    QCOMPARE(dockLayout.property("topDockIds").toStringList(), QStringList{});
    QCOMPARE(dockLayout.property("bottomDockIds").toStringList(), (QStringList{"group", "room"}));

    QCOMPARE(dockLayout.dockArea(QStringLiteral("group")), QStringLiteral("bottom"));
    QCOMPARE(dockLayout.dockArea(QStringLiteral("nonexistent")), QString());

    // An invalid area string is a silent no-op -- "group" stays in bottom.
    dockLayout.setDockArea(QStringLiteral("group"), QStringLiteral("nonsense"));
    QCOMPARE(dockLayout.dockArea(QStringLiteral("group")), QStringLiteral("bottom"));
    QCOMPARE(dockLayout.property("bottomDockIds").toStringList(), (QStringList{"group", "room"}));

    // Floating a dock excludes it from every area list, same as hiding it
    // does -- MainShell.qml's per-area Repeaters rely on this to drop a
    // dock the instant it's popped out into its own Window (see
    // FloatingDock in MainShell.qml).
    dockLayout.setProperty("clientFloating", true);
    QVERIFY(!dockLayout.property("leftDockIds").toStringList().contains("client"));
}

void TestQml::loadMainShellDocks()
{
    // Drives MainShell.qml's 8-dock SplitView layout (added alongside
    // DockLayoutController -- see ../src/qml/DockLayoutController.h and
    // ../src/mainwindow/QmlShellWindow.cpp's ctor) with one context
    // property per panel, reusing the exact same stub/fixture objects each
    // individual panel test above uses (GroupControllerStub for
    // loadGroupPanel(), FakeBackend for loadClientPanel(), a bare
    // RoomManager for loadRoomPanel(), ...) rather than constructing a
    // second copy of QmlShellWindow's real wiring.
    CommandRegistry registry(nullptr);
    MapViewModel viewModel;

    DockLayoutController dockLayout;

    QmlConfig config;

    LogModel logModel(nullptr);

    GroupModel groupModel;
    GroupProxyModel groupProxy;
    groupProxy.setSourceModel(&groupModel);
    GroupControllerStub groupController(nullptr);

    RoomManager roomManager(nullptr);
    RoomModel roomModel(nullptr, roomManager.getRoom());

    GameObserver gameObserver;
    AdventureTracker adventureTracker(gameObserver, nullptr);
    AdventureLogModel adventureLogModel(adventureTracker, nullptr);
    // Statusbar footer fixtures -- see loadMainShellChrome() below for the
    // dedicated toolbar/statusbar test; needed here only because
    // ClockStrip.qml/XpStatusItem.qml (unconditionally instantiated in
    // MainShell.qml's footer) don't null-guard their context properties.
    XpStatusAdapter xpStatusAdapter(adventureTracker, nullptr);
    MumeClock mumeClock(/*mumeEpoch=*/0, gameObserver, nullptr);
    ClockAdapter clockAdapter(gameObserver, mumeClock, nullptr);
    ToolbarLayoutController toolbarLayout;
    AudioVolumeController musicVolume(AudioVolumeController::AudioType::Music);
    AudioVolumeController soundVolume(AudioVolumeController::AudioType::Sound);

    MediaLibrary mediaLibrary;
    DescriptionAdapter descriptionAdapter(mediaLibrary, nullptr);

    CTimers timers(nullptr);
    TimerModel timerModel(timers, nullptr);
    TimerController timerController(timers, timerModel, nullptr);

    TasksModel tasksModel;

    ClientLineModel clientLineModel;
    HotkeyManager hotkeys;
    hotkeys.resetToDefaults();
    ClientController clientController(clientLineModel, hotkeys, nullptr);
    auto backend = std::make_unique<FakeBackend>();
    clientController.setBackend(std::move(backend));

    QQmlEngine engine;
    // Deliberately does NOT register a "groupicons" image provider -- same
    // dependency-graph tradeoff loadGroupPanel() documents above
    // (GroupIconProvider.cpp would drag in display/Filenames.cpp and, with
    // it, most of the parser/mapdata graph this small test binary avoids).
    // Must be registered before component creation, like
    // QmlDockWidget::addImageProvider()'s doc comment requires.
    engine.addImageProvider(QStringLiteral("description"),
                            new DescriptionImageProvider(descriptionAdapter.getStore()));

    engine.rootContext()->setContextProperty("commands", &registry);
    engine.rootContext()->setContextProperty("mapCore", QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty("mapViewModel", &viewModel);
    engine.rootContext()->setContextProperty("statusText", QStringLiteral("test status"));
    engine.rootContext()->setContextProperty("dockLayout", &dockLayout);
    engine.rootContext()->setContextProperty("config", &config);
    engine.rootContext()->setContextProperty("logModel", &logModel);
    engine.rootContext()->setContextProperty("groupModel", &groupModel);
    engine.rootContext()->setContextProperty("groupProxyModel", &groupProxy);
    engine.rootContext()->setContextProperty("groupController", &groupController);
    engine.rootContext()->setContextProperty("roomModel", &roomModel);
    engine.rootContext()->setContextProperty("adventureLogModel", &adventureLogModel);
    engine.rootContext()->setContextProperty("adapter", &descriptionAdapter);
    engine.rootContext()->setContextProperty("timerModel", &timerModel);
    engine.rootContext()->setContextProperty("timerController", &timerController);
    engine.rootContext()->setContextProperty("tasksModel", &tasksModel);
    engine.rootContext()->setContextProperty("toolbarLayout", &toolbarLayout);
    engine.rootContext()->setContextProperty("mapZoom", QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty("musicVolume", &musicVolume);
    engine.rootContext()->setContextProperty("soundVolume", &soundVolume);
    engine.rootContext()->setContextProperty("clock", &clockAdapter);
    engine.rootContext()->setContextProperty("xpStatusAdapter", &xpStatusAdapter);
    engine.rootContext()->setContextProperty("clientController", &clientController);
    engine.rootContext()->setContextProperty("clientLineModel", &clientLineModel);

    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/MMapper/MainShell.qml"_qs));
    while (component.isLoading()) {
        QCoreApplication::processEvents();
    }
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> object(component.create(engine.rootContext()));
    QVERIFY(object != nullptr);
    QCoreApplication::processEvents();

    // Each of the 4 fixed slots' DockPanel children are now Repeater
    // delegates driven by DockLayoutController's leftDockIds/topDockIds/
    // bottomDockIds/rightDockIds (see ../src/qml/DockLayoutController.h and
    // MainShell.qml's "dock region" comment) instead of 8 hard-coded
    // DockPanel blocks -- a dock only exists as a child at all while it's
    // both docked (area == its Repeater's container) and xVisible, so
    // Log/Room/Adventure/Timers/Tasks (all hidden by default) simply have
    // no "dockX"-named child yet, unlike the old always-instantiated-but-
    // hidden Loader design. Only Group/Description/Client -- visible by
    // default -- are found up front; the rest are looked up again after
    // being toggled visible below.
    static constexpr std::array kVisibleByDefaultDockNames{
        "dockGroup",
        "dockDescription",
        "dockClient",
    };
    for (const char *const name : kVisibleByDefaultDockNames) {
        QObject *const dock = object->findChild<QObject *>(QString::fromLatin1(name));
        QVERIFY2(dock != nullptr, name);
    }
    static constexpr std::array kHiddenByDefaultDockNames{
        "dockLog",
        "dockRoom",
        "dockAdventure",
        "dockTimers",
        "dockTasks",
    };
    for (const char *const name : kHiddenByDefaultDockNames) {
        // Create/destroy placement (see MainShell.qml's reconcileDocks): a
        // dock has a DockPanel instance ONLY while it is docked-and-shown; a
        // hidden dock has none at all (it is created on show and destroyed on
        // hide/float/move), so findChild does not find it.
        QVERIFY2(object->findChild<QObject *>(QString::fromLatin1(name)) == nullptr, name);
    }

    // Defaults mirror mainwindow.cpp's dock->hide()/no-hide-call split (see
    // DockLayoutController.h's Q_PROPERTY comment): Log/Room/Adventure/
    // Timers/Tasks start hidden, Group/Description/Client start visible.
    auto *const dockGroup = object->findChild<QQuickItem *>(QStringLiteral("dockGroup"));
    auto *const dockDescription = object->findChild<QQuickItem *>(QStringLiteral("dockDescription"));
    auto *const dockClient = object->findChild<QQuickItem *>(QStringLiteral("dockClient"));
    QVERIFY(dockGroup != nullptr);
    QVERIFY(dockDescription != nullptr);
    QVERIFY(dockClient != nullptr);
    QCOMPARE(dockGroup->isVisible(), true);
    QCOMPARE(dockDescription->isVisible(), true);
    QCOMPARE(dockClient->isVisible(), true);

    // Toggling a dock's visibility through the same controller property the
    // "Side Panels" menu items and each DockPanel's close button drive must
    // not crash the surrounding SplitView, and must create/destroy the dock's
    // DockPanel accordingly -- reflected as the instance appearing/vanishing
    // from the object tree.
    dockLayout.setProperty("logVisible", true);
    QCoreApplication::processEvents();
    {
        auto *const l = object->findChild<QQuickItem *>(QStringLiteral("dockLog"));
        QVERIFY(l != nullptr);
        QCOMPARE(l->isVisible(), true);
    }

    dockLayout.setProperty("groupVisible", false);
    // QTRY_*: destroy() defers deletion to a later event-loop pass, so the
    // instance may outlive a single processEvents().
    QTRY_COMPARE(object->findChild<QObject *>(QStringLiteral("dockGroup")), nullptr);

    dockLayout.setProperty("groupVisible", true);
    dockLayout.setProperty("logVisible", false);
    QTRY_VERIFY(object->findChild<QQuickItem *>(QStringLiteral("dockGroup")) != nullptr);
    QCOMPARE(object->findChild<QQuickItem *>(QStringLiteral("dockGroup"))->isVisible(), true);
    QTRY_COMPARE(object->findChild<QObject *>(QStringLiteral("dockLog")), nullptr);

    // Moving a panel to another area must leave it actually shown and
    // laid-out there (regression: reparenting a live item between SplitViews
    // left it reserving space but not drawing). A move destroys the source
    // instance and creates a fresh one in the target. Move Description
    // (right) to the left column, which already holds Client -- this also
    // exercises two panels stacked in one area.
    auto *const leftColumn = object->findChild<QQuickItem *>(QStringLiteral("leftColumn"));
    QVERIFY(leftColumn != nullptr);
    auto *const leftContent = leftColumn->property("contentItem").value<QQuickItem *>();
    QMetaObject::invokeMethod(&dockLayout,
                              "setDockArea",
                              Q_ARG(QString, QStringLiteral("description")),
                              Q_ARG(QString, QStringLiteral("left")));
    // QTRY_*: the destroyed source instance (same objectName) can linger in
    // the object tree until its deferred delete runs, so wait until the
    // fresh Description instance is the one parented under the left column.
    QTRY_VERIFY(object->findChild<QQuickItem *>(QStringLiteral("dockDescription")) != nullptr
                && object->findChild<QQuickItem *>(QStringLiteral("dockDescription"))->parentItem()
                       == leftContent);
    {
        auto *const d = object->findChild<QQuickItem *>(QStringLiteral("dockDescription"));
        auto *const c = object->findChild<QQuickItem *>(QStringLiteral("dockClient"));
        QVERIFY(d != nullptr);
        QVERIFY(c != nullptr);
        // Both stacked panels must be visible and non-zero-sized.
        QVERIFY2(d->isVisible(), "moved Description not visible");
        QVERIFY2(d->width() > 0.0 && d->height() > 0.0, "moved Description has zero size");
        QVERIFY2(c->isVisible(), "stacked Client not visible");
        QVERIFY2(c->width() > 0.0 && c->height() > 0.0, "stacked Client has zero size");
    }

    // Moving a panel into a previously-EMPTY/collapsed area is the harder
    // case: that area's container starts visible:false (the outer SplitView
    // gave its space to the map), so when a panel arrives the container must
    // flip visible AND be re-allocated non-zero space by the outer
    // SplitView -- otherwise the panel shows at zero size and can't be
    // interacted with. bottomRow starts empty (all its default docks
    // hidden); move Group into it.
    auto *const bottomRow = object->findChild<QQuickItem *>(QStringLiteral("bottomRow"));
    QVERIFY(bottomRow != nullptr);
    QCOMPARE(bottomRow->isVisible(), false);
    auto *const bottomContent = bottomRow->property("contentItem").value<QQuickItem *>();
    QMetaObject::invokeMethod(&dockLayout,
                              "setDockArea",
                              Q_ARG(QString, QStringLiteral("group")),
                              Q_ARG(QString, QStringLiteral("bottom")));
    QTRY_VERIFY(object->findChild<QQuickItem *>(QStringLiteral("dockGroup")) != nullptr
                && object->findChild<QQuickItem *>(QStringLiteral("dockGroup"))->parentItem()
                       == bottomContent);
    {
        auto *const g = object->findChild<QQuickItem *>(QStringLiteral("dockGroup"));
        QVERIFY(g != nullptr);
        QVERIFY2(bottomRow->isVisible(), "bottomRow did not become visible after a panel moved in");
        QVERIFY2(bottomRow->height() > 0.0, "bottomRow has zero height after a panel moved in");
        QVERIFY2(g->isVisible(), "moved-into-empty-area Group not visible");
        QVERIFY2(g->width() > 0.0 && g->height() > 0.0, "moved-into-empty-area Group has zero size");
    }
}

void TestQml::dockContainerCollapsesWhenEmpty()
{
    // Covers the "closing panels does not give the space back to the
    // central map canvas" bug report: MainShell.qml's leftColumn/
    // rightColumn/topRow/bottomRow (the SplitView children that group the
    // 8 DockPanels into the shell's four fixed slots) now bind their own
    // `visible` to "is any panel inside me docked-and-shown", instead of
    // always staying visible and reserving their SplitView.preferredWidth/
    // Height even once every panel inside has been hidden. Reuses
    // loadMainShellDocks()'s exact fixture set (see that test's comment for
    // why); only the assertions differ.
    CommandRegistry registry(nullptr);
    MapViewModel viewModel;

    DockLayoutController dockLayout;

    QmlConfig config;

    LogModel logModel(nullptr);

    GroupModel groupModel;
    GroupProxyModel groupProxy;
    groupProxy.setSourceModel(&groupModel);
    GroupControllerStub groupController(nullptr);

    RoomManager roomManager(nullptr);
    RoomModel roomModel(nullptr, roomManager.getRoom());

    GameObserver gameObserver;
    AdventureTracker adventureTracker(gameObserver, nullptr);
    AdventureLogModel adventureLogModel(adventureTracker, nullptr);
    XpStatusAdapter xpStatusAdapter(adventureTracker, nullptr);
    MumeClock mumeClock(/*mumeEpoch=*/0, gameObserver, nullptr);
    ClockAdapter clockAdapter(gameObserver, mumeClock, nullptr);
    ToolbarLayoutController toolbarLayout;
    AudioVolumeController musicVolume(AudioVolumeController::AudioType::Music);
    AudioVolumeController soundVolume(AudioVolumeController::AudioType::Sound);

    MediaLibrary mediaLibrary;
    DescriptionAdapter descriptionAdapter(mediaLibrary, nullptr);

    CTimers timers(nullptr);
    TimerModel timerModel(timers, nullptr);
    TimerController timerController(timers, timerModel, nullptr);

    TasksModel tasksModel;

    ClientLineModel clientLineModel;
    HotkeyManager hotkeys;
    hotkeys.resetToDefaults();
    ClientController clientController(clientLineModel, hotkeys, nullptr);
    auto backend = std::make_unique<FakeBackend>();
    clientController.setBackend(std::move(backend));

    QQmlEngine engine;
    engine.addImageProvider(QStringLiteral("description"),
                            new DescriptionImageProvider(descriptionAdapter.getStore()));

    engine.rootContext()->setContextProperty("commands", &registry);
    engine.rootContext()->setContextProperty("mapCore", QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty("mapViewModel", &viewModel);
    engine.rootContext()->setContextProperty("statusText", QStringLiteral("test status"));
    engine.rootContext()->setContextProperty("dockLayout", &dockLayout);
    engine.rootContext()->setContextProperty("config", &config);
    engine.rootContext()->setContextProperty("logModel", &logModel);
    engine.rootContext()->setContextProperty("groupModel", &groupModel);
    engine.rootContext()->setContextProperty("groupProxyModel", &groupProxy);
    engine.rootContext()->setContextProperty("groupController", &groupController);
    engine.rootContext()->setContextProperty("roomModel", &roomModel);
    engine.rootContext()->setContextProperty("adventureLogModel", &adventureLogModel);
    engine.rootContext()->setContextProperty("adapter", &descriptionAdapter);
    engine.rootContext()->setContextProperty("timerModel", &timerModel);
    engine.rootContext()->setContextProperty("timerController", &timerController);
    engine.rootContext()->setContextProperty("tasksModel", &tasksModel);
    engine.rootContext()->setContextProperty("toolbarLayout", &toolbarLayout);
    engine.rootContext()->setContextProperty("mapZoom", QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty("musicVolume", &musicVolume);
    engine.rootContext()->setContextProperty("soundVolume", &soundVolume);
    engine.rootContext()->setContextProperty("clock", &clockAdapter);
    engine.rootContext()->setContextProperty("xpStatusAdapter", &xpStatusAdapter);
    engine.rootContext()->setContextProperty("clientController", &clientController);
    engine.rootContext()->setContextProperty("clientLineModel", &clientLineModel);

    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/MMapper/MainShell.qml"_qs));
    while (component.isLoading()) {
        QCoreApplication::processEvents();
    }
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> object(component.create(engine.rootContext()));
    QVERIFY(object != nullptr);
    QCoreApplication::processEvents();

    auto *const leftColumn = object->findChild<QQuickItem *>(QStringLiteral("leftColumn"));
    auto *const rightColumn = object->findChild<QQuickItem *>(QStringLiteral("rightColumn"));
    auto *const topRow = object->findChild<QQuickItem *>(QStringLiteral("topRow"));
    auto *const bottomRow = object->findChild<QQuickItem *>(QStringLiteral("bottomRow"));
    QVERIFY(leftColumn != nullptr);
    QVERIFY(rightColumn != nullptr);
    QVERIFY(topRow != nullptr);
    QVERIFY(bottomRow != nullptr);

    // Defaults: client/description/group visible (left/right/top columns
    // shown), log/room/adventure/tasks hidden (bottom row starts
    // collapsed).
    QCOMPARE(leftColumn->isVisible(), true);
    QCOMPARE(rightColumn->isVisible(), true);
    QCOMPARE(topRow->isVisible(), true);
    QCOMPARE(bottomRow->isVisible(), false);

    // Hiding every panel in the right column (description + timers, timers
    // already hidden by default) must collapse the column itself so
    // SplitView.fillWidth on the center MapView can reclaim its width --
    // the bug report's core complaint.
    dockLayout.setProperty("descriptionVisible", false);
    QCoreApplication::processEvents();
    QCOMPARE(rightColumn->isVisible(), false);

    // Showing it again brings the column back.
    dockLayout.setProperty("descriptionVisible", true);
    QCoreApplication::processEvents();
    QCOMPARE(rightColumn->isVisible(), true);

    // Same story for the single-panel left/top slots.
    dockLayout.setProperty("clientVisible", false);
    QCoreApplication::processEvents();
    QCOMPARE(leftColumn->isVisible(), false);

    dockLayout.setProperty("groupVisible", false);
    QCoreApplication::processEvents();
    QCOMPARE(topRow->isVisible(), false);

    // The bottom row (4 panels) only appears once at least one of them is
    // shown.
    dockLayout.setProperty("logVisible", true);
    QCoreApplication::processEvents();
    QCOMPARE(bottomRow->isVisible(), true);
    dockLayout.setProperty("logVisible", false);
    QCoreApplication::processEvents();
    QCOMPARE(bottomRow->isVisible(), false);
}

void TestQml::dockFloatingWindowLifecycle()
{
    // Covers the "panels cannot pop out (float)" bug report:
    // DockLayoutController's xFloating property (../src/qml/
    // DockLayoutController.h) now drives MainShell.qml's per-dock
    // FloatingDock Loader (see its inline-component comment) -- flipping
    // one to true hides the docked DockPanel and spins up a real
    // QtQuick.Window (offscreen-backend software rendering creates a real
    // QQuickWindow-derived object without GL, same as every other loadX()
    // test in this file that instantiates Item/Window-derived QML), and
    // closing that floating window's native close affordance flips
    // xVisible back to false (DockPanel.qml's own close button already
    // covered by loadMainShellDocks()'s onCloseRequested wiring). Reuses
    // loadMainShellDocks()'s exact fixture set.
    CommandRegistry registry(nullptr);
    MapViewModel viewModel;

    DockLayoutController dockLayout;

    QmlConfig config;

    LogModel logModel(nullptr);

    GroupModel groupModel;
    GroupProxyModel groupProxy;
    groupProxy.setSourceModel(&groupModel);
    GroupControllerStub groupController(nullptr);

    RoomManager roomManager(nullptr);
    RoomModel roomModel(nullptr, roomManager.getRoom());

    GameObserver gameObserver;
    AdventureTracker adventureTracker(gameObserver, nullptr);
    AdventureLogModel adventureLogModel(adventureTracker, nullptr);
    XpStatusAdapter xpStatusAdapter(adventureTracker, nullptr);
    MumeClock mumeClock(/*mumeEpoch=*/0, gameObserver, nullptr);
    ClockAdapter clockAdapter(gameObserver, mumeClock, nullptr);
    ToolbarLayoutController toolbarLayout;
    AudioVolumeController musicVolume(AudioVolumeController::AudioType::Music);
    AudioVolumeController soundVolume(AudioVolumeController::AudioType::Sound);

    MediaLibrary mediaLibrary;
    DescriptionAdapter descriptionAdapter(mediaLibrary, nullptr);

    CTimers timers(nullptr);
    TimerModel timerModel(timers, nullptr);
    TimerController timerController(timers, timerModel, nullptr);

    TasksModel tasksModel;

    ClientLineModel clientLineModel;
    HotkeyManager hotkeys;
    hotkeys.resetToDefaults();
    ClientController clientController(clientLineModel, hotkeys, nullptr);
    auto backend = std::make_unique<FakeBackend>();
    clientController.setBackend(std::move(backend));

    QQmlEngine engine;
    engine.addImageProvider(QStringLiteral("description"),
                            new DescriptionImageProvider(descriptionAdapter.getStore()));

    engine.rootContext()->setContextProperty("commands", &registry);
    engine.rootContext()->setContextProperty("mapCore", QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty("mapViewModel", &viewModel);
    engine.rootContext()->setContextProperty("statusText", QStringLiteral("test status"));
    engine.rootContext()->setContextProperty("dockLayout", &dockLayout);
    engine.rootContext()->setContextProperty("config", &config);
    engine.rootContext()->setContextProperty("logModel", &logModel);
    engine.rootContext()->setContextProperty("groupModel", &groupModel);
    engine.rootContext()->setContextProperty("groupProxyModel", &groupProxy);
    engine.rootContext()->setContextProperty("groupController", &groupController);
    engine.rootContext()->setContextProperty("roomModel", &roomModel);
    engine.rootContext()->setContextProperty("adventureLogModel", &adventureLogModel);
    engine.rootContext()->setContextProperty("adapter", &descriptionAdapter);
    engine.rootContext()->setContextProperty("timerModel", &timerModel);
    engine.rootContext()->setContextProperty("timerController", &timerController);
    engine.rootContext()->setContextProperty("tasksModel", &tasksModel);
    engine.rootContext()->setContextProperty("toolbarLayout", &toolbarLayout);
    engine.rootContext()->setContextProperty("mapZoom", QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty("musicVolume", &musicVolume);
    engine.rootContext()->setContextProperty("soundVolume", &soundVolume);
    engine.rootContext()->setContextProperty("clock", &clockAdapter);
    engine.rootContext()->setContextProperty("xpStatusAdapter", &xpStatusAdapter);
    engine.rootContext()->setContextProperty("clientController", &clientController);
    engine.rootContext()->setContextProperty("clientLineModel", &clientLineModel);

    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/MMapper/MainShell.qml"_qs));
    while (component.isLoading()) {
        QCoreApplication::processEvents();
    }
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> object(component.create(engine.rootContext()));
    QVERIFY(object != nullptr);
    QCoreApplication::processEvents();

    auto *const dockDescription = object->findChild<QQuickItem *>(QStringLiteral("dockDescription"));
    auto *const floatDescription = object->findChild<QQuickItem *>(
        QStringLiteral("floatDescription"));
    QVERIFY(dockDescription != nullptr);
    QVERIFY(floatDescription != nullptr);

    // Not floating yet: docked panel shown, no floating Window spun up.
    QCOMPARE(dockDescription->isVisible(), true);
    QCOMPARE(floatDescription->property("active").toBool(), false);
    QCOMPARE(floatDescription->property("item").value<QObject *>(), nullptr);

    // Floating it removes "description" from rightDockIds (see
    // DockLayoutController::recomputeAreaLists(), which excludes floating
    // docks), so reconcileDocks() DESTROYS the docked DockPanel in the right
    // column and the FloatingDock spins up the floating Window instead.
    // (dockDescription is dangling after this point -- don't dereference it.)
    dockLayout.setProperty("descriptionFloating", true);
    QTRY_COMPARE(object->findChild<QObject *>(QStringLiteral("dockDescription")), nullptr);
    QCOMPARE(floatDescription->property("active").toBool(), true);
    auto *const floatWindowObj = floatDescription->property("item").value<QObject *>();
    QVERIFY(floatWindowObj != nullptr);
    auto *const floatWindow = qobject_cast<QQuickWindow *>(floatWindowObj);
    QVERIFY(floatWindow != nullptr);
    QCOMPARE(floatWindow->title(), QStringLiteral("Description Panel"));

    // Closing the floating window (its native close affordance, not
    // DockPanel's own close button inside it) must flip descriptionVisible
    // back to false, same as clicking DockPanel's close button would --
    // sent as a real QCloseEvent rather than QQuickWindow::close(), which
    // this offscreen-backend test environment does not reliably route
    // through QWindow::closeEvent()/the "closing" signal the way a real
    // platform window's close (titlebar X, Alt+F4, ...) does.
    QCloseEvent closeEvent;
    QCoreApplication::sendEvent(floatWindow, &closeEvent);
    QCoreApplication::processEvents();

    QCOMPARE(dockLayout.property("descriptionVisible").toBool(), false);
    // ...which in turn tears the floating Window back down (active follows
    // xVisible && xFloating).
    QCOMPARE(floatDescription->property("active").toBool(), false);
}

void TestQml::loadMainShellChrome()
{
    // Exercises the menu/toolbar/statusbar chrome added alongside
    // ToolbarLayoutController/MapZoomController/AudioVolumeController (see
    // ../src/qml/ToolbarLayoutController.h, ../src/display/MapZoomController.h,
    // ../src/mainwindow/AudioVolumeController.h) on top of
    // loadMainShellDocks()'s panel fixture set -- MainShell.qml builds all
    // of that unconditionally regardless of which part of the shell a given
    // test cares about, so every context property has to be present here
    // too.
    CommandRegistry registry(nullptr);
    static constexpr std::array kLiveIds{
        "mouse-mode.move",
        "mouse-mode.room-raypick",
        "mouse-mode.room-select",
        "mouse-mode.connection-select",
        "mouse-mode.create-room",
        "mouse-mode.create-connection",
        "mouse-mode.create-oneway-connection",
        "mouse-mode.infomark-select",
        "mouse-mode.create-infomark",
        "world.rebuild-meshes",
    };
    for (const char *const id : kLiveIds) {
        const bool checkable = QByteArray(id).startsWith("mouse-mode.");
        UiCommand *const cmd = registry.addCommand(QString::fromLatin1(id), checkable);
        if (checkable) {
            registry.addToGroup(cmd, QStringLiteral("mouse-mode"), true);
        }
        // Every ToolButton bound to a command gets its text from
        // cmd.text (see CommandAction.qml); give each live command a
        // distinct, real label (matching QmlShellWindow.cpp's
        // ALL_COMMAND_SPECS) so the "find the button by its text" lookup
        // below is unambiguous -- an unset UiCommand::text defaults to "",
        // which every other (disabled/unregistered) command's button would
        // also show.
        cmd->setText(QString::fromLatin1(id));
    }
    registry.command(QStringLiteral("mouse-mode.move"))->setChecked(true);

    // view.show-menu-bar -- covers the menuBar.visible binding
    // QmlShellWindow.cpp/MainShell.qml wire this command to (see this file's
    // assertions after `object` is constructed below).
    UiCommand *const showMenuBarCmd = registry.addCommand(QStringLiteral("view.show-menu-bar"),
                                                          true);
    showMenuBarCmd->setChecked(true);

    MapViewModel viewModel;
    DockLayoutController dockLayout;
    ToolbarLayoutController toolbarLayout;
    AudioVolumeController musicVolume(AudioVolumeController::AudioType::Music);
    AudioVolumeController soundVolume(AudioVolumeController::AudioType::Sound);

    QmlConfig config;
    LogModel logModel(nullptr);

    GroupModel groupModel;
    GroupProxyModel groupProxy;
    groupProxy.setSourceModel(&groupModel);
    GroupControllerStub groupController(nullptr);

    RoomManager roomManager(nullptr);
    RoomModel roomModel(nullptr, roomManager.getRoom());

    GameObserver gameObserver;
    AdventureTracker adventureTracker(gameObserver, nullptr);
    AdventureLogModel adventureLogModel(adventureTracker, nullptr);
    XpStatusAdapter xpStatusAdapter(adventureTracker, nullptr);
    MumeClock mumeClock(/*mumeEpoch=*/0, gameObserver, nullptr);
    ClockAdapter clockAdapter(gameObserver, mumeClock, nullptr);

    MediaLibrary mediaLibrary;
    DescriptionAdapter descriptionAdapter(mediaLibrary, nullptr);

    CTimers timers(nullptr);
    TimerModel timerModel(timers, nullptr);
    TimerController timerController(timers, timerModel, nullptr);

    TasksModel tasksModel;

    ClientLineModel clientLineModel;
    HotkeyManager hotkeys;
    hotkeys.resetToDefaults();
    ClientController clientController(clientLineModel, hotkeys, nullptr);
    auto backend = std::make_unique<FakeBackend>();
    clientController.setBackend(std::move(backend));

    QQmlEngine engine;
    engine.addImageProvider(QStringLiteral("description"),
                            new DescriptionImageProvider(descriptionAdapter.getStore()));

    engine.rootContext()->setContextProperty("commands", &registry);
    engine.rootContext()->setContextProperty("mapCore", QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty("mapViewModel", &viewModel);
    engine.rootContext()->setContextProperty("statusText", QStringLiteral("test status"));
    engine.rootContext()->setContextProperty("dockLayout", &dockLayout);
    engine.rootContext()->setContextProperty("config", &config);
    engine.rootContext()->setContextProperty("logModel", &logModel);
    engine.rootContext()->setContextProperty("groupModel", &groupModel);
    engine.rootContext()->setContextProperty("groupProxyModel", &groupProxy);
    engine.rootContext()->setContextProperty("groupController", &groupController);
    engine.rootContext()->setContextProperty("roomModel", &roomModel);
    engine.rootContext()->setContextProperty("adventureLogModel", &adventureLogModel);
    engine.rootContext()->setContextProperty("adapter", &descriptionAdapter);
    engine.rootContext()->setContextProperty("timerModel", &timerModel);
    engine.rootContext()->setContextProperty("timerController", &timerController);
    engine.rootContext()->setContextProperty("tasksModel", &tasksModel);
    engine.rootContext()->setContextProperty("clientController", &clientController);
    engine.rootContext()->setContextProperty("clientLineModel", &clientLineModel);
    engine.rootContext()->setContextProperty("toolbarLayout", &toolbarLayout);
    engine.rootContext()->setContextProperty("mapZoom", QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty("musicVolume", &musicVolume);
    engine.rootContext()->setContextProperty("soundVolume", &soundVolume);
    engine.rootContext()->setContextProperty("clock", &clockAdapter);
    engine.rootContext()->setContextProperty("xpStatusAdapter", &xpStatusAdapter);

    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/MMapper/MainShell.qml"_qs));
    while (component.isLoading()) {
        QCoreApplication::processEvents();
    }
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> object(component.create(engine.rootContext()));
    QVERIFY(object != nullptr);
    QCoreApplication::processEvents();

    // --- menu structure spot-checks ---
    QObject *const fileExportMenu = object->findChild<QObject *>(QStringLiteral("fileExportMenu"));
    QVERIFY(fileExportMenu != nullptr);

    QObject *const editRoomsMenu = object->findChild<QObject *>(QStringLiteral("editRoomsMenu"));
    QVERIFY(editRoomsMenu != nullptr);
    // "&Rooms" mirrors mainwindow.cpp's editMenu->addMenu(...->tr("&Rooms")).
    QCOMPARE(editRoomsMenu->property("title").toString(), QStringLiteral("&Rooms"));

    // --- toolbar presence + a ToolButton triggering its bound command ---
    QObject *const mouseModeToolBar = object->findChild<QObject *>(
        QStringLiteral("mouseModeToolBar"));
    QVERIFY(mouseModeToolBar != nullptr);
    QVERIFY(!mouseModeToolBar->property("visible").toBool());

    toolbarLayout.setProperty("mouseModeVisible", true);
    QCoreApplication::processEvents();
    QVERIFY(mouseModeToolBar->property("visible").toBool());

    UiCommand *const roomSelectCmd = registry.command(QStringLiteral("mouse-mode.room-select"));
    QVERIFY(roomSelectCmd != nullptr);
    QSignalSpy triggeredSpy(roomSelectCmd, &UiCommand::sig_triggered);

    // The ToolButton bound to mouse-mode.room-select is objectName-less (Qt
    // Quick Controls doesn't set one), so locate it by its bound text --
    // same approach loadRoomEditDialog() above uses for the exit D-pad's
    // South button.
    QQuickItem *roomSelectButton = nullptr;
    for (QQuickItem *const candidate : mouseModeToolBar->findChildren<QQuickItem *>()) {
        if (candidate->property("text").toString() == roomSelectCmd->getText()) {
            roomSelectButton = candidate;
            break;
        }
    }
    QVERIFY2(roomSelectButton != nullptr, "did not find the mouse-mode.room-select ToolButton");
    // Trigger through the ToolButton's bound `action` (the CommandAction
    // instance from MainShell.qml), not by emitting the ToolButton's own
    // clicked() signal directly: QQC2's action-forwarding runs inside its
    // internal click() handler (mouse press/release state machine), which a
    // bare clicked()-signal emission bypasses -- this instead exercises the
    // actual integration point this shell owns (the ToolButton's `action:
    // CommandAction { cmd: ... }` binding), not Qt Quick Controls' own
    // press-forwarding, which is out of scope to test here.
    QObject *const action = roomSelectButton->property("action").value<QObject *>();
    QVERIFY(action != nullptr);
    QMetaObject::invokeMethod(action, "trigger");
    QCoreApplication::processEvents();
    QCOMPARE(triggeredSpy.count(), 1);

    // --- zoom slider present (see MapZoomController.h's file comment for
    // why mapZoom itself is a null stub here rather than a real controller) ---
    QObject *const zoomSlider = object->findChild<QObject *>(QStringLiteral("zoomSlider"));
    QVERIFY(zoomSlider != nullptr);

    // --- audio volume sliders present and bound to the real controllers ---
    auto *const musicSlider = object->findChild<QQuickItem *>(QStringLiteral("musicVolumeSlider"));
    QVERIFY(musicSlider != nullptr);
    musicVolume.setVolume(42);
    QCoreApplication::processEvents();
    QCOMPARE(musicSlider->property("value").toInt(), 42);

    // --- statusbar items exist ---
    QVERIFY(object->findChild<QObject *>(QStringLiteral("statusLabel")) != nullptr);
    QVERIFY(object->findChild<QObject *>(QStringLiteral("pathMachineLabel")) != nullptr);
    QVERIFY(object->findChild<QObject *>(QStringLiteral("clockStrip")) != nullptr);
    QVERIFY(object->findChild<QObject *>(QStringLiteral("xpStatusItem")) != nullptr);

    // --- menuBar.visible follows the view.show-menu-bar command's checked
    // state (see MainShell.qml's `menuBar: QQC2.MenuBar { visible: ... }`
    // binding and QmlShellWindow.cpp's view.show-menu-bar wiring) ---
    QObject *const menuBarObj = object->property("menuBar").value<QObject *>();
    QVERIFY(menuBarObj != nullptr);
    QVERIFY(menuBarObj->property("visible").toBool());

    showMenuBarCmd->setChecked(false);
    QCoreApplication::processEvents();
    QVERIFY(!menuBarObj->property("visible").toBool());

    showMenuBarCmd->setChecked(true);
    QCoreApplication::processEvents();
    QVERIFY(menuBarObj->property("visible").toBool());
}

namespace {
// Registers the command ids MapContextMenu.qml (../src/qml/shell/
// MapContextMenu.qml) binds against via CommandAction, mirroring the subset
// QmlShellWindow.cpp's registerCommands()/wireSelectionCommands() wire live
// (see that file's ALL_COMMAND_SPECS/isLiveCommand()). Shared by
// loadMapContextMenu()/mapContextMenuSelectionSections() below.
void registerMapContextMenuCommands(CommandRegistry &registry)
{
    for (const char *const id : {"room.create",
                                 "room.edit-selected",
                                 "room.move-up-selected",
                                 "room.move-down-selected",
                                 "room.merge-up-selected",
                                 "room.merge-down-selected",
                                 "room.delete-selected",
                                 "room.connect-to-neighbours",
                                 "room.goto-selected",
                                 "room.force-update-selected",
                                 "connection.delete-selected",
                                 "infomark.edit-selected",
                                 "infomark.delete-selected"}) {
        std::ignore = registry.addCommand(QString::fromLatin1(id));
    }
    for (const char *const id : {"mouse-mode.move",
                                 "mouse-mode.room-raypick",
                                 "mouse-mode.room-select",
                                 "mouse-mode.infomark-select",
                                 "mouse-mode.connection-select",
                                 "mouse-mode.create-infomark",
                                 "mouse-mode.create-room",
                                 "mouse-mode.create-connection",
                                 "mouse-mode.create-oneway-connection"}) {
        std::ignore = registry.addCommand(QString::fromLatin1(id), true);
    }
}
} // namespace

void TestQml::loadMapContextMenu()
{
    // MapContextMenu.qml (../src/qml/shell/MapContextMenu.qml) is the map
    // canvas's right-click menu, mirroring MainWindow::slot_showContextMenu()
    // (mainwindow.cpp). This test drives it with a real CommandRegistry (see
    // registerMapContextMenuCommands() above) and a ShellSelectionStub
    // standing in for QmlShellWindow's "shell" context property (see that
    // stub's comment above for why QmlShellWindow itself isn't linkable
    // here). Default state: no selection of any kind, so only the trailing
    // "Mouse Mode" submenu should be present/visible.
    CommandRegistry registry(nullptr);
    registerMapContextMenuCommands(registry);
    ShellSelectionStub shell;
    MapCoreContextMenuStub core;

    QQmlEngine engine;
    engine.rootContext()->setContextProperty("commands", &registry);
    engine.rootContext()->setContextProperty("shell", &shell);

    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/MMapper/MapContextMenu.qml"_qs));
    while (component.isLoading()) {
        QCoreApplication::processEvents();
    }
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> object(component.create(engine.rootContext()));
    QVERIFY(object != nullptr);
    object->setProperty("core", QVariant::fromValue<QObject *>(&core));
    QCoreApplication::processEvents();

    QObject *const mouseModeMenu = object->findChild<QObject *>(QStringLiteral("ctxMouseModeMenu"));
    QVERIFY(mouseModeMenu != nullptr);
    QCOMPARE(mouseModeMenu->property("title").toString(), QStringLiteral("Mouse Mode"));

    // Every conditional item must exist in the object tree regardless of
    // section state (see MapContextMenu.qml: each one is a plain always-
    // instantiated child with a `visible:` binding, not a Loader/Repeater),
    // and with no selection of any kind, every section-driving property on
    // the root Menu must read false.
    for (const char *const name : {"ctxDeleteConnection",
                                   "ctxCreateRoom",
                                   "ctxEditRoom",
                                   "ctxMoveUpRoom",
                                   "ctxMoveDownRoom",
                                   "ctxMergeUpRoom",
                                   "ctxMergeDownRoom",
                                   "ctxDeleteRoom",
                                   "ctxConnectToNeighbours",
                                   "ctxRoomOpsSeparator",
                                   "ctxGotoRoom",
                                   "ctxForceUpdateRoom",
                                   "ctxInfomarkSeparator",
                                   "ctxEditInfomark",
                                   "ctxDeleteInfomark"}) {
        QVERIFY2(object->findChild<QObject *>(QString::fromLatin1(name)) != nullptr, name);
    }
    QVERIFY(!object->property("showCreateRoom").toBool());
    QVERIFY(!object->property("showRoomOps").toBool());
    QVERIFY(!object->property("showInfomarkOps").toBool());
    QVERIFY(!object->property("hasConnectionSelection").toBool());

    // Emitting the underlying canvas's dismiss signal must not crash even
    // though the menu was never actually popped up (regression coverage for
    // the Connections wiring itself, not for QQC2.Popup's own geometry
    // logic under the offscreen platform).
    emit core.sig_dismissContextMenu();
    QCoreApplication::processEvents();
}

void TestQml::mapContextMenuSelectionSections()
{
    // Drives ShellSelectionStub through the same selection combinations
    // MainWindow::slot_showContextMenu() branches on (mainwindow.cpp),
    // asserting MapContextMenu.qml's showCreateRoom/showRoomOps/
    // showInfomarkOps/hasConnectionSelection properties -- each
    // conditional MenuItem's own `visible:` binding is a direct one-line
    // expression of exactly one of these (see the QML file), so this is a
    // faithful proxy for "the right section is visible" without depending
    // on QQC2.Menu/Popup's own item-visibility machinery, which (being a
    // Popup that is never actually opened here, under the offscreen
    // platform) does not reliably reflect a plain `visible:` binding on an
    // unopened menu's contentData children.
    CommandRegistry registry(nullptr);
    registerMapContextMenuCommands(registry);
    ShellSelectionStub shell;

    QQmlEngine engine;
    engine.rootContext()->setContextProperty("commands", &registry);
    engine.rootContext()->setContextProperty("shell", &shell);

    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/MMapper/MapContextMenu.qml"_qs));
    while (component.isLoading()) {
        QCoreApplication::processEvents();
    }
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> object(component.create(engine.rootContext()));
    QVERIFY(object != nullptr);
    QCoreApplication::processEvents();

    auto sectionProp = [&object](const char *const name) { return object->property(name).toBool(); };

    // --- empty room selection -> only room.create ---
    shell.setRoomSelection(/*has=*/true, /*empty=*/true);
    QCoreApplication::processEvents();
    QVERIFY(sectionProp("showCreateRoom"));
    QVERIFY(!sectionProp("showRoomOps"));

    // --- non-empty room selection -> the full room operation set ---
    shell.setRoomSelection(/*has=*/true, /*empty=*/false);
    QCoreApplication::processEvents();
    QVERIFY(!sectionProp("showCreateRoom"));
    QVERIFY(sectionProp("showRoomOps"));
    // No infomark selection yet -> that section stays hidden.
    QVERIFY(!sectionProp("showInfomarkOps"));

    // --- infomark selection alongside the room selection -> both sections
    // shown together (mirrors slot_showContextMenu()'s
    // `if (m_infoMarkSelection != nullptr && !m_infoMarkSelection->empty())`
    // branch, which can fire alongside the room branch above it) ---
    shell.setInfomarkSelection(/*has=*/true, /*empty=*/false);
    QCoreApplication::processEvents();
    QVERIFY(sectionProp("showRoomOps"));
    QVERIFY(sectionProp("showInfomarkOps"));

    // An empty infomark selection (freshly drawn, about to auto-open the
    // editor -- see QmlShellWindow.cpp's sig_newInfomarkSelection handler)
    // must NOT show the infomark section, mirroring slot_showContextMenu()'s
    // `!m_infoMarkSelection->empty()` guard.
    shell.setInfomarkSelection(/*has=*/true, /*empty=*/true);
    QCoreApplication::processEvents();
    QVERIFY(!sectionProp("showInfomarkOps"));
    shell.setInfomarkSelection(/*has=*/true, /*empty=*/false);
    QCoreApplication::processEvents();

    // --- a connection selection forecloses the room/infomark sections
    // entirely, mirroring slot_showContextMenu()'s
    // `if (m_connectionSelection != nullptr) ... else { ... }` ---
    shell.setConnectionSelection(/*has=*/true);
    QCoreApplication::processEvents();
    QVERIFY(sectionProp("hasConnectionSelection"));
    QVERIFY(!sectionProp("showRoomOps"));
    QVERIFY(!sectionProp("showCreateRoom"));
    QVERIFY(!sectionProp("showInfomarkOps"));

    // --- clearing every selection collapses back to nothing but the
    // trailing Mouse Mode submenu ---
    shell.setConnectionSelection(/*has=*/false);
    shell.setRoomSelection(/*has=*/false, /*empty=*/true);
    shell.setInfomarkSelection(/*has=*/false, /*empty=*/true);
    QCoreApplication::processEvents();
    QVERIFY(!sectionProp("hasConnectionSelection"));
    QVERIFY(!sectionProp("showCreateRoom"));
    QVERIFY(!sectionProp("showRoomOps"));
    QVERIFY(!sectionProp("showInfomarkOps"));
}

void TestQml::mainShellEscapeShortcutForwards()
{
    // MainShell.qml's Shortcut{sequence: "Escape"} (see
    // ../src/qml/shell/MainShell.qml) forwards Escape to the canvas core's
    // userPressedEscape() Q_INVOKABLE (see ../src/display/MapCanvasCore.h),
    // mirroring MapWindow::keyPressEvent()'s Qt::Key_Escape handling in the
    // widget shell. A real MapCanvasCore needs OpenGL (see loadMainShell()'s
    // comment on why this small binary never links it); MapCoreEscapeStub
    // above stands in for it, exposing only the one invokable under test.
    CommandRegistry registry(nullptr);
    MapViewModel viewModel;
    ToolbarLayoutController toolbarLayout;
    AudioVolumeController musicVolume(AudioVolumeController::AudioType::Music);
    AudioVolumeController soundVolume(AudioVolumeController::AudioType::Sound);
    GameObserver gameObserver;
    MumeClock mumeClock(/*mumeEpoch=*/0, gameObserver, nullptr);
    ClockAdapter clockAdapter(gameObserver, mumeClock, nullptr);
    AdventureTracker adventureTracker(gameObserver, nullptr);
    XpStatusAdapter xpStatusAdapter(adventureTracker, nullptr);
    MapCoreEscapeStub mapCoreStub;

    QQmlEngine engine;
    engine.rootContext()->setContextProperty("commands", &registry);
    engine.rootContext()->setContextProperty("mapCore", &mapCoreStub);
    engine.rootContext()->setContextProperty("mapViewModel", &viewModel);
    engine.rootContext()->setContextProperty("statusText", QStringLiteral("test status"));
    engine.rootContext()->setContextProperty("toolbarLayout", &toolbarLayout);
    engine.rootContext()->setContextProperty("mapZoom", QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty("musicVolume", &musicVolume);
    engine.rootContext()->setContextProperty("soundVolume", &soundVolume);
    engine.rootContext()->setContextProperty("clock", &clockAdapter);
    engine.rootContext()->setContextProperty("xpStatusAdapter", &xpStatusAdapter);

    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/MMapper/MainShell.qml"_qs));
    while (component.isLoading()) {
        QCoreApplication::processEvents();
    }
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> object(component.create(engine.rootContext()));
    QVERIFY(object != nullptr);
    QCoreApplication::processEvents();

    auto *const window = qobject_cast<QQuickWindow *>(object.data());
    QVERIFY(window != nullptr);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QCOMPARE(mapCoreStub.callCount, 0);
    QTest::keyClick(window, Qt::Key_Escape);
    QCoreApplication::processEvents();
    QCOMPARE(mapCoreStub.callCount, 1);
    QCOMPARE(mapCoreStub.lastPressed, true);
}

void TestQml::mainShellCommandOpensAboutDialog()
{
    // Exercises the exact pattern QmlShellWindow.cpp's wireDialogCommands()
    // uses to wire the "help.about" command to a fresh AboutInfo + QmlDialog
    // (see slot_about()'s MMAPPER_WITH_QML branch in mainwindow.cpp, which
    // it mirrors): triggering the command must construct and load a working
    // dialog. QmlShellWindow itself isn't linkable into this small test
    // binary (it's compiled straight into the "mmapper" executable -- see
    // src/CMakeLists.txt's WITH_QML block comment -- because it touches
    // MapCanvasCore's OpenGL dependency graph), so this test reproduces the
    // command-trigger-opens-a-dialog wiring directly against a real
    // CommandRegistry/UiCommand pair instead, the same substitution
    // loadMainShell() above makes for the rest of QmlShellWindow's wiring.
    CommandRegistry registry(nullptr);
    UiCommand *const aboutCmd = registry.addCommand(QStringLiteral("help.about"), false);

    QPointer<QmlDialog> lastDialog;
    QObject::connect(aboutCmd, &UiCommand::sig_triggered, [&lastDialog]() {
        auto *const dialog = new QmlDialog(QStringLiteral("About MMapper"),
                                           QStringLiteral("AboutDialog"),
                                           nullptr);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        auto *const info = new AboutInfo(dialog);
        dialog->setContextProperty("aboutInfo", info);
        dialog->setQmlSource(QUrl(u"qrc:/qt/qml/MMapper/AboutDialog.qml"_qs));
        dialog->open();
        lastDialog = dialog;
    });

    QVERIFY(lastDialog.isNull());
    aboutCmd->trigger();
    QVERIFY(!lastDialog.isNull());

    QQuickWidget *const quick = lastDialog->quickWidget();
    QVERIFY(quick != nullptr);
    while (quick->status() == QQuickWidget::Loading) {
        QCoreApplication::processEvents();
    }
    QCoreApplication::processEvents();

    QCOMPARE(quick->status(), QQuickWidget::Ready);
    QVERIFY(quick->rootObject() != nullptr);
    QVERIFY(lastDialog->isVisible());

    lastDialog->close();
}

void TestQml::qmlShellSettingsPersistenceRoundTrip()
{
    // Configuration::QmlShellSettings (see ../src/configuration/
    // configuration.h's "qmlShell" group, added alongside this commit's
    // QmlShellWindow lifecycle work) persists Shell B's top-level window
    // geometry plus its 8 dock / 9 toolbar visibility flags, kept in a
    // separate key namespace from GeneralSettings::windowGeometry/
    // windowState so the widget shell and Shell B never clobber each
    // other's saved layout. This drives Configuration::writeTo()/
    // readFrom() (the same public round-trip Configuration::write()/read()
    // use for the default backing store) against a temporary ini file so
    // the test never touches the real user settings.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("qmlshell-roundtrip.ini"));

    auto &qmlShell = setConfig().qmlShell;
    const auto originalGeometry = qmlShell.geometry;
    const bool originalDockLogVisible = qmlShell.dockLogVisible;
    const bool originalDockGroupVisible = qmlShell.dockGroupVisible;
    const bool originalToolbarFileVisible = qmlShell.toolbarFileVisible;
    const bool originalToolbarAudioVisible = qmlShell.toolbarAudioVisible;
    const bool originalDockDescriptionFloating = qmlShell.dockDescriptionFloating;
    const auto originalDockDescriptionFloatGeometry = qmlShell.dockDescriptionFloatGeometry;
    auto cleanup = qScopeGuard([&]() {
        auto &restore = setConfig().qmlShell;
        restore.geometry = originalGeometry;
        restore.dockLogVisible = originalDockLogVisible;
        restore.dockGroupVisible = originalDockGroupVisible;
        restore.toolbarFileVisible = originalToolbarFileVisible;
        restore.toolbarAudioVisible = originalToolbarAudioVisible;
        restore.dockDescriptionFloating = originalDockDescriptionFloating;
        restore.dockDescriptionFloatGeometry = originalDockDescriptionFloatGeometry;
    });

    qmlShell.geometry = QByteArray("test-geometry-bytes");
    qmlShell.dockLogVisible = true;
    qmlShell.dockGroupVisible = false;
    qmlShell.toolbarFileVisible = true;
    qmlShell.toolbarAudioVisible = true;
    // Floating flag + float geometry -- added alongside DockLayoutController's
    // xFloating/xFloatGeometry properties (see ../src/qml/
    // DockLayoutController.h); the geometry itself is an opaque
    // QDataStream-encoded QRect (see QmlShellWindow.cpp's
    // encodeFloatGeometry()/decodeFloatGeometry()), so this round-trips the
    // raw bytes the same way the top-level window's own `geometry` field
    // does above rather than re-deriving a QRect here.
    qmlShell.dockDescriptionFloating = true;
    qmlShell.dockDescriptionFloatGeometry = QByteArray("test-float-geometry-bytes");

    {
        QSettings out(path, QSettings::IniFormat);
        getConfig().writeTo(out);
    }

    // Clobber the in-memory state so the read-back below actually proves
    // something.
    qmlShell.geometry.clear();
    qmlShell.dockLogVisible = false;
    qmlShell.dockGroupVisible = true;
    qmlShell.toolbarFileVisible = false;
    qmlShell.toolbarAudioVisible = false;
    qmlShell.dockDescriptionFloating = false;
    qmlShell.dockDescriptionFloatGeometry.clear();

    {
        QSettings in(path, QSettings::IniFormat);
        setConfig().readFrom(in);
    }

    QCOMPARE(getConfig().qmlShell.geometry, QByteArray("test-geometry-bytes"));
    QCOMPARE(getConfig().qmlShell.dockLogVisible, true);
    QCOMPARE(getConfig().qmlShell.dockGroupVisible, false);
    QCOMPARE(getConfig().qmlShell.toolbarFileVisible, true);
    QCOMPARE(getConfig().qmlShell.toolbarAudioVisible, true);
    QCOMPARE(getConfig().qmlShell.dockDescriptionFloating, true);
    QCOMPARE(getConfig().qmlShell.dockDescriptionFloatGeometry,
             QByteArray("test-float-geometry-bytes"));
}

void TestQml::ioTaskControllerLifecycle()
{
    // Pure-C++ coverage of IoTaskController (../src/mainwindow/
    // IoTaskController.h), QmlShellWindow's progress-popup state -- see its
    // begin()/update()/end()/cancel() doc comments. QmlShellWindow itself
    // isn't linkable into this test binary (see loadMainShell()'s comment),
    // so this exercises the controller directly the same way
    // pathMachineStatusFunnel() exercises a synthetic context property
    // update standing in for QmlShellWindow.cpp's real wiring.
    IoTaskController controller;
    QCOMPARE(controller.isActive(), false);
    QCOMPARE(controller.getPercent(), 0);
    QCOMPARE(controller.isCancelable(), false);

    QSignalSpy changedSpy(&controller, &IoTaskController::sig_changed);
    QSignalSpy cancelSpy(&controller, &IoTaskController::sig_cancelRequested);

    // begin() primes every property and is cancelable for a load/merge-like
    // task (mirrors beginIoTaskProgress()'s Allow case in QmlShellWindow.cpp).
    controller.begin(QStringLiteral("Loading map..."), /*cancelable=*/true);
    QCOMPARE(controller.isActive(), true);
    QCOMPARE(controller.getLabel(), QStringLiteral("Loading map..."));
    QCOMPARE(controller.getPercent(), 0);
    QCOMPARE(controller.isCancelable(), true);
    QCOMPARE(changedSpy.count(), 1);

    // update() mirrors tickIoTaskProgress()'s per-poll push.
    controller.update(QStringLiteral("load from disk..."), 42);
    QCOMPARE(controller.getLabel(), QStringLiteral("load from disk..."));
    QCOMPARE(controller.getPercent(), 42);
    QCOMPARE(changedSpy.count(), 2);

    // cancel() only fires sig_cancelRequested while cancelable, mirroring
    // the popup's Cancel button being enabled only for Allow-cancel tasks.
    controller.cancel();
    QCOMPARE(cancelSpy.count(), 1);

    // end() (mirrors endIoTaskProgress()) resets every property, including
    // cancelable -- a stale "cancelable" from a finished task must never
    // leak into the next one.
    controller.end();
    QCOMPARE(controller.isActive(), false);
    QCOMPARE(controller.getLabel(), QString());
    QCOMPARE(controller.getPercent(), 0);
    QCOMPARE(controller.isCancelable(), false);
    QCOMPARE(changedSpy.count(), 3);

    // A non-cancelable task (mirrors saveMapFile()'s AllowCancel::Forbid)
    // must never emit sig_cancelRequested from cancel().
    controller.begin(QStringLiteral("Saving map..."), /*cancelable=*/false);
    cancelSpy.clear();
    controller.cancel();
    QCOMPARE(cancelSpy.count(), 0);

    // update()/end() are no-ops while inactive (guards documented in
    // IoTaskController.cpp) -- verify update() after end() doesn't resurrect
    // the popup.
    controller.end();
    changedSpy.clear();
    controller.update(QStringLiteral("ignored"), 50);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(controller.isActive(), false);
}

void TestQml::mainShellIoProgressPopup()
{
    // Drives MainShell.qml's ioProgressPopup (added alongside
    // IoTaskController -- see its "ioTask" context property doc comment at
    // the top of MainShell.qml) with a real IoTaskController standing in
    // for QmlShellWindow's instance, reusing loadMainShellChrome()'s full
    // fixture set (MainShell.qml needs every context property present
    // regardless of which part of the shell a given test cares about).
    CommandRegistry registry(nullptr);
    MapViewModel viewModel;
    DockLayoutController dockLayout;
    ToolbarLayoutController toolbarLayout;
    AudioVolumeController musicVolume(AudioVolumeController::AudioType::Music);
    AudioVolumeController soundVolume(AudioVolumeController::AudioType::Sound);

    QmlConfig config;
    LogModel logModel(nullptr);

    GroupModel groupModel;
    GroupProxyModel groupProxy;
    groupProxy.setSourceModel(&groupModel);
    GroupControllerStub groupController(nullptr);

    RoomManager roomManager(nullptr);
    RoomModel roomModel(nullptr, roomManager.getRoom());

    GameObserver gameObserver;
    AdventureTracker adventureTracker(gameObserver, nullptr);
    AdventureLogModel adventureLogModel(adventureTracker, nullptr);
    XpStatusAdapter xpStatusAdapter(adventureTracker, nullptr);
    MumeClock mumeClock(/*mumeEpoch=*/0, gameObserver, nullptr);
    ClockAdapter clockAdapter(gameObserver, mumeClock, nullptr);

    MediaLibrary mediaLibrary;
    DescriptionAdapter descriptionAdapter(mediaLibrary, nullptr);

    CTimers timers(nullptr);
    TimerModel timerModel(timers, nullptr);
    TimerController timerController(timers, timerModel, nullptr);

    TasksModel tasksModel;

    ClientLineModel clientLineModel;
    HotkeyManager hotkeys;
    hotkeys.resetToDefaults();
    ClientController clientController(clientLineModel, hotkeys, nullptr);
    auto backend = std::make_unique<FakeBackend>();
    clientController.setBackend(std::move(backend));

    IoTaskController ioTask;

    QQmlEngine engine;
    engine.addImageProvider(QStringLiteral("description"),
                            new DescriptionImageProvider(descriptionAdapter.getStore()));

    engine.rootContext()->setContextProperty("commands", &registry);
    engine.rootContext()->setContextProperty("mapCore", QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty("mapViewModel", &viewModel);
    engine.rootContext()->setContextProperty("statusText", QStringLiteral("test status"));
    engine.rootContext()->setContextProperty("dockLayout", &dockLayout);
    engine.rootContext()->setContextProperty("config", &config);
    engine.rootContext()->setContextProperty("logModel", &logModel);
    engine.rootContext()->setContextProperty("groupModel", &groupModel);
    engine.rootContext()->setContextProperty("groupProxyModel", &groupProxy);
    engine.rootContext()->setContextProperty("groupController", &groupController);
    engine.rootContext()->setContextProperty("roomModel", &roomModel);
    engine.rootContext()->setContextProperty("adventureLogModel", &adventureLogModel);
    engine.rootContext()->setContextProperty("adapter", &descriptionAdapter);
    engine.rootContext()->setContextProperty("timerModel", &timerModel);
    engine.rootContext()->setContextProperty("timerController", &timerController);
    engine.rootContext()->setContextProperty("tasksModel", &tasksModel);
    engine.rootContext()->setContextProperty("clientController", &clientController);
    engine.rootContext()->setContextProperty("clientLineModel", &clientLineModel);
    engine.rootContext()->setContextProperty("toolbarLayout", &toolbarLayout);
    engine.rootContext()->setContextProperty("mapZoom", QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty("musicVolume", &musicVolume);
    engine.rootContext()->setContextProperty("soundVolume", &soundVolume);
    engine.rootContext()->setContextProperty("clock", &clockAdapter);
    engine.rootContext()->setContextProperty("xpStatusAdapter", &xpStatusAdapter);
    engine.rootContext()->setContextProperty("ioTask", &ioTask);

    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/MMapper/MainShell.qml"_qs));
    while (component.isLoading()) {
        QCoreApplication::processEvents();
    }
    QVERIFY2(!component.isError(), qPrintable(component.errorString()));

    QScopedPointer<QObject> object(component.create(engine.rootContext()));
    QVERIFY(object != nullptr);
    QCoreApplication::processEvents();

    auto *const popup = object->findChild<QObject *>(QStringLiteral("ioProgressPopup"));
    QVERIFY(popup != nullptr);
    auto *const label = object->findChild<QObject *>(QStringLiteral("ioProgressLabel"));
    QVERIFY(label != nullptr);
    auto *const bar = object->findChild<QObject *>(QStringLiteral("ioProgressBar"));
    QVERIFY(bar != nullptr);
    auto *const cancelButton = object->findChild<QObject *>(
        QStringLiteral("ioProgressCancelButton"));
    QVERIFY(cancelButton != nullptr);

    // Starts hidden, matching IoTaskController's default-constructed
    // inactive state.
    QCOMPARE(popup->property("visible").toBool(), false);

    // Starting a cancelable task (mirrors loadFile()'s beginIoTaskProgress())
    // must show the popup with the task's label/percent and an enabled
    // Cancel button.
    ioTask.begin(QStringLiteral("Loading map..."), /*cancelable=*/true);
    QCoreApplication::processEvents();
    QCOMPARE(popup->property("visible").toBool(), true);
    QCOMPARE(label->property("text").toString(), QStringLiteral("Loading map..."));
    QCOMPARE(bar->property("value").toInt(), 0);
    QCOMPARE(cancelButton->property("enabled").toBool(), true);

    // A progress update (mirrors tickIoTaskProgress()) must reach the label
    // and ProgressBar.
    ioTask.update(QStringLiteral("load from disk..."), 55);
    QCoreApplication::processEvents();
    QCOMPARE(label->property("text").toString(), QStringLiteral("load from disk..."));
    QCOMPARE(bar->property("value").toInt(), 55);

    // Clicking Cancel must reach IoTaskController::cancel(), which emits
    // sig_cancelRequested only because this task is cancelable -- QML has no
    // direct way to invoke a Button's onClicked, so this calls through
    // QMetaObject the same way CommandAction.qml's onTriggered would invoke
    // a bound command.
    QSignalSpy cancelSpy(&ioTask, &IoTaskController::sig_cancelRequested);
    QVERIFY(QMetaObject::invokeMethod(cancelButton, "clicked"));
    QCOMPARE(cancelSpy.count(), 1);

    // A non-cancelable task (mirrors saveMapFile()'s beginIoTaskProgress())
    // must show a disabled Cancel button.
    ioTask.end();
    QCoreApplication::processEvents();
    QCOMPARE(popup->property("visible").toBool(), false);
    ioTask.begin(QStringLiteral("Saving map..."), /*cancelable=*/false);
    QCoreApplication::processEvents();
    QCOMPARE(popup->property("visible").toBool(), true);
    QCOMPARE(cancelButton->property("enabled").toBool(), false);

    // Finishing the task (mirrors endIoTaskProgress()) hides the popup
    // again.
    ioTask.end();
    QCoreApplication::processEvents();
    QCOMPARE(popup->property("visible").toBool(), false);
}

QTEST_MAIN(TestQml)

#include "TestQml.moc"
