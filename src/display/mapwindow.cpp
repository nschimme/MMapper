// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "mapwindow.h"

#include "../configuration/configuration.h"
#include "../display/Filenames.h"
#include "../global/MakeQPointer.h"
#include "../global/SignalBlocker.h"
#include "../global/Version.h"
#include "../global/utils.h"
#include "AudioHintWidget.h"
#include "mapcanvas.h"

#include <QGridLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QSvgRenderer>
#include <QToolTip>
#include <QtSvgWidgets/QSvgWidget>

class QResizeEvent;

namespace {
/**
 * @brief Widget for displaying the SVG splash screen and overlaying version information.
 *
 * This widget renders the application's branding SVG scaled to fit its area while maintaining
 * aspect ratio. It also overlays the current MMapper version string in the bottom-right
 * corner of the rendered SVG area.
 */
class SplashWidget : public QWidget
{
public:
    explicit SplashWidget(QWidget *parent)
        : QWidget(parent)
        , m_renderer(QStringLiteral(":/icons/mmapper-hi.svg"))
    {}

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);

        const QSize svgSize = m_renderer.defaultSize();
        if (!svgSize.isValid()) {
            return;
        }

        const QRect targetRect = rect();
        const qreal aspect = static_cast<qreal>(svgSize.width()) / svgSize.height();

        int drawWidth = targetRect.width();
        int drawHeight = targetRect.height();

        if (static_cast<qreal>(drawWidth) / drawHeight > aspect) {
            drawWidth = static_cast<int>(drawHeight * aspect);
        } else {
            drawHeight = static_cast<int>(drawWidth / aspect);
        }

        const QRect drawRect((targetRect.width() - drawWidth) / 2,
                             (targetRect.height() - drawHeight) / 2,
                             drawWidth,
                             drawHeight);

        m_renderer.render(&painter, drawRect);

        // Paint version text
        painter.setPen(Qt::yellow);
        const QString versionText = QString::fromUtf8(getMMapperVersion());
        QFont font = painter.font();
        font.setBold(true);
        painter.setFont(font);

        const QFontMetrics fm(painter.fontMetrics());
        const int textWidth = fm.horizontalAdvance(versionText);
        int x = drawRect.right() - textWidth - 5;
        int y = drawRect.bottom() - fm.descent() - 5;

        // Draw text with shadow
        painter.setPen(QColor(0, 0, 0, 150));
        painter.drawText(x + 1, y + 1, versionText);
        painter.setPen(Qt::yellow);
        painter.drawText(x, y, versionText);
    }

private:
    QSvgRenderer m_renderer;
};
} // namespace

MapWindow::MapWindow(MapData &mapData,
                     GameObserver &observer,
                     PrespammedPath &pp,
                     Mmapper2Group &gm,
                     QWidget *const parent)
    : QWidget(parent)
{
    m_viewModel = new MapViewModel(this);

    m_gridLayout = mmqt::makeQPointer<QGridLayout>(this);
    m_gridLayout->setSpacing(0);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);
    m_gridLayout->setRowStretch(0, 1);
    m_gridLayout->setColumnStretch(0, 1);

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

    m_canvas = new MapCanvas(mapData, observer, pp, gm);
    m_canvas->setMinimumSize(QSize(1280 / 4, 720 / 4));
    m_canvas->resize(QSize(1280, 720));

    m_canvasContainer = QWidget::createWindowContainer(m_canvas, this);
    assert(m_canvasContainer);
    assert(m_canvasContainer->parent() == this);

    m_gridLayout->addWidget(m_canvasContainer, 0, 0, 1, 1);

    m_audioHint = new AudioHintWidget(this);
    m_gridLayout->addWidget(m_audioHint, 2, 0, 1, 2);

    setMinimumSize(m_canvas->minimumSize());

    m_splashWidget = mmqt::makeQPointer<SplashWidget>(this);
    m_splashWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_gridLayout->addWidget(m_splashWidget, 0, 0, 1, 1);
    m_splashWidget->show();

    // from map window to canvas
    {
        connect(m_horizontalScrollBar,
                &QScrollBar::valueChanged,
                m_canvas,
                [this](const int x) -> void {
                    const float val = m_viewModel->scrollToWorld(glm::ivec2{x, 0}).x;
                    m_canvas->slot_setHorizontalScroll(val);
                });

        connect(m_verticalScrollBar,
                &QScrollBar::valueChanged,
                m_canvas,
                [this](const int y) -> void {
                    const float value = m_viewModel->scrollToWorld(glm::ivec2{0, y}).y;
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

    connect(m_viewModel,
            &MapViewModel::sig_continuousScrollStep,
            this,
            &MapWindow::slot_applyScrollStep);
}

void MapWindow::hideSplashImage()
{
    if (m_splashWidget) {
        m_splashWidget->hide();
        m_splashWidget->deleteLater();
    }
}

void MapWindow::keyPressEvent(QKeyEvent *const event)
{
    if (event->key() == Qt::Key_Escape) {
        deref(m_canvas).userPressedEscape(true);
        return;
    }
    QWidget::keyPressEvent(event);
}

void MapWindow::keyReleaseEvent(QKeyEvent *const event)
{
    if (event->key() == Qt::Key_Escape) {
        deref(m_canvas).userPressedEscape(false);
        return;
    }
    QWidget::keyReleaseEvent(event);
}

MapWindow::~MapWindow() = default;

void MapWindow::slot_mapMove(const int dx, const int input_dy)
{
    auto &horz = deref(m_horizontalScrollBar);
    auto &vert = deref(m_verticalScrollBar);
    const SignalBlocker block_horz{horz};
    const SignalBlocker block_vert{vert};

    // Y is negated because delta is in world space
    const int dy = -input_dy;

    const int hValue = horz.value() + dx;
    const int vValue = vert.value() + dy;

    const glm::ivec2 scrollPos{hValue, vValue};
    centerOnScrollPos(scrollPos);
}

void MapWindow::slot_continuousScroll(const int hStep, const int vStep)
{
    m_viewModel->slot_continuousScroll(hStep, vStep);
}

void MapWindow::slot_applyScrollStep(const int hStep, const int vStep)
{
    auto &horz = deref(m_horizontalScrollBar);
    auto &vert = deref(m_verticalScrollBar);
    const SignalBlocker block_horz{horz};
    const SignalBlocker block_vert{vert};

    const int vValue = vert.value() + vStep;
    const int hValue = horz.value() + hStep;

    const glm::ivec2 scrollPos{hValue, vValue};
    centerOnScrollPos(scrollPos);
}

void MapWindow::slot_graphicsSettingsChanged()
{
    deref(m_canvas).graphicsSettingsChanged();
}

void MapWindow::slot_centerOnWorldPos(const glm::vec2 worldPos)
{
    auto &horz = deref(m_horizontalScrollBar);
    auto &vert = deref(m_verticalScrollBar);
    const SignalBlocker block_horz{horz};
    const SignalBlocker block_vert{vert};

    const auto scrollPos = m_viewModel->worldToScroll(worldPos);
    horz.setValue(scrollPos.x);
    vert.setValue(scrollPos.y);

    emit sig_setScroll(worldPos);
}

void MapWindow::centerOnScrollPos(const glm::ivec2 scrollPos)
{
    deref(m_horizontalScrollBar).setValue(scrollPos.x);
    deref(m_verticalScrollBar).setValue(scrollPos.y);

    const auto worldPos = m_viewModel->scrollToWorld(scrollPos);
    emit sig_setScroll(worldPos);
}

void MapWindow::resizeEvent(QResizeEvent * /*event*/)
{
    updateScrollBars();
}

void MapWindow::slot_setScrollBars(const Coordinate min, const Coordinate max)
{
    m_viewModel->slot_setScrollBars(min, max);
    updateScrollBars();
}

void MapWindow::updateScrollBars()
{
    const auto showScrollBars = getConfig().general.showScrollBars;

    auto &horz = deref(m_horizontalScrollBar);
    const int hMax = m_viewModel->getHorizontalScrollMax();
    horz.setRange(0, hMax);
    if (hMax > 0 && showScrollBars) {
        horz.show();
    } else {
        horz.hide();
    }

    auto &vert = deref(m_verticalScrollBar);
    const int vMax = m_viewModel->getVerticalScrollMax();
    vert.setRange(0, vMax);
    if (vMax > 0 && showScrollBars) {
        vert.show();
    } else {
        vert.hide();
    }
}

MapCanvas *MapWindow::getCanvas() const
{
    return m_canvas;
}

void MapWindow::setZoom(const float zoom)
{
    deref(m_canvas).setZoom(zoom);
}

float MapWindow::getZoom() const
{
    return deref(m_canvas).getRawZoom();
}

void MapWindow::slot_showTooltip(const QString &text, const QPoint &pos)
{
    auto &container = deref(m_canvasContainer);
    QToolTip::showText(container.mapToGlobal(pos), text, &container, container.rect(), 5000);
}

void MapWindow::setCanvasEnabled(bool enabled)
{
    deref(m_canvasContainer).setEnabled(enabled);
}
