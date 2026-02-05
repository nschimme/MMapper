#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/EnumIndexedArray.h"
#include "../map/ExitDirection.h"
#include "../map/mmapper2room.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/roomselection.h"
#include "../opengl/OpenGL.h"
#include "CanvasMouseModeEnum.h"
#include "RoadIndex.h"
#include "connectionselection.h"
#include "prespammedpath.h"

#include <map>
#include <memory>
#include <optional>
#include <unordered_map>

#include <QOpenGLTexture>
#include <QtGui/QMatrix4x4>
#include <QtGui/QMouseEvent>
#include <QtGui/qopengl.h>
#include <QtGui>

class ConnectionSelection;
class MapData;
class PrespammedPath;
class InfomarkSelection;

enum class NODISCARD RoomTintEnum { DARK, NO_SUNDEATH };
static const size_t NUM_ROOM_TINTS = 2;
NODISCARD extern const MMapper::Array<RoomTintEnum, NUM_ROOM_TINTS> &getAllRoomTints();
#define ALL_ROOM_TINTS getAllRoomTints()

static constexpr int BASESIZE = 32;

struct NODISCARD ScaleFactor final
{
public:
    static constexpr const float ZOOM_STEP = 1.175f;
    static constexpr int MIN_VALUE_HUNDREDTHS = 4;
    static constexpr int MAX_VALUE_INT = 5;
    static constexpr float MIN_VALUE = static_cast<float>(MIN_VALUE_HUNDREDTHS) * 0.01f;
    static constexpr float MAX_VALUE = static_cast<float>(MAX_VALUE_INT);

private:
    float m_scaleFactor = 1.f;
    float m_pinchFactor = 1.f;

private:
    NODISCARD static float clamp(float x) { assert(std::isfinite(x)); return std::clamp(x, MIN_VALUE, MAX_VALUE); }

public:
    NODISCARD static bool isClamped(float x) { return ::isClamped(x, MIN_VALUE, MAX_VALUE); }
    NODISCARD float getRaw() const { return clamp(m_scaleFactor); }
    NODISCARD float getTotal() const { return clamp(m_scaleFactor * m_pinchFactor); }
    void set(const float scale) { m_scaleFactor = clamp(scale); }
    void setPinch(const float pinch) { m_pinchFactor = pinch; }
    void endPinch() { m_scaleFactor = getTotal(); m_pinchFactor = 1.f; }
    void reset() { *this = ScaleFactor(); }
    ScaleFactor &operator*=(const float ratio) { set(m_scaleFactor * ratio); return *this; }
    void logStep(const int numSteps) { if (numSteps != 0) *this *= std::pow(ZOOM_STEP, static_cast<float>(numSteps)); }
};

struct NODISCARD MapCanvasViewport
{
    glm::mat4 m_viewProj{1.f};
    glm::vec2 m_scroll{0.f};
    ScaleFactor m_scaleFactor;
    int m_currentLayer = 0;
    Viewport m_viewportRect{{0,0},{0,0}};

    explicit MapCanvasViewport() = default;

    NODISCARD auto width() const { return m_viewportRect.size.x; }
    NODISCARD auto height() const { return m_viewportRect.size.y; }
    NODISCARD Viewport getViewport() const { return m_viewportRect; }
    NODISCARD float getTotalScaleFactor() const { return m_scaleFactor.getTotal(); }

    NODISCARD std::optional<glm::vec3> project(const glm::vec3 &) const;
    NODISCARD glm::vec3 unproject_raw(const glm::vec3 &) const;
    NODISCARD glm::vec3 unproject_clamped(const glm::vec2 &) const;
    NODISCARD std::optional<glm::vec3> unproject(const QInputEvent *event) const;
    NODISCARD std::optional<MouseSel> getUnprojectedMouseSel(const QInputEvent *event) const;
    NODISCARD glm::vec2 getMouseCoords(const QInputEvent *event) const;
};

class NODISCARD MapScreen final
{
public:
    static constexpr const float DEFAULT_MARGIN_PIXELS = 24.f;
private:
    const MapCanvasViewport &m_viewport;
    enum class NODISCARD VisiblityResultEnum { INSIDE_MARGIN, ON_MARGIN, OUTSIDE_MARGIN, OFF_SCREEN };
public:
    explicit MapScreen(const MapCanvasViewport &);
    ~MapScreen();
    DELETE_CTORS_AND_ASSIGN_OPS(MapScreen);
    NODISCARD glm::vec3 getCenter() const;
    NODISCARD bool isRoomVisible(const Coordinate &c, float margin) const;
    NODISCARD glm::vec3 getProxyLocation(const glm::vec3 &pos, float margin) const;
private:
    NODISCARD VisiblityResultEnum testVisibility(const glm::vec3 &input_pos, float margin) const;
};

struct NODISCARD MapCanvasInputState
{
    CanvasMouseModeEnum m_canvasMouseMode = CanvasMouseModeEnum::MOVE;
    bool m_mouseRightPressed = false;
    bool m_mouseLeftPressed = false;
    bool m_altPressed = false;
    bool m_ctrlPressed = false;
    std::optional<MouseSel> m_sel1, m_sel2, m_moveBackup;
    bool m_selectedArea = false;
    SharedRoomSelection m_roomSelection;
    struct NODISCARD RoomSelMove final { Coordinate2i pos; bool wrongPlace = false; };
    std::optional<RoomSelMove> m_roomSelectionMove;
    std::shared_ptr<InfomarkSelection> m_infoMarkSelection;
    struct NODISCARD InfomarkSelectionMove final { Coordinate2f pos; };
    std::optional<InfomarkSelectionMove> m_infoMarkSelectionMove;
    std::shared_ptr<ConnectionSelection> m_connectionSelection;
    PrespammedPath &m_prespammedPath;

    explicit MapCanvasInputState(PrespammedPath &prespammedPath);
    ~MapCanvasInputState();
    NODISCARD bool hasSel1() const { return m_sel1.has_value(); }
    NODISCARD bool hasSel2() const { return m_sel2.has_value(); }
    NODISCARD bool hasBackup() const { return m_moveBackup.has_value(); }
    NODISCARD MouseSel getSel1() const { return m_sel1.value(); }
    NODISCARD MouseSel getSel2() const { return m_sel2.value(); }
    NODISCARD MouseSel getBackup() const { return m_moveBackup.value(); }
    void startMoving(const MouseSel &startPos) { m_moveBackup = startPos; }
    void stopMoving() { m_moveBackup.reset(); }
};
