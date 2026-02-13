// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "mapwindow.h"

#include "../display/Filenames.h"
#include "../global/MakeQPointer.h"
#include "../global/SignalBlocker.h"
#include "../global/Version.h"
#include "mapcanvas.h"

#include <memory>

#include <QGridLayout>
#include <QLabel>
#include <QPixmap>
#include <QScrollBar>
#include <QToolTip>

class QResizeEvent;

MapWindow::MapWindow(MapData &mapData, PrespammedPath &pp, Mmapper2Group &gm, QWidget *const parent)
    : QWidget(parent)
{
    m_gridLayout = mmqt::makeQPointer<QGridLayout>(this);
    m_gridLayout->setSpacing(0);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);

    m_verticalScrollBar = mmqt::makeQPointer<QScrollBar>(this);
    m_verticalScrollBar->setOrientation(Qt::Vertical);
    m_verticalScrollBar->setRange(0, 0);
    m_verticalScrollBar->hide();
    m_verticalScrollBar->setSingleStep(MapCanvas::SCROLL_SCALE);

    m_gridLayout->addWidget(m_verticalScrollBar, 0, 1, 1, 1);

    m_horizontalScrollBar = mmqt::makeQPointer<QScrollBar>(this);
    m_horizontalScrollBar->setOrientation(Qt::Horizontal);
    m_horizontalScrollBar->setRange(0, 0);
    m_horizontalScrollBar->hide();
    m_horizontalScrollBar->setSingleStep(MapCanvas::SCROLL_SCALE);

    m_gridLayout->addWidget(m_horizontalScrollBar, 1, 0, 1, 1);

    m_canvas = new MapCanvas(mapData, pp, gm);
    m_canvas->setMinimumSize(QSize(1280 / 4, 720 / 4));
    m_canvas->resize(QSize(1280, 720));

    m_canvasContainer = QWidget::createWindowContainer(m_canvas, this);
    assert(m_canvasContainer);
    assert(m_canvasContainer->parent() == this);

    m_gridLayout->addWidget(m_canvasContainer, 0, 0, 1, 1);
    setMinimumSize(m_canvas->minimumSize());

    // Splash setup
    auto createSplashPixmap = [](const QSize &targetLogicalSize, qreal dpr) -> QPixmap {
        // Load base pixmap
        QPixmap splash = getPixmapFilenameRaw("splash.png");
        splash.setDevicePixelRatio(dpr);

        // Scale splash to target physical size
        QSize targetPhysicalSize(static_cast<int>(targetLogicalSize.width() * dpr),
                                 static_cast<int>(targetLogicalSize.height() * dpr));
        QPixmap scaled = splash.scaled(targetPhysicalSize,
                                       Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);

        // Now paint on the scaled pixmap
        QPainter painter(&scaled);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setPen(QPen(Qt::yellow));

        QFont font = painter.font();
        painter.setFont(font);

        const QString versionText = QString::fromUtf8(getMMapperVersion());

        // Calculate logical size for positioning text
        QRect rect = scaled.rect();
        int scaledWidth = static_cast<int>(rect.width() / dpr);
        int scaledHeight = static_cast<int>(rect.height() / dpr);
        int textWidth = QFontMetrics(font).horizontalAdvance(versionText);

        // Draw text bottom-right with some padding
        painter.drawText(scaledWidth - textWidth - 5, scaledHeight - 5, versionText);

        painter.end();

        return scaled;
    };
    auto splashPixmap = createSplashPixmap(size(), devicePixelRatioF());

    // Now set pixmap with painted text
    m_splashLabel = mmqt::makeQPointer<QLabel>(this);
    m_splashLabel->setPixmap(splashPixmap);
    m_splashLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_splashLabel->setGeometry(rect());
    m_gridLayout->addWidget(m_splashLabel, 0, 0, 1, 1, Qt::AlignCenter);
    m_splashLabel->show();

    // from map window to canvas
    {
        connect(m_horizontalScrollBar,
                &QScrollBar::valueChanged,
                m_canvas,
                [this](const int x) -> void {
                    const float val = m_knownMapSize.scrollToWorld(glm::ivec2{x, 0}).x;
                    m_canvas->slot_setHorizontalScroll(val);
                });

        connect(m_verticalScrollBar,
                &QScrollBar::valueChanged,
                m_canvas,
                [this](const int y) -> void {
                    const float value = m_knownMapSize.scrollToWorld(glm::ivec2{0, y}).y;
                    m_canvas->slot_setVerticalScroll(value);
                });

        connect(this, &MapWindow::sig_setScroll, m_canvas, &MapCanvas::slot_setScroll);
    }

    // from canvas to map window
    {
        connect(m_canvas, &MapCanvas::sig_onCenter, this, &MapWindow::slot_centerOnWorldPos);
        connect(m_canvas, &MapCanvas::sig_setScrollBars, this, &MapWindow::slot_setScrollBars);
        connect(m_canvas, &MapCanvas::sig_continuousScroll, this, &MapWindow::slot_continuousScroll);
        connect(m_canvas, &MapCanvas::sig_mapMove, this, &MapWindow::slot_mapMove);
        connect(m_canvas, &MapCanvas::sig_zoomChanged, this, &MapWindow::slot_zoomChanged);
        connect(m_canvas, &MapCanvas::sig_showTooltip, this, &MapWindow::slot_showTooltip);
    }
}

void MapWindow::hideSplashImage()
{
    if (m_splashLabel) {
        m_splashLabel->hide();
        m_splashLabel->deleteLater();
    }
}

void MapWindow::keyPressEvent(QKeyEvent *const event)
{
    if (event->key() == Qt::Key_Escape) {
        m_canvas->userPressedEscape(true);
        return;
    }
    QWidget::keyPressEvent(event);
}

void MapWindow::keyReleaseEvent(QKeyEvent *const event)
{
    if (event->key() == Qt::Key_Escape) {
        m_canvas->userPressedEscape(false);
        return;
    }
    QWidget::keyReleaseEvent(event);
}

MapWindow::~MapWindow() = default;

void MapWindow::slot_mapMove(const int dx, const int input_dy)
{
    // Y is negated because delta is in world space
    const int dy = -input_dy;

    const SignalBlocker block_horz{*m_horizontalScrollBar};
    const SignalBlocker block_vert{*m_verticalScrollBar};

    const int hValue = m_horizontalScrollBar->value() + dx;
    const int vValue = m_verticalScrollBar->value() + dy;

    const glm::ivec2 scrollPos{hValue, vValue};
    centerOnScrollPos(scrollPos);
}

// REVISIT: This looks more like "delayed jump" than "continuous scroll."
void MapWindow::slot_continuousScroll(const int hStep, const int input_vStep)
{
    const auto fitsInInt8 = [](int n) -> bool {
        // alternate: test against std::numeric_limits<int8_t>::min and max.
        return static_cast<int>(static_cast<int8_t>(n)) == n;
    };

    // code originally used int8_t
    assert(fitsInInt8(hStep));
    assert(fitsInInt8(input_vStep));

    // Y is negated because delta is in world space
    const int vStep = -input_vStep;

    m_horizontalScrollStep = hStep;
    m_verticalScrollStep = vStep;

    // stop
    if ((scrollTimer != nullptr) && hStep == 0 && vStep == 0) {
        if (scrollTimer->isActive()) {
            scrollTimer->stop();
        }
        scrollTimer->deleteLater();
    }

    // start
    if ((scrollTimer == nullptr) && (hStep != 0 || vStep != 0)) {
        scrollTimer = mmqt::makeQPointer<QTimer>(this);
        connect(scrollTimer, &QTimer::timeout, this, &MapWindow::slot_scrollTimerTimeout);
        scrollTimer->start(100);
    }
}

void MapWindow::slot_scrollTimerTimeout()
{
    const SignalBlocker block_horz{*m_horizontalScrollBar};
    const SignalBlocker block_vert{*m_verticalScrollBar};

    const int vValue = m_verticalScrollBar->value() + m_verticalScrollStep;
    const int hValue = m_horizontalScrollBar->value() + m_horizontalScrollStep;

    const glm::ivec2 scrollPos{hValue, vValue};
    centerOnScrollPos(scrollPos);
}

void MapWindow::slot_graphicsSettingsChanged()
{
    this->m_canvas->graphicsSettingsChanged();
}

void MapWindow::slot_centerOnWorldPos(const glm::vec2 &worldPos)
{
    const SignalBlocker block_horz{*m_horizontalScrollBar};
    const SignalBlocker block_vert{*m_verticalScrollBar};

    const auto scrollPos = m_knownMapSize.worldToScroll(worldPos);
    m_horizontalScrollBar->setValue(scrollPos.x);
    m_verticalScrollBar->setValue(scrollPos.y);
}

void MapWindow::centerOnScrollPos(const glm::ivec2 &scrollPos)
{
    m_horizontalScrollBar->setValue(scrollPos.x);
    m_verticalScrollBar->setValue(scrollPos.y);

    const auto worldPos = m_knownMapSize.scrollToWorld(scrollPos);
    emit sig_setScroll(worldPos);
}

void MapWindow::resizeEvent(QResizeEvent * /*event*/)
{
    updateScrollBars();
}

void MapWindow::slot_setScrollBars(const Coordinate &min, const Coordinate &max)
{
    m_knownMapSize.min = min.to_ivec3();
    m_knownMapSize.max = max.to_ivec3();
    updateScrollBars();
}

void MapWindow::updateScrollBars()
{
    const auto dims = m_knownMapSize.size() * MapCanvas::SCROLL_SCALE;
    const auto showScrollBars = getConfig().general.showScrollBars;
    m_horizontalScrollBar->setRange(0, dims.x);
    if (dims.x > 0 && showScrollBars) {
        m_horizontalScrollBar->show();
    } else {
        m_horizontalScrollBar->hide();
    }

    m_verticalScrollBar->setRange(0, dims.y);
    if (dims.y > 0 && showScrollBars) {
        m_verticalScrollBar->show();
    } else {
        m_verticalScrollBar->hide();
    }
}

MapCanvas *MapWindow::getCanvas() const
{
    return m_canvas;
}

void MapWindow::setZoom(const float zoom)
{
    m_canvas->setZoom(zoom);
}

float MapWindow::getZoom() const
{
    return m_canvas->getRawZoom();
}

void MapWindow::slot_showTooltip(const QString &text, const QPoint &pos)
{
    QToolTip::showText(m_canvasContainer->mapToGlobal(pos), text, m_canvasContainer);
}

void MapWindow::setCanvasEnabled(bool enabled)
{
    m_canvasContainer->setEnabled(enabled);
}

glm::vec2 MapWindow::KnownMapSize::scrollToWorld(const glm::ivec2 &scrollPos) const
{
    auto worldPos = glm::vec2{scrollPos} / static_cast<float>(MapCanvas::SCROLL_SCALE);
    worldPos.y = static_cast<float>(size().y) - worldPos.y; // negate Y
    worldPos += glm::vec2{min};
    return worldPos;
}

glm::ivec2 MapWindow::KnownMapSize::worldToScroll(const glm::vec2 &worldPos_in) const
{
    auto worldPos = worldPos_in;
    worldPos -= glm::vec2{min};
    worldPos.y = static_cast<float>(size().y) - worldPos.y; // negate Y
    const glm::ivec2 scrollPos{worldPos * static_cast<float>(MapCanvas::SCROLL_SCALE)};
    return scrollPos;
}
