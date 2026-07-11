// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TestQml.h"

#include "../src/adventure/AdventureLogModel.h"
#include "../src/adventure/adventuretracker.h"
#include "../src/observer/gameobserver.h"
#include "../src/proxy/GmcpMessage.h"
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
#include <QScopedPointer>
#include <QtTest/QtTest>

TestQml::TestQml() = default;

TestQml::~TestQml() = default;

void TestQml::initTestCase()
{
    // Allows QQuickWidget to run without a GPU under the offscreen platform.
    qputenv("QT_QUICK_BACKEND", "software");
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

QTEST_MAIN(TestQml)
