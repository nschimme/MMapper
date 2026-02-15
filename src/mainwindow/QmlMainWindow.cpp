// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "QmlMainWindow.h"

#include "../client/ClientWidget.h"
#include "../display/mapcanvas.h"
#include "../display/mapwindow.h"
#include "../global/Version.h"
#include "../global/window_utils.h"
#include "../group/groupwidget.h"
#include "../mapstorage/MapSource.h"
#include "../roompanel/RoomWidget.h"
#include "DescriptionWidget.h"
#include "WidgetProxyItem.h"

#include <QFileDialog>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTextBrowser>
#include <QVBoxLayout>

QmlMainWindow::QmlMainWindow(MMapperCore &core)
    : QWidget(nullptr)
    , m_core(core)
{
    setObjectName("QmlMainWindow");
    const auto appSuffix = isMMapperBeta() ? " Beta" : "";
    mmqt::setWindowTitle2(*this, QString("MMapper%1").arg(appSuffix), "Untitled");

    initWidgets();
    setupQml();

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_quickWidget);

    // Apply the 2/3rds offset to the map
    // NDC coordinates: shift by 1/3 (0.333) to the right
    m_mapWindow->getCanvas()->setViewportOffset(glm::vec2(0.333f, 0.0f));

    m_core.startServices();
}

QmlMainWindow::~QmlMainWindow() = default;

void QmlMainWindow::initWidgets()
{
    m_mapWindow = new MapWindow(m_core.mapData(),
                                m_core.prespammedPath(),
                                m_core.groupManager(),
                                this);
    m_core.setMapCanvas(m_mapWindow->getCanvas());

    m_clientWidget = new ClientWidget(*m_core.listener(), m_core.hotkeyManager(), this);

    // Minimal widgets for QML mode
    m_roomWidget = nullptr;
    m_descriptionWidget = nullptr;
    m_groupWidget = nullptr;
    m_logWidget = new QTextBrowser(this);
    static_cast<QTextBrowser *>(m_logWidget)->setReadOnly(true);

    connect(&m_core, &MMapperCore::sig_log, this, &QmlMainWindow::slot_log);
    connect(m_mapWindow->getCanvas(), &MapCanvas::sig_log, this, &QmlMainWindow::slot_log);
}

void QmlMainWindow::setupQml()
{
    qmlRegisterType<WidgetProxyItem>("MMapper", 1, 0, "WidgetProxyItem");

    m_quickWidget = new QQuickWidget(this);
    m_quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    m_bridge = new QmlBridge(m_core, this);
    m_bridge->setMapWidget(m_mapWindow);
    m_bridge->setClientWidget(m_clientWidget);
    m_bridge->setSecondaryWidgets(nullptr, nullptr, nullptr);
    m_bridge->setLogWidget(m_logWidget);

    connect(m_bridge, &QmlBridge::requestOpenFile, this, &QmlMainWindow::slot_open);

    m_quickWidget->rootContext()->setContextProperty("bridge", m_bridge);

    m_quickWidget->setSource(QUrl("qrc:/main.qml"));
}

void QmlMainWindow::slot_open()
{
    const auto nameFilter = QStringLiteral("MMapper2 maps (*.mm2)"
                                           ";;MMapper2 XML or Pandora maps (*.xml)"
                                           ";;Alternate suffix for MMapper2 XML maps (*.mm2xml)");
    const QString fileName = QFileDialog::getOpenFileName(this,
                                                          "Choose map file ...",
                                                          "",
                                                          nameFilter);
    if (!fileName.isEmpty()) {
        // TODO: Properly implement loadFile in QmlMainWindow if needed,
        // or refactor it into MMapperCore. For now, we'll just log it.
        slot_log("QmlMainWindow", "Request to load: " + fileName);
    }
}

void QmlMainWindow::slot_log(const QString &mod, const QString &message)
{
    static_cast<QTextBrowser *>(m_logWidget)->append(QString("[%1] %2").arg(mod, message));
}

void QmlMainWindow::loadFile(std::shared_ptr<MapSource> source)
{
    // Minimal implementation for QML mode: delegate to MapData directly if not async
    // or just log it for now.
    slot_log("QmlMainWindow", "loadFile: " + source->getFileName());
    // In a full implementation, this would handle the async loading just like MainWindow.
}
