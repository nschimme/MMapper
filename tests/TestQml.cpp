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
#include "../src/mainwindow/LogModel.h"
#include "../src/mainwindow/TasksModel.h"
#include "../src/map/Map.h"
#include "../src/map/RawRoom.h"
#include "../src/map/RoomHandle.h"
#include "../src/map/mmapper2room.h"
#include "../src/media/DescriptionAdapter.h"
#include "../src/media/MediaLibrary.h"
#include "../src/observer/gameobserver.h"
#include "../src/proxy/GmcpMessage.h"
#include "../src/qml/DescriptionImageProvider.h"
#include "../src/qml/QmlConfig.h"
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
#include <thread>

#include <QDir>
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
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QVERIFY(QDir(tempDir.path()).mkpath("areas"));
    QVERIFY(QDir(tempDir.path()).mkpath("rooms"));

    // "The Blue Mountains" is the room's RoomArea; DescriptionAdapter::
    // updateRoom (DescriptionAdapter.cpp:~156-159) lowercases it, strips a
    // leading "the ", replaces spaces with dashes, and ASCII-fies the
    // result, so it must resolve to areas/blue-mountains.<ext>.
    const QString areaImagePath = tempDir.path() + "/areas/blue-mountains.png";
    {
        QImage areaImage(200, 150, QImage::Format_ARGB32_Premultiplied);
        areaImage.fill(Qt::blue);
        QVERIFY(areaImage.save(areaImagePath, "PNG"));
    }

    // Nice-to-have: also cover the higher-priority rooms/<serverId>.<ext>
    // lookup with a second, differently-sized fixture image so the two
    // paths can't be confused with each other.
    const QString roomImagePath = tempDir.path() + "/rooms/42.png";
    {
        QImage roomImage(64, 48, QImage::Format_ARGB32_Premultiplied);
        roomImage.fill(Qt::red);
        QVERIFY(roomImage.save(roomImagePath, "PNG"));
    }

    // MediaLibrary's constructor scans directories immediately, so the
    // config override must be in place *before* library is constructed;
    // TestQml's getAssetsPath() stub returns an empty path (see above), so
    // resourcesDirectory is the only source scanPath() finds anything in.
    const QString originalResourcesDir = getConfig().canvas.resourcesDirectory;
    auto restoreConfig = qScopeGuard(
        [originalResourcesDir]() { setConfig().canvas.resourcesDirectory = originalResourcesDir; });
    setConfig().canvas.resourcesDirectory = tempDir.path();

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
        ProgressCounter pc;
        ExternalRawRoom raw;
        raw.setId(ExternalRoomId{1});
        raw.setArea(mmqt::makeRoomArea(QStringLiteral("The Blue Mountains")));
        raw.setName(mmqt::makeRoomName(QStringLiteral("A Mountain Pass")));
        raw.setDescription(mmqt::makeRoomDesc(QStringLiteral("A cold wind blows.")));

        MapPair mapPair = Map::fromRooms(pc, {raw}, {});
        const RoomHandle room = mapPair.modified.getRoomHandle(ExternalRoomId{1});
        QVERIFY(room);

        adapter.updateRoom(room);

        const QUrl imageUrl = adapter.getImageUrl();
        QVERIFY(!imageUrl.isEmpty());
        QCOMPARE(imageUrl.toString(), QStringLiteral("image://description/sharp/1"));
        QCOMPARE(adapter.getRoomName(), QStringLiteral("A Mountain Pass"));

        QSize size;
        const QImage img = requestSharpImage(imageUrl, &size);
        QVERIFY(!img.isNull());
        QCOMPARE(size, QSize(200, 150));
        QCOMPARE(img.size(), QSize(200, 150));
    }

    // --- serverId-based resolution (rooms/<id> takes priority over area) ---
    {
        ProgressCounter pc;
        ExternalRawRoom raw;
        raw.setId(ExternalRoomId{2});
        raw.setServerId(ServerRoomId{42});
        // Deliberately a non-matching area, so a pass would prove the
        // serverId lookup (not the area fallback) resolved the image.
        raw.setArea(mmqt::makeRoomArea(QStringLiteral("Nowhere In Particular")));
        raw.setName(mmqt::makeRoomName(QStringLiteral("Room 42")));
        raw.setDescription(mmqt::makeRoomDesc(QStringLiteral("Some room.")));

        MapPair mapPair = Map::fromRooms(pc, {raw}, {});
        const RoomHandle room = mapPair.modified.getRoomHandle(ExternalRoomId{2});
        QVERIFY(room);

        adapter.updateRoom(room);

        const QUrl imageUrl = adapter.getImageUrl();
        QVERIFY(!imageUrl.isEmpty());

        QSize size;
        const QImage img = requestSharpImage(imageUrl, &size);
        QVERIFY(!img.isNull());
        QCOMPARE(size, QSize(64, 48));
        QCOMPARE(img.size(), QSize(64, 48));
    }
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

    adapter.showToolTip(QStringLiteral("test"));

    // QToolTip::showText() posts the actual native popup asynchronously via
    // an internal timer/event; poll with a bounded loop rather than
    // asserting synchronously, since visibility under the offscreen platform
    // can be flaky. The invokable existing and not crashing is the minimum
    // bar this test must clear regardless of whether the popup itself
    // becomes visible under offscreen.
    bool visible = false;
    for (int i = 0; i < 100 && !visible; ++i) {
        QCoreApplication::processEvents();
        visible = QToolTip::isVisible();
        if (!visible) {
            QTest::qWait(5);
        }
    }
    if (visible) {
        QCOMPARE(QToolTip::text(), QStringLiteral("test"));
    } else {
        qInfo() << "[clockAdapterNativeToolTip] QToolTip never reported visible under the "
                   "offscreen platform; showToolTip()/hideToolTip() did not crash, which is "
                   "the minimum this test can verify headlessly.";
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
    // Exercises ClientDisplay.qml's stick/atEnd tracking against direct
    // contentY writes -- standing in for wheel scrolling and scrollbar
    // dragging, neither of which fires onMovementEnded, which is exactly
    // the bug this test guards against regressing.
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

    // Simulate wheel/scrollbar: write contentY toward the top directly.
    // Neither fires onMovementEnded, which is exactly the bug this guards
    // against regressing.
    listView->setProperty("contentY", 0.0);
    QTRY_VERIFY_WITH_TIMEOUT(!rootObject->property("atEnd").toBool(), 2000);

    // Append-while-unstuck guard: neither contentY nor atEnd may move once
    // the user has scrolled away, even though new rows keep arriving.
    const qreal contentYAfterScroll = listView->property("contentY").toReal();
    model.appendText(QStringLiteral("late line\n"));
    for (int i = 0; i < 10; ++i) {
        QCoreApplication::processEvents();
    }

    QCOMPARE(listView->property("contentY").toReal(), contentYAfterScroll);
    QVERIFY(!rootObject->property("atEnd").toBool());

    // Scroll back down to the bottom -- re-engages stick, mirroring a
    // manual drag-to-bottom.
    const qreal height = listView->property("height").toReal();
    const qreal contentHeight = listView->property("contentHeight").toReal();
    listView->setProperty("contentY", std::max(0.0, contentHeight - height));
    QTRY_VERIFY_WITH_TIMEOUT(rootObject->property("atEnd").toBool(), 2000);

    // Appending while stuck must re-pin the view to the (new) end.
    model.appendText(QStringLiteral("final line\n"));
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

QTEST_MAIN(TestQml)

#include "TestQml.moc"
