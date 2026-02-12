// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "mapwindow.h"

#include "../configuration/configuration.h"
#include "../display/Filenames.h"
#include "../display/InfomarkSelection.h"
#include "../display/MapCanvasData.h"
#include "../display/connectionselection.h"
#include "../global/SignalBlocker.h"
#include "../global/Version.h"
#include "../mapdata/mapdata.h"
#include "mapcanvas.h"

#include <memory>

#include <QGestureEvent>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QScrollBar>
#include <QToolTip>
#include <QWheelEvent>

#if defined(_MSC_VER) || defined(__MINGW32__)
#undef near // Bad dog, Microsoft; bad dog!!!
#undef far  // Bad dog, Microsoft; bad dog!!!
#endif

class QResizeEvent;

MapWindow::MapWindow(MapData &mapData, PrespammedPath &pp, Mmapper2Group &gm, QWidget *const parent)
    : QWidget(parent)
    , MapCanvasInputState(pp)
    , m_data(mapData)
{
    m_gridLayout = std::make_unique<QGridLayout>(this);
    m_gridLayout->setSpacing(0);
    m_gridLayout->setContentsMargins(0, 0, 0, 0);

    m_verticalScrollBar = std::make_unique<QScrollBar>(this);
    m_verticalScrollBar->setOrientation(Qt::Vertical);
    m_verticalScrollBar->setRange(0, 0);
    m_verticalScrollBar->hide();
    m_verticalScrollBar->setSingleStep(MapCanvas::SCROLL_SCALE);

    m_gridLayout->addWidget(m_verticalScrollBar.get(), 0, 1, 1, 1);

    m_horizontalScrollBar = std::make_unique<QScrollBar>(this);
    m_horizontalScrollBar->setOrientation(Qt::Horizontal);
    m_horizontalScrollBar->setRange(0, 0);
    m_horizontalScrollBar->hide();
    m_horizontalScrollBar->setSingleStep(MapCanvas::SCROLL_SCALE);

    m_gridLayout->addWidget(m_horizontalScrollBar.get(), 1, 0, 1, 1);

    m_canvas = new MapCanvas(mapData, pp, gm);
    m_canvas->setInputState(this);
    MapCanvas *const canvas = m_canvas;

    m_canvasContainer = QWidget::createWindowContainer(m_canvas, this);
    m_canvasContainer->setFocusPolicy(Qt::StrongFocus);
    m_canvasContainer->setCursor(Qt::OpenHandCursor);
    m_canvasContainer->grabGesture(Qt::PinchGesture);
    m_canvasContainer->setContextMenuPolicy(Qt::CustomContextMenu);
    m_canvasContainer->installEventFilter(this);

    setMouseTracking(true);

    connect(m_canvasContainer,
            &QWidget::customContextMenuRequested,
            this,
            &MapWindow::sig_customContextMenuRequested);

    m_gridLayout->addWidget(m_canvasContainer, 0, 0, 1, 1);
    setMinimumSize(1280 / 4, 720 / 4);

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
    m_splashLabel = std::make_unique<QLabel>(this);
    m_splashLabel->setPixmap(splashPixmap);
    m_splashLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_splashLabel->setGeometry(rect());
    m_gridLayout->addWidget(m_splashLabel.get(), 0, 0, 1, 1, Qt::AlignCenter);
    m_splashLabel->show();

    // from map window to canvas
    {
        connect(m_horizontalScrollBar.get(),
                &QScrollBar::valueChanged,
                canvas,
                [this](const int x) -> void {
                    const float val = m_knownMapSize.scrollToWorld(glm::ivec2{x, 0}).x;
                    m_canvas->slot_setHorizontalScroll(val);
                });

        connect(m_verticalScrollBar.get(),
                &QScrollBar::valueChanged,
                canvas,
                [this](const int y) -> void {
                    const float value = m_knownMapSize.scrollToWorld(glm::ivec2{0, y}).y;
                    m_canvas->slot_setVerticalScroll(value);
                });

        connect(this, &MapWindow::sig_setScroll, canvas, &MapCanvas::slot_setScroll);
    }

    // from canvas to map window
    {
        connect(canvas, &MapCanvas::sig_onCenter, this, &MapWindow::slot_centerOnWorldPos);
        connect(canvas, &MapCanvas::sig_setScrollBars, this, &MapWindow::slot_setScrollBars);
        connect(canvas, &MapCanvas::sig_log, this, &MapWindow::sig_log);
    }

    // relay signals from canvas to window
    {
        connect(canvas, &MapCanvas::sig_onCenter, this, &MapWindow::sig_onCenter);
        connect(canvas, &MapCanvas::sig_setScrollBars, this, &MapWindow::sig_setScrollBars);
        connect(canvas, &MapCanvas::sig_selectionChanged, this, &MapWindow::sig_selectionChanged);
        connect(canvas, &MapCanvas::sig_newRoomSelection, this, &MapWindow::sig_newRoomSelection);
        connect(canvas,
                &MapCanvas::sig_newConnectionSelection,
                this,
                &MapWindow::sig_newConnectionSelection);
        connect(canvas,
                &MapCanvas::sig_newInfomarkSelection,
                this,
                &MapWindow::sig_newInfomarkSelection);
        connect(canvas, &MapCanvas::sig_setCurrentRoom, this, &MapWindow::sig_setCurrentRoom);
        connect(canvas, &MapCanvas::sig_zoomChanged, this, &MapWindow::sig_zoomChanged);
    }
}

void MapWindow::hideSplashImage()
{
    if (m_splashLabel) {
        m_splashLabel->hide();
        m_splashLabel.reset();
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

void MapWindow::setMouseTracking(const bool enable)
{
    QWidget::setMouseTracking(enable);
    if (m_canvasContainer) {
        m_canvasContainer->setMouseTracking(enable);
    }
}

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
        scrollTimer.reset();
    }

    // start
    if ((scrollTimer == nullptr) && (hStep != 0 || vStep != 0)) {
        scrollTimer = std::make_unique<QTimer>(this);
        connect(scrollTimer.get(), &QTimer::timeout, this, &MapWindow::slot_scrollTimerTimeout);
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
    const auto scrollPos = m_knownMapSize.worldToScroll(worldPos);
    centerOnScrollPos(scrollPos);
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

MapCanvas *MapWindow::getMapCanvas() const
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

bool MapWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_canvasContainer) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            handleMousePress(static_cast<QMouseEvent *>(event));
            return false;
        case QEvent::MouseButtonRelease:
            handleMouseRelease(static_cast<QMouseEvent *>(event));
            return false;
        case QEvent::MouseMove:
            handleMouseMove(static_cast<QMouseEvent *>(event));
            return false;
        case QEvent::Wheel:
            handleWheel(static_cast<QWheelEvent *>(event));
            return false;
        case QEvent::KeyPress:
            keyPressEvent(static_cast<QKeyEvent *>(event));
            return false;
        case QEvent::KeyRelease:
            keyReleaseEvent(static_cast<QKeyEvent *>(event));
            return false;
        case QEvent::Gesture:
            handleGesture(static_cast<QGestureEvent *>(event));
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MapWindow::handleMousePress(QMouseEvent *const event)
{
    if (m_canvasContainer) {
        m_canvasContainer->setFocus();
    }

    const bool hasLeftButton = (event->buttons() & Qt::LeftButton) != 0u;
    const bool hasRightButton = (event->buttons() & Qt::RightButton) != 0u;
    const bool hasCtrl = (event->modifiers() & Qt::ControlModifier) != 0u;
    const bool hasAlt = (event->modifiers() & Qt::AltModifier) != 0u;

    if (hasLeftButton && hasAlt) {
        m_altDragState.emplace(AltDragState{event->pos(), m_canvasContainer->cursor()});
        m_canvasContainer->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    m_sel1 = m_sel2 = m_canvas->getUnprojectedMouseSel(event);
    m_mouseLeftPressed = hasLeftButton;
    m_mouseRightPressed = hasRightButton;

    if (event->button() == Qt::ForwardButton) {
        m_canvas->slot_layerUp();
        event->accept();
        return;
    } else if (event->button() == Qt::BackButton) {
        m_canvas->slot_layerDown();
        event->accept();
        return;
    } else if (!m_mouseLeftPressed && m_mouseRightPressed) {
        if (m_canvasMouseMode == CanvasMouseModeEnum::MOVE && hasSel1()) {
            // Select the room under the cursor
            slot_setRoomSelection(SigRoomSelection{
                RoomSelection::createSelection(m_data.findAllRooms(getSel1().getCoordinate()))});

            // Select infomarks under the cursor.
            slot_setInfomarkSelection(m_canvas->getInfomarkSelection(getSel1()));
        }
        m_mouseRightPressed = false;
        event->accept();
        return;
    }

    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        m_canvas->infomarksChanged();
        break;
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        // Select infomarks
        if (hasLeftButton && hasSel1()) {
            auto tmpSel = m_canvas->getInfomarkSelection(getSel1());
            if (m_infoMarkSelection != nullptr && !tmpSel->empty()
                && m_infoMarkSelection->contains(tmpSel->front().getId())) {
                m_infoMarkSelectionMove.reset();   // dtor, if necessary
                m_infoMarkSelectionMove.emplace(); // ctor
            } else {
                m_selectedArea = false;
                m_infoMarkSelectionMove.reset();
            }
        }
        m_canvas->selectionChanged();
        break;
    case CanvasMouseModeEnum::MOVE:
        if (hasLeftButton && hasSel1()) {
            m_canvasContainer->setCursor(Qt::ClosedHandCursor);
            startMoving(m_sel1.value());
        }
        break;

    case CanvasMouseModeEnum::RAYPICK_ROOMS:
        if (hasLeftButton) {
            const auto xy = m_canvas->getMouseCoords(event);
            const auto nearPos = m_canvas->unproject_raw(glm::vec3{xy, 0.f});
            const auto farPos = m_canvas->unproject_raw(glm::vec3{xy, 1.f});

            const auto hiz = static_cast<int>(std::floor(nearPos.z));
            const auto loz = static_cast<int>(std::ceil(farPos.z));
            if (hiz <= loz) {
                break;
            }

            const auto inv_denom = 1.f / (farPos.z - nearPos.z);

            RoomIdSet tmpSel;
            for (int z = hiz; z >= loz; --z) {
                const float t = (static_cast<float>(z) - nearPos.z) * inv_denom;
                assert(isClamped(t, 0.f, 1.f));
                const auto pos = glm::mix(nearPos, farPos, t);
                assert(static_cast<int>(std::lround(pos.z)) == z);
                const Coordinate c2 = MouseSel(Coordinate2f(pos.x, pos.y), z).getCoordinate();
                if (const auto r = m_data.findRoomHandle(c2)) {
                    tmpSel.insert(r.getId());
                }
            }

            if (!tmpSel.empty()) {
                slot_setRoomSelection(
                    SigRoomSelection(RoomSelection::createSelection(std::move(tmpSel))));
            }
        }
        break;

    case CanvasMouseModeEnum::SELECT_ROOMS:
        // Cancel
        if (hasRightButton) {
            m_selectedArea = false;
            slot_clearRoomSelection();
        }
        // Select rooms
        if (hasLeftButton && hasSel1()) {
            if (!hasCtrl) {
                const auto pRoom = m_data.findRoomHandle(getSel1().getCoordinate());
                if (pRoom.exists() && m_roomSelection != nullptr
                    && m_roomSelection->contains(pRoom.getId())) {
                    m_roomSelectionMove.emplace(RoomSelMove{});
                } else {
                    m_roomSelectionMove.reset();
                    m_selectedArea = false;
                    slot_clearRoomSelection();
                }
            } else {
                m_ctrlPressed = true;
            }
        }
        m_canvas->selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
        // Select connection
        if (hasLeftButton && hasSel1()) {
            m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1());
            if (!m_connectionSelection->isFirstValid()) {
                m_connectionSelection = nullptr;
            }
            emit sig_newConnectionSelection(nullptr);
        }
        // Cancel
        if (hasRightButton) {
            slot_clearConnectionSelection();
        }
        m_canvas->selectionChanged();
        break;

    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
        if (hasLeftButton && hasSel1()) {
            m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1());
            if (!m_connectionSelection->isFirstValid()) {
                m_connectionSelection = nullptr;
            } else {
                const auto &r1 = m_connectionSelection->getFirst().room;
                const ExitDirEnum dir1 = m_connectionSelection->getFirst().direction;

                if (r1.getExit(dir1).outIsEmpty()) {
                    m_connectionSelection = nullptr;
                }
            }
            emit sig_newConnectionSelection(nullptr);
        }
        // Cancel
        if (hasRightButton) {
            slot_clearConnectionSelection();
        }
        m_canvas->selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_ROOMS:
        slot_createRoom();
        break;

    case CanvasMouseModeEnum::NONE:
    default:
        break;
    }
    event->accept();
}

void MapWindow::handleMouseRelease(QMouseEvent *const event)
{
    if (m_altDragState.has_value()) {
        m_canvasContainer->setCursor(m_altDragState->originalCursor);
        m_altDragState.reset();
        event->accept();
        return;
    }

    emit sig_continuousScroll(0, 0);
    m_sel2 = m_canvas->getUnprojectedMouseSel(event);

    if (m_mouseRightPressed) {
        m_mouseRightPressed = false;
    }

    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        m_canvasContainer->setCursor(Qt::ArrowCursor);
        if (m_mouseLeftPressed) {
            m_mouseLeftPressed = false;
            if (hasInfomarkSelectionMove()) {
                const auto pos_copy = m_infoMarkSelectionMove->pos;
                m_infoMarkSelectionMove.reset();
                if (m_infoMarkSelection != nullptr) {
                    const auto offset = Coordinate{(pos_copy * INFOMARK_SCALE).truncate(), 0};

                    // Update infomark location
                    const InfomarkSelection &sel = deref(m_infoMarkSelection);
                    sel.applyOffset(offset);
                    m_canvas->infomarksChanged();
                }
            } else if (hasSel1() && hasSel2()) {
                // Add infomarks to selection
                const auto c1 = getSel1().getScaledCoordinate(INFOMARK_SCALE);
                const auto c2 = getSel2().getScaledCoordinate(INFOMARK_SCALE);
                auto tmpSel = InfomarkSelection::alloc(m_data, c1, c2);
                if (tmpSel && tmpSel->size() == 1) {
                    const InfomarkHandle &firstMark = tmpSel->front();
                    const Coordinate &pos = firstMark.getPosition1();
                    QString ctemp = QString("Selected Info Mark: [%1] (at %2,%3,%4)")
                                        .arg(firstMark.getText().toQString())
                                        .arg(pos.x)
                                        .arg(pos.y)
                                        .arg(pos.z);
                    emit sig_log("MapCanvas", ctemp);
                }
                slot_setInfomarkSelection(tmpSel);
            }
            m_selectedArea = false;
        }
        m_canvas->selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        if (m_mouseLeftPressed && hasSel1() && hasSel2()) {
            m_mouseLeftPressed = false;
            const auto c1 = getSel1().getScaledCoordinate(INFOMARK_SCALE);
            const auto c2 = getSel2().getScaledCoordinate(INFOMARK_SCALE);
            auto tmpSel = InfomarkSelection::allocEmpty(m_data, c1, c2);
            slot_setInfomarkSelection(tmpSel);
        }
        m_canvas->infomarksChanged();
        break;

    case CanvasMouseModeEnum::MOVE:
        stopMoving();
        m_canvasContainer->setCursor(Qt::OpenHandCursor);
        if (m_mouseLeftPressed) {
            m_mouseLeftPressed = false;
        }
        // Display a room info tooltip if there was no mouse movement
        if (hasSel1() && hasSel2() && getSel1().to_vec3() == getSel2().to_vec3()) {
            if (const auto room = m_data.findRoomHandle(getSel1().getCoordinate())) {
                // Tooltip doesn't support ANSI, and there's no way to add formatted text.
                auto message = mmqt::previewRoom(room,
                                                 mmqt::StripAnsiEnum::Yes,
                                                 mmqt::PreviewStyleEnum::ForDisplay);

                QToolTip::showText(m_canvasContainer->mapToGlobal(event->position().toPoint()),
                                   message,
                                   m_canvasContainer,
                                   m_canvasContainer->rect(),
                                   5000);
            }
        }
        break;

    case CanvasMouseModeEnum::RAYPICK_ROOMS:
        break;

    case CanvasMouseModeEnum::SELECT_ROOMS:
        m_canvasContainer->setCursor(Qt::ArrowCursor);

        if (m_ctrlPressed && m_altPressed) {
            break;
        }

        if (m_mouseLeftPressed) {
            m_mouseLeftPressed = false;

            if (m_roomSelectionMove.has_value()) {
                const auto pos = m_roomSelectionMove->pos;
                const bool wrongPlace = m_roomSelectionMove->wrongPlace;
                m_roomSelectionMove.reset();
                if (!wrongPlace && (m_roomSelection != nullptr)) {
                    const Coordinate moverel{pos, 0};
                    m_data.applySingleChange(Change{
                        room_change_types::MoveRelative2{m_roomSelection->getRoomIds(), moverel}});
                }

            } else {
                if (hasSel1() && hasSel2()) {
                    const Coordinate &c1 = getSel1().getCoordinate();
                    const Coordinate &c2 = getSel2().getCoordinate();
                    if (m_roomSelection == nullptr) {
                        // add rooms to default selections
                        m_roomSelection = RoomSelection::createSelection(
                            m_data.findAllRooms(c1, c2));
                    } else {
                        auto &sel = deref(m_roomSelection);
                        sel.removeMissing(m_data);
                        // add or remove rooms to/from default selection
                        const auto tmpSel = m_data.findAllRooms(c1, c2);
                        for (const RoomId key : tmpSel) {
                            if (sel.contains(key)) {
                                sel.erase(key);
                            } else {
                                sel.insert(key);
                            }
                        }
                    }
                }

                if (m_roomSelection != nullptr && !m_roomSelection->empty()) {
                    slot_setRoomSelection(SigRoomSelection{m_roomSelection});
                }
            }
            m_selectedArea = false;
        }
        m_canvas->selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
        if (m_mouseLeftPressed && hasSel1() && hasSel2()) {
            m_mouseLeftPressed = false;

            if (m_connectionSelection == nullptr) {
                m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1());
            }
            m_connectionSelection->setSecond(getSel2());

            if (!m_connectionSelection->isValid()) {
                m_connectionSelection = nullptr;
            } else {
                const auto first = m_connectionSelection->getFirst();
                const auto second = m_connectionSelection->getSecond();
                const auto &r1 = first.room;
                const auto &r2 = second.room;

                if (r1.exists() && r2.exists()) {
                    const ExitDirEnum dir1 = first.direction;
                    const ExitDirEnum dir2 = second.direction;

                    const RoomId id1 = r1.getId();
                    const RoomId id2 = r2.getId();

                    const bool isCompleteNew = m_connectionSelection->isCompleteNew();
                    m_connectionSelection = nullptr;

                    if (isCompleteNew) {
                        const WaysEnum ways = (m_canvasMouseMode
                                               != CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS)
                                                  ? WaysEnum::TwoWay
                                                  : WaysEnum::OneWay;
                        const exit_change_types::ModifyExitConnection change{ChangeTypeEnum::Add,
                                                                             id1,
                                                                             dir1,
                                                                             id2,
                                                                             ways};
                        m_data.applySingleChange(Change{change});
                    }
                    m_connectionSelection = ConnectionSelection::alloc(m_data);
                    m_connectionSelection->setFirst(id1, dir1);
                    m_connectionSelection->setSecond(id2, dir2);
                    m_canvas->slot_mapChanged();
                }
            }

            slot_setConnectionSelection(m_connectionSelection);
        }
        m_canvas->selectionChanged();
        break;

    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
        if (m_mouseLeftPressed && (m_connectionSelection != nullptr || hasSel1()) && hasSel2()) {
            m_mouseLeftPressed = false;

            if (m_connectionSelection == nullptr && hasSel1()) {
                m_connectionSelection = ConnectionSelection::alloc(m_data, getSel1());
            }
            m_connectionSelection->setSecond(getSel2());

            if (!m_connectionSelection->isValid() || !m_connectionSelection->isCompleteExisting()) {
                m_connectionSelection = nullptr;
            }
            slot_setConnectionSelection(m_connectionSelection);
        }
        m_canvas->selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_ROOMS:
        if (m_mouseLeftPressed) {
            m_mouseLeftPressed = false;
        }
        break;

    case CanvasMouseModeEnum::NONE:
    default:
        break;
    }

    m_altPressed = false;
    m_ctrlPressed = false;
    event->accept();
}

void MapWindow::handleMouseMove(QMouseEvent *const event)
{
    if (m_altDragState.has_value()) {
        // The user released the Alt key mid-drag.
        if (!((event->modifiers() & Qt::AltModifier) != 0u)) {
            m_canvasContainer->setCursor(m_altDragState->originalCursor);
            m_altDragState.reset();
            return;
        }

        auto &conf = setConfig().canvas.advanced;
        auto &dragState = m_altDragState.value();
        bool angleChanged = false;

        const auto pos = event->pos();
        const auto delta = pos - dragState.lastPos;
        dragState.lastPos = pos;

        // Camera rotation sensitivity: 3 degree per pixel
        constexpr float SENSITIVITY = 0.3f;

        // Horizontal movement adjusts yaw (horizontalAngle)
        const int dx = delta.x();
        if (dx != 0) {
            conf.horizontalAngle.set(conf.horizontalAngle.get()
                                     + static_cast<int>(static_cast<float>(dx) * SENSITIVITY));
            angleChanged = true;
        }

        // Vertical movement adjusts pitch (verticalAngle), if auto-tilt is off
        if (!conf.autoTilt.get()) {
            const int dy = delta.y();
            if (dy != 0) {
                // Negated to match intuitive up/down dragging
                conf.verticalAngle.set(conf.verticalAngle.get()
                                       + static_cast<int>(static_cast<float>(-dy) * SENSITIVITY));
                angleChanged = true;
            }
        }

        if (angleChanged) {
            m_canvas->graphicsSettingsChanged();
        }
        event->accept();
        return;
    }

    const bool hasLeftButton = (event->buttons() & Qt::LeftButton) != 0u;

    if (m_canvasMouseMode != CanvasMouseModeEnum::MOVE) {
        // NOTE: Y is opposite of what you might expect here.
        const int vScroll = std::invoke([this, event]() -> int {
            const int h = m_canvasContainer->height();
            const int MARGIN = std::min(100, h / 4);
            const auto y = event->position().y();
            if (y < MARGIN) {
                return MapCanvas::SCROLL_SCALE;
            } else if (y > h - MARGIN) {
                return -MapCanvas::SCROLL_SCALE;
            } else {
                return 0;
            }
        });
        const int hScroll = std::invoke([this, event]() -> int {
            const int w = m_canvasContainer->width();
            const int MARGIN = std::min(100, w / 4);
            const auto x = event->position().x();
            if (x < MARGIN) {
                return -MapCanvas::SCROLL_SCALE;
            } else if (x > w - MARGIN) {
                return MapCanvas::SCROLL_SCALE;
            } else {
                return 0;
            }
        });

        emit sig_continuousScroll(hScroll, vScroll);
        slot_continuousScroll(hScroll, vScroll);
    }

    m_sel2 = m_canvas->getUnprojectedMouseSel(event);

    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        if (hasLeftButton && hasSel1() && hasSel2()) {
            if (hasInfomarkSelectionMove()) {
                m_infoMarkSelectionMove->pos = getSel2().pos - getSel1().pos;
                m_canvasContainer->setCursor(Qt::ClosedHandCursor);

            } else {
                m_selectedArea = true;
            }
        }
        m_canvas->selectionChanged();
        break;
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        if (hasLeftButton) {
            m_selectedArea = true;
        }
        m_canvas->infomarksChanged();
        break;
    case CanvasMouseModeEnum::MOVE:
        if (hasLeftButton && m_mouseLeftPressed && hasSel2() && hasBackup()) {
            const Coordinate2i delta = ((getSel2().pos - getBackup().pos)
                                        * static_cast<float>(MapCanvas::SCROLL_SCALE))
                                           .truncate();
            if (delta.x != 0 || delta.y != 0) {
                // negated because dragging to right is scrolling to the left.
                emit sig_mapMove(-delta.x, -delta.y);
                slot_mapMove(-delta.x, -delta.y);
            }
        }
        break;

    case CanvasMouseModeEnum::RAYPICK_ROOMS:
        break;

    case CanvasMouseModeEnum::SELECT_ROOMS:
        if (hasLeftButton) {
            if (hasRoomSelectionMove() && hasSel1() && hasSel2()) {
                auto &map = this->m_data;
                auto isMovable = [&map](RoomSelection &sel, const Coordinate &offset) -> bool {
                    sel.removeMissing(map);
                    return map.getCurrentMap().wouldAllowRelativeMove(sel.getRoomIds(), offset);
                };

                const auto diff = getSel2().pos.truncate() - getSel1().pos.truncate();
                const auto wrongPlace = !isMovable(deref(m_roomSelection), Coordinate{diff, 0});
                m_roomSelectionMove->pos = diff;
                m_roomSelectionMove->wrongPlace = wrongPlace;

                m_canvasContainer->setCursor(wrongPlace ? Qt::ForbiddenCursor
                                                        : Qt::ClosedHandCursor);
            } else {
                m_selectedArea = true;
            }
        }
        m_canvas->selectionChanged();
        break;

    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
        if (hasLeftButton && hasSel2()) {
            if (m_connectionSelection != nullptr) {
                m_connectionSelection->setSecond(getSel2());

                if (m_connectionSelection->isValid() && !m_connectionSelection->isCompleteNew()) {
                    m_connectionSelection->removeSecond();
                }

                m_canvas->selectionChanged();
            }
        } else {
            m_canvas->slot_requestUpdate();
        }
        break;

    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
        if (hasLeftButton && hasSel2()) {
            if (m_connectionSelection != nullptr) {
                m_connectionSelection->setSecond(getSel2());

                if (!m_connectionSelection->isCompleteExisting()) {
                    m_connectionSelection->removeSecond();
                }

                m_canvas->selectionChanged();
            }
        }
        break;

    case CanvasMouseModeEnum::CREATE_ROOMS:
    case CanvasMouseModeEnum::NONE:
    default:
        break;
    }
    event->accept();
}

void MapWindow::handleWheel(QWheelEvent *const event)
{
    const bool hasCtrl = (event->modifiers() & Qt::ControlModifier) != 0u;

    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::MOVE:
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
    case CanvasMouseModeEnum::RAYPICK_ROOMS:
    case CanvasMouseModeEnum::SELECT_ROOMS:
    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_ROOMS:
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
        if (hasCtrl) {
            if (event->angleDelta().y() > 100) {
                m_canvas->slot_layerDown();
            } else if (event->angleDelta().y() < -100) {
                m_canvas->slot_layerUp();
            }
        } else {
            const auto zoomAndMaybeRecenter = [this, event](const int numSteps) -> bool {
                assert(numSteps != 0);
                const auto optCenter = m_canvas->getUnprojectedMouseSel(event);

                if (!optCenter) {
                    m_canvas->m_scaleFactor.logStep(numSteps);
                    return false;
                }

                const auto newCenter = optCenter->to_vec3();
                const auto oldCenter = m_canvas->getMapScreen().getCenter();

                const auto delta1 = glm::ivec2(glm::vec2(newCenter - oldCenter)
                                               * static_cast<float>(MapCanvas::SCROLL_SCALE));

                emit sig_mapMove(delta1.x, delta1.y);
                slot_mapMove(delta1.x, delta1.y);

                m_canvas->m_scaleFactor.logStep(numSteps);

                m_canvas->setViewportAndMvp(m_canvas->width(), m_canvas->height());

                if (const auto &optCenter2 = m_canvas->getUnprojectedMouseSel(event)) {
                    const auto delta2 = glm::ivec2(glm::vec2(optCenter2->to_vec3() - newCenter)
                                                   * static_cast<float>(MapCanvas::SCROLL_SCALE));

                    emit sig_mapMove(-delta2.x, -delta2.y);
                    slot_mapMove(-delta2.x, -delta2.y);
                }

                return true;
            };

            const int numSteps = event->angleDelta().y() / 120;
            if (numSteps != 0) {
                zoomAndMaybeRecenter(numSteps);
                zoomChanged();
                m_canvas->update();
            }
        }
        break;

    case CanvasMouseModeEnum::NONE:
    default:
        break;
    }

    event->accept();
}

void MapWindow::handleGesture(QGestureEvent *const event)
{
    // Zoom in / out
    QGesture *const gesture = event->gesture(Qt::PinchGesture);
    const auto *const pinch = dynamic_cast<QPinchGesture *>(gesture);
    if (pinch == nullptr) {
        return;
    }

    const QPinchGesture::ChangeFlags changeFlags = pinch->changeFlags();
    if (changeFlags & QPinchGesture::ScaleFactorChanged) {
        const auto pinchFactor = static_cast<float>(pinch->totalScaleFactor());
        m_canvas->m_scaleFactor.setPinch(pinchFactor);
    }
    if (pinch->state() == Qt::GestureFinished) {
        m_canvas->m_scaleFactor.endPinch();
        zoomChanged();
    }
    m_canvas->update();
    event->accept();
}

void MapWindow::slot_createRoom()
{
    if (!hasSel1()) {
        return;
    }

    const Coordinate c = getSel1().getCoordinate();
    if (const auto &existing = m_data.findRoomHandle(c)) {
        return;
    }

    if (m_data.createEmptyRoom(Coordinate{c.x, c.y, m_canvas->m_currentLayer})) {
        // success
    } else {
        // failed!
    }
}

void MapWindow::slot_setCanvasMouseMode(const CanvasMouseModeEnum mode)
{
    m_canvas->slot_clearAllSelections();

    switch (mode) {
    case CanvasMouseModeEnum::MOVE:
        m_canvasContainer->setCursor(Qt::OpenHandCursor);
        break;

    default:
    case CanvasMouseModeEnum::NONE:
    case CanvasMouseModeEnum::RAYPICK_ROOMS:
    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        m_canvasContainer->setCursor(Qt::CrossCursor);
        break;

    case CanvasMouseModeEnum::SELECT_ROOMS:
    case CanvasMouseModeEnum::CREATE_ROOMS:
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        m_canvasContainer->setCursor(Qt::ArrowCursor);
        break;
    }

    m_canvasMouseMode = mode;
    m_selectedArea = false;
    m_canvas->selectionChanged();
}

void MapWindow::slot_setRoomSelection(const SigRoomSelection &selection)
{
    m_canvas->slot_setRoomSelection(selection);
}

void MapWindow::slot_setConnectionSelection(const std::shared_ptr<ConnectionSelection> &selection)
{
    m_canvas->slot_setConnectionSelection(selection);
}

void MapWindow::slot_setInfomarkSelection(const std::shared_ptr<InfomarkSelection> &selection)
{
    m_canvas->slot_setInfomarkSelection(selection);
}

void MapWindow::slot_clearRoomSelection()
{
    slot_setRoomSelection(SigRoomSelection{});
}

void MapWindow::slot_clearConnectionSelection()
{
    slot_setConnectionSelection(nullptr);
}

void MapWindow::slot_clearInfomarkSelection()
{
    slot_setInfomarkSelection(nullptr);
}

void MapWindow::slot_clearAllSelections()
{
    slot_clearRoomSelection();
    slot_clearConnectionSelection();
    slot_clearInfomarkSelection();
}

void MapWindow::zoomChanged()
{
    emit sig_zoomChanged(m_canvas->getRawZoom());
}
