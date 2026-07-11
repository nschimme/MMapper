// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TestQml.h"

#include "../src/adventure/AdventureLogModel.h"
#include "../src/adventure/adventuretracker.h"
#include "../src/configuration/configuration.h"
#include "../src/group/CGroupChar.h"
#include "../src/group/GroupModel.h"
#include "../src/mainwindow/LogModel.h"
#include "../src/observer/gameobserver.h"
#include "../src/proxy/GmcpMessage.h"
#include "../src/qml/QmlConfig.h"
#include "../src/qml/QmlDockWidget.h"
#include "../src/roompanel/RoomManager.h"
#include "../src/roompanel/RoomModel.h"
#include "../src/timers/CTimers.h"
#include "../src/timers/TimerController.h"
#include "../src/timers/TimerModel.h"

#include <QFontMetricsF>
#include <QJSEngine>
#include <QJSValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QScopeGuard>
#include <QScopedPointer>
#include <QSignalSpy>
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

TestQml::TestQml() = default;

TestQml::~TestQml() = default;

void TestQml::initTestCase()
{
    // Allows QQuickWidget to run without a GPU under the offscreen platform.
    qputenv("QT_QUICK_BACKEND", "software");
    // Required before any getConfig()/setConfig() call (see
    // qmlConfigRoundTrip()); see also TestMainWindow and TestClock.
    setEnteredMain();
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

QTEST_MAIN(TestQml)

#include "TestQml.moc"
