// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "MapCanvasData.h"

#undef near // Bad dog, Microsoft; bad dog!!!
#undef far  // Bad dog, Microsoft; bad dog!!!

#include "../configuration/configuration.h"
#include "../global/ConfigConsts.h"
#include "../map/infomark.h"
#include "../opengl/LineRendering.h"
#include "InfomarkSelection.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <optional>

#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QDebug>
#include <QNativeGestureEvent>
#include <QPointF>
#include <QTouchEvent>

const MMapper::Array<RoomTintEnum, NUM_ROOM_TINTS> &getAllRoomTints()
{
    static const MMapper::Array<RoomTintEnum, NUM_ROOM_TINTS>
        all_room_tints{RoomTintEnum::DARK, RoomTintEnum::NO_SUNDEATH};
    return all_room_tints;
}

MapCanvasInputState::MapCanvasInputState(MapData &mapData, PrespammedPath &prespammedPath)
    : m_mapData{mapData}
    , m_prespammedPath{prespammedPath}
{}

MapCanvasInputState::~MapCanvasInputState() = default;

void MapCanvasInputState::updateButtonState(const QMouseEvent *const event)
{
    m_mouseLeftPressed = (event->buttons() & Qt::LeftButton) != 0u;
    m_mouseRightPressed = (event->buttons() & Qt::RightButton) != 0u;
}

void MapCanvasInputState::updateModifierState(const QInputEvent *const event)
{
    m_ctrlPressed = (event->modifiers() & Qt::ControlModifier) != 0u;
    m_altPressed = (event->modifiers() & Qt::AltModifier) != 0u;
}

std::optional<float> MapCanvasInputState::calculatePinchDelta(const QTouchEvent *const event)
{
    const auto &points = event->points();
    if (points.size() != 2) {
        if (m_initialPinchDistance > 0.f) {
            m_initialPinchDistance = 0.f;
            m_lastPinchFactor = 1.f;
        }
        return std::nullopt;
    }

    const auto &p1 = points[0];
    const auto &p2 = points[1];

    if (event->type() == QEvent::TouchBegin || p1.state() == QEventPoint::Pressed
        || p2.state() == QEventPoint::Pressed) {
        m_initialPinchDistance = glm::distance(glm::vec2(static_cast<float>(p1.position().x()),
                                                         static_cast<float>(p1.position().y())),
                                               glm::vec2(static_cast<float>(p2.position().x()),
                                                         static_cast<float>(p2.position().y())));
        m_lastPinchFactor = 1.f;
    }

    std::optional<float> deltaFactor;
    if (m_initialPinchDistance > 1e-3f) { // PINCH_DISTANCE_THRESHOLD
        const float currentDistance = glm::distance(glm::vec2(static_cast<float>(p1.position().x()),
                                                              static_cast<float>(p1.position().y())),
                                                    glm::vec2(static_cast<float>(p2.position().x()),
                                                              static_cast<float>(
                                                                  p2.position().y())));
        const float currentPinchFactor = currentDistance / m_initialPinchDistance;
        deltaFactor = currentPinchFactor / m_lastPinchFactor;
        m_lastPinchFactor = currentPinchFactor;
    }

    if (event->type() == QEvent::TouchEnd || p1.state() == QEventPoint::Released
        || p2.state() == QEventPoint::Released) {
        m_initialPinchDistance = 0.f;
        m_lastPinchFactor = 1.f;
    }

    return deltaFactor;
}

std::optional<float> MapCanvasInputState::calculateNativeZoomDelta(
    const QNativeGestureEvent *const event)
{
    const auto value = static_cast<float>(event->value());
    float deltaFactor = 1.f;
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Mac) {
        // On macOS, event->value() for ZoomNativeGesture is the magnification delta
        // since the last event.
        deltaFactor += value;
    } else {
        // On other platforms, it's typically the cumulative scale factor (1.0 at start).
        if (event->isBeginEvent()) {
            m_lastMagnification = 1.f;
        }

        if (std::abs(m_lastMagnification) > 1e-6f) { // GESTURE_EPSILON
            deltaFactor = value / m_lastMagnification;
        }
        m_lastMagnification = value;
    }
    return deltaFactor;
}

void MapCanvasInputState::handleEscape()
{
    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::NONE:
    case CanvasMouseModeEnum::CREATE_ROOMS:
        break;

    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
        m_connectionSelection = nullptr;
        break;

    case CanvasMouseModeEnum::RAYPICK_ROOMS:
    case CanvasMouseModeEnum::SELECT_ROOMS:
        m_selectedArea = false;
        m_roomSelectionMove.reset();
        m_roomSelection.reset();
        break;

    case CanvasMouseModeEnum::MOVE:
        // special case for move: right click selects infomarks
        FALLTHROUGH;
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        m_infoMarkSelectionMove.reset();
        m_infoMarkSelection = nullptr;
        break;
    }
}

void MapCanvasInputState::updateRoomSelectionArea()
{
    if (hasSel1() && hasSel2()) {
        const Coordinate &c1 = getSel1().getCoordinate();
        const Coordinate &c2 = getSel2().getCoordinate();
        if (m_roomSelection == nullptr) {
            // add rooms to default selections
            m_roomSelection = RoomSelection::createSelection(m_mapData.findAllRooms(c1, c2));
        } else {
            auto &sel = deref(m_roomSelection);
            sel.removeMissing(m_mapData);
            // add or remove rooms to/from default selection
            const auto tmpSel = m_mapData.findAllRooms(c1, c2);
            for (const RoomId key : tmpSel) {
                if (sel.contains(key)) {
                    sel.erase(key);
                } else {
                    sel.insert(key);
                }
            }
        }
    }
}

std::shared_ptr<InfomarkSelection> MapCanvasInputState::getInfomarkSelectionAt(
    const MouseSel &sel, const MapCanvasViewport &viewport) const
{
    const auto [lo, hi] = viewport.calculateInfomarkProbeRange(sel);
    return InfomarkSelection::alloc(m_mapData, lo, hi);
}

void MapCanvasInputState::handleMoveModeRightClick(const MouseSel &sel,
                                                   const MapCanvasViewport &viewport)
{
    m_roomSelection = RoomSelection::createSelection(m_mapData.findAllRooms(sel.getCoordinate()));
    m_infoMarkSelection = getInfomarkSelectionAt(sel, viewport);
}

void MapCanvasInputState::handleRoomSelectionRelease()
{
    if (m_roomSelectionMove.has_value()) {
        const auto pos = m_roomSelectionMove->pos;
        const bool wrongPlace = m_roomSelectionMove->wrongPlace;
        m_roomSelectionMove.reset();
        if (!wrongPlace && (m_roomSelection != nullptr)) {
            const Coordinate moverel{pos, 0};
            m_mapData.applySingleChange(
                Change{room_change_types::MoveRelative2{m_roomSelection->getRoomIds(), moverel}});
        }
    } else {
        if (hasSel1() && hasSel2()) {
            const Coordinate &c1 = getSel1().getCoordinate();
            const Coordinate &c2 = getSel2().getCoordinate();
            if (m_roomSelection == nullptr) {
                m_roomSelection = RoomSelection::createSelection(m_mapData.findAllRooms(c1, c2));
            } else {
                auto &sel = deref(m_roomSelection);
                sel.removeMissing(m_mapData);
                const auto tmpSel = m_mapData.findAllRooms(c1, c2);
                for (const RoomId key : tmpSel) {
                    if (sel.contains(key)) {
                        sel.erase(key);
                    } else {
                        sel.insert(key);
                    }
                }
            }
        }
    }
    m_selectedArea = false;
}

std::optional<Coordinate> MapCanvasInputState::handleInfomarkSelectionRelease()
{
    if (hasInfomarkSelectionMove()) {
        const auto pos_copy = m_infoMarkSelectionMove->pos;
        m_infoMarkSelectionMove.reset();
        if (m_infoMarkSelection != nullptr) {
            const auto offset
                = Coordinate{(pos_copy * static_cast<float>(INFOMARK_SCALE)).truncate(), 0};
            const InfomarkSelection &sel = deref(m_infoMarkSelection);
            sel.applyOffset(offset);
            m_selectedArea = false;
            return offset;
        }
    } else if (hasSel1() && hasSel2()) {
        const auto c1 = getSel1().getScaledCoordinate(static_cast<float>(INFOMARK_SCALE));
        const auto c2 = getSel2().getScaledCoordinate(static_cast<float>(INFOMARK_SCALE));
        if (m_canvasMouseMode == CanvasMouseModeEnum::CREATE_INFOMARKS) {
            m_infoMarkSelection = InfomarkSelection::allocEmpty(m_mapData, c1, c2);
        } else {
            m_infoMarkSelection = InfomarkSelection::alloc(m_mapData, c1, c2);
        }
    }
    m_selectedArea = false;
    return std::nullopt;
}

// world space to screen space (logical pixels)
std::optional<glm::vec3> MapCanvasViewport::project(const glm::vec3 &v) const
{
    const auto tmp = m_viewProj * glm::vec4(v, 1.f);

    // This can happen if you set the layer height to the view distance
    // and then try to project a point on layer = 1, when the vertical
    // angle is 1, so the plane would pass through the camera.
    if (std::abs(tmp.w) < mmgl::W_PROJECTION_EPSILON) {
        return std::nullopt;
    }
    const auto ndc = glm::vec3{tmp} / tmp.w; /* [-1, 1]^3 if clamped */

    if (glm::any(glm::greaterThan(glm::abs(ndc), glm::vec3{1.f + mmgl::PROJECTION_EPSILON}))) {
        // result is not visible on screen.
        return std::nullopt;
    }

    const auto screen = glm::clamp(ndc * 0.5f + 0.5f, 0.f, 1.f); /* [0, 1]^3 if clamped */

    const Viewport viewport = getViewport();
    const auto mouse = glm::vec2{screen} * glm::vec2{viewport.size} + glm::vec2{viewport.offset};
    const glm::vec3 mouse_depth{mouse, screen.z};
    return mouse_depth;
}

// input: 2d mouse coordinates clamped in viewport_offset + [0..viewport_size]
// and a depth value in the range 0..1.
//
// output: world coordinates.
glm::vec3 MapCanvasViewport::unproject_raw(const glm::vec3 &mouse_depth) const
{
    return unproject_raw(mouse_depth, m_viewProj);
}

glm::vec3 MapCanvasViewport::unproject_raw(const glm::vec3 &mouse_depth,
                                           const glm::mat4 &viewProj) const
{
    const float depth = mouse_depth.z;
    assert(::isClamped(depth, 0.f, 1.f));

    const Viewport viewport = getViewport();
    const glm::vec2 mouse{mouse_depth};
    const auto screen2d = (mouse - glm::vec2{viewport.offset}) / glm::vec2{viewport.size};
    const glm::vec3 screen{screen2d, depth};
    const auto ndc = screen * 2.f - 1.f;

    const auto tmp = glm::inverse(viewProj) * glm::vec4(ndc, 1.f);
    // clamp to avoid division by zero
    constexpr float limit = 1e-6f;
    const auto w = (std::abs(tmp.w) < limit) ? std::copysign(limit, tmp.w) : tmp.w;
    const auto world = glm::vec3{tmp} / w;
    return world;
}

// Returns a value on the current layer;
// note: the returned coordinate may not be visible,
// because it could be
glm::vec3 MapCanvasViewport::unproject_clamped(const glm::vec2 &mouse) const
{
    return unproject_clamped(mouse, m_viewProj);
}

glm::vec3 MapCanvasViewport::unproject_clamped(const glm::vec2 &mouse,
                                               const glm::mat4 &viewProj) const
{
    const auto flayer = static_cast<float>(m_currentLayer);
    const auto &x = mouse.x;
    const auto &y = mouse.y;
    const auto nearPos = unproject_raw(glm::vec3{x, y, 0.f}, viewProj);
    const auto farPos = unproject_raw(glm::vec3{x, y, 1.f}, viewProj);
    const float t = (flayer - nearPos.z) / (farPos.z - nearPos.z);
    const auto result = glm::mix(nearPos, farPos, std::clamp(t, 0.f, 1.f));
    return glm::vec3{glm::vec2{result}, flayer};
}

std::optional<glm::vec2> MapCanvasViewport::getMouseCoords(const QInputEvent *const event) const
{
    if (const auto *const mouse = dynamic_cast<const QMouseEvent *>(event)) {
        const auto x = static_cast<float>(mouse->position().x());
        const auto y = static_cast<float>(height() - mouse->position().y());
        return glm::vec2{x, y};
    } else if (const auto *const wheel = dynamic_cast<const QWheelEvent *>(event)) {
        const auto x = static_cast<float>(wheel->position().x());
        const auto y = static_cast<float>(height() - wheel->position().y());
        return glm::vec2{x, y};
    } else if (const auto *const gesture = dynamic_cast<const QNativeGestureEvent *>(event)) {
        const auto x = static_cast<float>(gesture->position().x());
        const auto y = static_cast<float>(height() - gesture->position().y());
        return glm::vec2{x, y};
    } else if (const auto *const touch = dynamic_cast<const QTouchEvent *>(event)) {
        const auto &points = touch->points();
        if (points.isEmpty()) {
            return std::nullopt;
        }
        QPointF centroid(0, 0);
        for (const auto &p : points) {
            centroid += p.position();
        }
        centroid /= static_cast<qreal>(points.size());
        const auto x = static_cast<float>(centroid.x());
        const auto y = static_cast<float>(height() - centroid.y());
        return glm::vec2{x, y};
    } else {
        qWarning() << "MapCanvasViewport::getMouseCoords: unhandled event type" << event->type();
        assert(false);
        return std::nullopt;
    }
}

// input: screen coordinates;
// output is the world space intersection with the current layer
std::optional<glm::vec3> MapCanvasViewport::unproject(const QInputEvent *const event) const
{
    const auto xy = getMouseCoords(event);
    if (!xy) {
        return std::nullopt;
    }
    return unproject(*xy);
}

std::optional<glm::vec3> MapCanvasViewport::unproject(const glm::vec2 &xy) const
{
    // We don't actually know the depth we're trying to unproject;
    // technically we're solving for a ray, so we should unproject
    // two different depths and find where the ray intersects the
    // current layer.

    const auto nearPos = unproject_raw(glm::vec3{xy, 0.f});
    const auto farPos = unproject_raw(glm::vec3{xy, 1.f});
    const auto unclamped = (static_cast<float>(m_currentLayer) - nearPos.z)
                           / (farPos.z - nearPos.z);

    if (!::isClamped(unclamped, 0.f - mmgl::PROJECTION_EPSILON, 1.f + mmgl::PROJECTION_EPSILON)) {
        return std::nullopt;
    }

    // REVISIT: set the z value exactly to m_currentLayer?
    // (Note: caller ignores Z and uses integer value for current layer)
    return glm::mix(nearPos, farPos, glm::clamp(unclamped, 0.f, 1.f));
}

std::optional<MouseSel> MapCanvasViewport::getUnprojectedMouseSel(const QInputEvent *const event) const
{
    const auto xy = getMouseCoords(event);
    if (!xy) {
        return std::nullopt;
    }
    return getUnprojectedMouseSel(*xy);
}

std::optional<MouseSel> MapCanvasViewport::getUnprojectedMouseSel(const glm::vec2 &xy) const
{
    const auto opt_v = unproject(xy);
    if (!opt_v.has_value()) {
        return std::nullopt;
    }
    const glm::vec3 &v = opt_v.value();
    return MouseSel{Coordinate2f{v.x, v.y}, m_currentLayer};
}

namespace {
NODISCARD float getPitchDegrees(const float zoomScale)
{
    const float degrees = getConfig().canvas.advanced.verticalAngle.getFloat();
    if (!getConfig().canvas.advanced.autoTilt.get()) {
        return degrees;
    }

    static_assert(ScaleFactor::MAX_VALUE >= 2.f);
    return glm::smoothstep(0.5f, 2.f, zoomScale) * degrees;
}
} // namespace

float MapCanvasViewport::getPitchDegrees() const
{
    return ::getPitchDegrees(getTotalScaleFactor());
}

glm::mat4 MapCanvasViewport::getViewProj_old(const glm::ivec2 &size) const
{
    constexpr float FIXED_VIEW_DISTANCE = 60.f;
    constexpr float ROOM_Z_SCALE = 7.f;
    constexpr auto baseSize = static_cast<float>(BASESIZE);

    const float zoomScale = getTotalScaleFactor();
    const float swp = zoomScale * baseSize / static_cast<float>(size.x);
    const float shp = zoomScale * baseSize / static_cast<float>(size.y);

    QMatrix4x4 proj;
    proj.frustum(-0.5f, +0.5f, -0.5f, 0.5f, 5.f, 10000.f);
    proj.scale(swp, shp, 1.f);
    proj.translate(-m_scroll.x, -m_scroll.y, -FIXED_VIEW_DISTANCE);
    proj.scale(1.f, 1.f, ROOM_Z_SCALE);

    return glm::make_mat4(proj.constData());
}

glm::mat4 MapCanvasViewport::getViewProj(const glm::ivec2 &size) const
{
    static_assert(GLM_CONFIG_CLIP_CONTROL == GLM_CLIP_CONTROL_RH_NO);

    const int width = size.x;
    const int height = size.y;

    const float aspect = static_cast<float>(width) / static_cast<float>(height);

    const auto &advanced = getConfig().canvas.advanced;
    const float fovDegrees = advanced.fov.getFloat();
    const float zoomScale = getTotalScaleFactor();
    const auto pitchRadians = glm::radians(::getPitchDegrees(zoomScale));
    const auto yawRadians = glm::radians(advanced.horizontalAngle.getFloat());
    const auto layerHeight = advanced.layerHeight.getFloat();

    const auto pixelScale = std::invoke([aspect, fovDegrees, width]() -> float {
        constexpr float HARDCODED_LOGICAL_PIXELS = 44.f;
        const auto dummyProj = glm::perspective(glm::radians(fovDegrees), aspect, 1.f, 10.f);

        const auto centerRoomProj = glm::inverse(dummyProj) * glm::vec4(0.f, 0.f, 0.f, 1.f);
        const auto centerRoom = glm::vec3(centerRoomProj) / centerRoomProj.w;

        // Use east instead of north, so that tilted perspective matches horizontally.
        const auto oneRoomEast = dummyProj * glm::vec4(centerRoom + glm::vec3(1.f, 0.f, 0.f), 1.f);
        const float clipDist = std::abs(oneRoomEast.x / oneRoomEast.w);
        const float ndcDist = clipDist * 0.5f;

        // width is in logical pixels
        const float screenDist = ndcDist * static_cast<float>(width);
        const auto pixels = std::abs(centerRoom.z) * screenDist;
        return pixels / HARDCODED_LOGICAL_PIXELS;
    });

    const float ZSCALE = layerHeight;
    const float camDistance = pixelScale / zoomScale;
    const auto center = glm::vec3(m_scroll, static_cast<float>(m_currentLayer) * ZSCALE);

    const auto rotateHorizontal = glm::mat3(
        glm::rotate(glm::mat4(1), -yawRadians, glm::vec3(0, 0, 1)));

    const auto forward = rotateHorizontal
                         * glm::vec3(0.f, std::sin(pitchRadians), -std::cos(pitchRadians));
    const auto right = rotateHorizontal * glm::vec3(1, 0, 0);
    const auto up = glm::cross(right, glm::normalize(forward));

    const auto eye = center - camDistance * forward;

    const auto proj = glm::perspective(glm::radians(fovDegrees), aspect, 0.25f, 1024.f);
    const auto view = glm::lookAt(eye, center, up);
    const auto scaleZ = glm::scale(glm::mat4(1), glm::vec3(1.f, 1.f, ZSCALE));

    return proj * view * scaleZ;
}

void MapCanvasViewport::updateViewProj(const bool want3D)
{
    const glm::ivec2 size{width(), height()};
    m_viewProj = (!want3D) ? getViewProj_old(size) : getViewProj(size);
}

void MapCanvasViewport::zoomAt(const float factor, const glm::vec2 &mousePos, const bool want3D)
{
    const auto optWorldPos = unproject(mousePos);
    if (!optWorldPos) {
        m_scaleFactor *= factor;
        updateViewProj(want3D);
        return;
    }

    const glm::vec2 worldPos = glm::vec2(*optWorldPos);
    const glm::vec2 oldScroll = m_scroll;

    m_scaleFactor *= factor;
    updateViewProj(want3D);

    const auto optNewWorldPos = unproject(mousePos);
    if (optNewWorldPos) {
        const glm::vec2 delta = worldPos - glm::vec2(*optNewWorldPos);
        m_scroll = oldScroll + delta;
    } else {
        m_scroll = oldScroll;
    }

    updateViewProj(want3D);
}

void MapCanvasViewport::centerOn(const Coordinate &pos)
{
    m_currentLayer = pos.z;
    m_scroll = pos.to_vec2() + glm::vec2{0.5f, 0.5f};
}

std::pair<int, int> MapCanvasViewport::calculateContinuousScroll(const glm::vec2 &mousePos) const
{
    const int h = height();
    const int w = width();
    const int marginV = std::min(100, h / 4);
    const int marginH = std::min(100, w / 4);

    int vScroll = 0;
    const auto y = static_cast<int>(static_cast<float>(h) - mousePos.y);
    if (y < marginV) {
        vScroll = SCROLL_SCALE;
    } else if (y > h - marginV) {
        vScroll = -SCROLL_SCALE;
    }

    int hScroll = 0;
    const auto x = static_cast<int>(mousePos.x);
    if (x < marginH) {
        hScroll = -SCROLL_SCALE;
    } else if (x > w - marginH) {
        hScroll = SCROLL_SCALE;
    }

    return {hScroll, vScroll};
}

bool MapCanvasViewport::performPanning(const glm::vec2 &mousePos,
                                       const glm::vec3 &startWorldPos,
                                       const glm::vec2 &startScroll,
                                       const glm::mat4 &startViewProj)
{
    const glm::vec3 currWorldPos = unproject_clamped(mousePos, startViewProj);
    const glm::vec2 delta = glm::vec2(currWorldPos - startWorldPos);

    if (glm::length(delta) > 1e-6f) { // GESTURE_EPSILON
        m_scroll = startScroll - delta;
        return true;
    }
    return false;
}

bool MapCanvasViewport::applyRotationDelta(const int dx, const int dy)
{
    auto &conf = setConfig().canvas.advanced;
    bool angleChanged = false;
    constexpr float SENSITIVITY = 0.3f;

    if (dx != 0) {
        conf.horizontalAngle.set(conf.horizontalAngle.get()
                                 + static_cast<int>(static_cast<float>(dx) * SENSITIVITY));
        angleChanged = true;
    }

    if (!conf.autoTilt.get()) {
        if (dy != 0) {
            conf.verticalAngle.set(conf.verticalAngle.get()
                                   + static_cast<int>(static_cast<float>(-dy) * SENSITIVITY));
            angleChanged = true;
        }
    }
    return angleChanged;
}

std::vector<Coordinate> MapCanvasViewport::calculateRaypickCoordinates(const glm::vec2 &xy) const
{
    const auto nearPos = unproject_raw(glm::vec3{xy, 0.f});
    const auto farPos = unproject_raw(glm::vec3{xy, 1.f});

    const auto hiz = static_cast<int>(std::floor(nearPos.z));
    const auto loz = static_cast<int>(std::ceil(farPos.z));
    if (hiz <= loz) {
        return {};
    }

    const auto inv_denom = 1.f / (farPos.z - nearPos.z);
    std::vector<Coordinate> coords;
    for (int z = hiz; z >= loz; --z) {
        const float t = (static_cast<float>(z) - nearPos.z) * inv_denom;
        if (::isClamped(t, 0.f, 1.f)) {
            const auto pos = glm::mix(nearPos, farPos, t);
            coords.emplace_back(MouseSel{Coordinate2f{pos.x, pos.y}, z}.getCoordinate());
        }
    }
    return coords;
}

std::pair<Coordinate, Coordinate> MapCanvasViewport::calculateInfomarkProbeRange(
    const MouseSel &sel) const
{
    static constexpr float CLICK_RADIUS = 10.f;

    const auto center = sel.to_vec3();
    const auto optClickPoint = project(center);
    if (!optClickPoint) {
        // This distance is in world space, but the mouse click is in screen space.
        static_assert(INFOMARK_SCALE % 5 == 0);
        static constexpr auto INFOMARK_CLICK_RADIUS = INFOMARK_SCALE / 5;
        const auto pos = sel.getScaledCoordinate(INFOMARK_SCALE);
        const auto lo = pos + Coordinate{-INFOMARK_CLICK_RADIUS, -INFOMARK_CLICK_RADIUS, 0};
        const auto hi = pos + Coordinate{+INFOMARK_CLICK_RADIUS, +INFOMARK_CLICK_RADIUS, 0};
        return {lo, hi};
    }

    const glm::vec2 clickPoint = glm::vec2{optClickPoint.value()};
    glm::vec3 maxCoord = center;
    glm::vec3 minCoord = center;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            const auto coord = unproject_clamped(clickPoint
                                                 + glm::vec2{static_cast<float>(dx) * CLICK_RADIUS,
                                                             static_cast<float>(dy) * CLICK_RADIUS});
            maxCoord = glm::max(maxCoord, coord);
            minCoord = glm::min(minCoord, coord);
        }
    }

    const auto getScaled = [this](const glm::vec3 &c) -> Coordinate {
        const auto pos = glm::ivec3(glm::vec2(c) * static_cast<float>(INFOMARK_SCALE),
                                    m_currentLayer);
        return Coordinate{pos.x, pos.y, pos.z};
    };

    return {getScaled(minCoord), getScaled(maxCoord)};
}

glm::vec3 MapCanvasViewport::getCenter() const
{
    const Viewport vp = getViewport();
    return unproject_clamped(glm::vec2{vp.offset} + glm::vec2{vp.size} * 0.5f);
}

bool MapCanvasViewport::isRoomVisible(const Coordinate &c, const float marginPixels) const
{
    const auto pos = c.to_vec3();
    for (int i = 0; i < 4; ++i) {
        const glm::vec3 offset{static_cast<float>(i & 1), static_cast<float>((i >> 1) & 1), 0.f};
        const auto corner = pos + offset;
        switch (testVisibility(corner, marginPixels)) {
        case VisiblityResultEnum::INSIDE_MARGIN:
        case VisiblityResultEnum::ON_MARGIN:
            break;

        case VisiblityResultEnum::OUTSIDE_MARGIN:
        case VisiblityResultEnum::OFF_SCREEN:
            return false;
        }
    }

    return true;
}

// Purposely ignores the possibility of glClipPlane() and glDepthRange().
MapCanvasViewport::VisiblityResultEnum MapCanvasViewport::testVisibility(
    const glm::vec3 &input_pos, const float marginPixels) const
{
    assert(marginPixels >= 1.f);

    const auto opt_mouse = project(input_pos);
    if (!opt_mouse.has_value()) {
        return VisiblityResultEnum::OFF_SCREEN;
    }

    // NOTE: From now on, we'll ignore depth because we know it's "on screen."
    const auto vp = getViewport();
    const glm::vec2 offset{vp.offset};
    const glm::vec2 size{vp.size};
    const glm::vec2 half_size = size * 0.5f;
    const auto &mouse_depth = opt_mouse.value();
    const glm::vec2 mouse = glm::vec2(mouse_depth) - offset;

    // for height 480, height/2 is 240, and then:
    // e.g.
    //   240 - abs(5 - 240) = 5 pixels
    //   240 - abs(475 - 240) = 5 pixels
    const glm::vec2 d = half_size - glm::abs(mouse - half_size);

    // We want the minimum value (closest to the edge).
    const auto dist = std::min(d.x, d.y);

    // e.g. if margin is 20.0, then floorMargin is 20, and ceilMargin is 21.0
    const float floorMargin = std::floor(marginPixels);
    const float ceilMargin = floorMargin + 1.f;

    // larger values are "more inside".
    // e.g.
    //   distance 5 vs margin 20 is "outside",
    //   distance 25 vs margin 20 is "inside".
    if (dist < floorMargin) {
        return VisiblityResultEnum::OUTSIDE_MARGIN;
    } else if (dist > ceilMargin) {
        return VisiblityResultEnum::INSIDE_MARGIN;
    } else {
        return VisiblityResultEnum::ON_MARGIN;
    }
}

glm::vec3 MapCanvasViewport::getProxyLocation(const glm::vec3 &input_pos,
                                              const float marginPixels) const
{
    const auto center = getCenter();

    const VisiblityResultEnum abs_result = testVisibility(input_pos, marginPixels);
    switch (abs_result) {
    case VisiblityResultEnum::INSIDE_MARGIN:
    case VisiblityResultEnum::ON_MARGIN:
        return input_pos;
    case VisiblityResultEnum::OUTSIDE_MARGIN:
    case VisiblityResultEnum::OFF_SCREEN:
        break;
    }

    float proxyFraction = 0.5f;
    float stepFraction = 0.25f;
    constexpr int maxSteps = 23;
    glm::vec3 bestInside = center;
    float bestInsideFraction = 0.f;
    // clang-tidy is confused here. The loop induction variable is an integer.
    for (int i = 0; i < maxSteps; ++i, stepFraction *= 0.5f) {
        const auto tmp_pos = glm::mix(center, input_pos, proxyFraction);
        const VisiblityResultEnum tmp_result = testVisibility(tmp_pos, marginPixels);
        switch (tmp_result) {
        case VisiblityResultEnum::INSIDE_MARGIN:
            // Once we've hit "inside", math tells us it should never end up
            // hitting inside with a lower value, but let's guard just to be safe.
            assert(proxyFraction > bestInsideFraction);
            if (proxyFraction > bestInsideFraction) {
                bestInside = tmp_pos;
                bestInsideFraction = proxyFraction;
            }
            proxyFraction += stepFraction;
            break;
        case VisiblityResultEnum::ON_MARGIN:
            return tmp_pos;
        case VisiblityResultEnum::OUTSIDE_MARGIN:
        case VisiblityResultEnum::OFF_SCREEN:
            proxyFraction -= stepFraction;
            break;
        }
    }

    // This really should never happen, because it means we visited 23 bits
    // of mantissa without finding one on the margin.
    // Here's our best guess.
    return bestInside;
}
