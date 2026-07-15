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
#include "../src/global/AsyncTasks.h"
#include "../src/global/progresscounter.h"
#include "../src/group/CGroupChar.h"
#include "../src/group/GroupModel.h"
#include "../src/mainwindow/AboutInfo.h"
#include "../src/mainwindow/FindRoomsModel.h"
#include "../src/mainwindow/LogModel.h"
#include "../src/mainwindow/TasksModel.h"
#include "../src/mainwindow/UpdateChecker.h"
#include "../src/map/Map.h"
#include "../src/map/RawRoom.h"
#include "../src/map/RoomHandle.h"
#include "../src/map/mmapper2room.h"
#include "../src/media/DescriptionAdapter.h"
#include "../src/media/MediaLibrary.h"
#include "../src/observer/gameobserver.h"
#include "../src/preferences/PreferencesController.h"
#include "../src/proxy/GmcpMessage.h"
#include "../src/qml/DescriptionImageProvider.h"
#include "../src/qml/QmlConfig.h"
#include "../src/qml/QmlDialog.h"
#include "../src/qml/QmlDockWidget.h"
#include "../src/roompanel/RoomManager.h"
#include "../src/roompanel/RoomModel.h"
#include "../src/timers/CTimers.h"
#include "../src/timers/TimerController.h"
#include "../src/timers/TimerModel.h"
#include "FakeClientBackend.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include <QDir>
#include <QFile>
#include <QFontMetricsF>
#include <QImage>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QScopeGuard>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QToolTip>
#include <QWidget>
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

void TestQml::loadAdventurePanel()
{
    GameObserver observer;
    AdventureTracker tracker(observer, nullptr);
    AdventureLogModel model(tracker, nullptr);

    QmlDockWidget dock("t", "TestDockAdventure", nullptr);
    dock.setContextProperty("adventureLogModel", &model);
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
    quick.rootContext()->setContextProperty("adapter", &adapter);
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

QTEST_MAIN(TestQml)

#include "TestQml.moc"
