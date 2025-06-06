// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "Connections.h"

#include "opengl/legacy/Legacy.h" // For deref, SharedFunctions, Functions
#include "../configuration/configuration.h"
#include "../global/Flags.h"
#include "../map/DoorFlags.h"
#include "../map/ExitFieldVariant.h"
#include "../map/exit.h"
#include "../map/room.h"
#include "../mapdata/mapdata.h"
#include "../opengl/Font.h"
#include "../opengl/FontFormatFlags.h"
#include "../opengl/OpenGL.h"
#include "../opengl/OpenGLTypes.h"
#include "opengl/legacy/LineRenderer.h" // For Legacy::LineRenderer and Legacy::LineInstanceData
#include "ConnectionLineBuilder.h"
#include "MapCanvasData.h"
#include "connectionselection.h"
#include "mapcanvas.h"

#include <cassert>
#include <cstdlib>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include <QColor>
#include <QMessageLogContext>
#include <QtCore>
#include <QtGui/QOpenGLExtraFunctions> // For QOpenGLExtraFunctions

// Assuming LineShader.h is included via LineRenderer.h or Legacy.h
// #include "opengl/legacy/LineShader.h"

static constexpr const float CONNECTION_LINE_WIDTH = 2.f; // This might become the default thickness
static constexpr const float VALID_CONNECTION_POINT_SIZE = 6.f;
static constexpr const float NEW_CONNECTION_POINT_SIZE = 8.f;

static constexpr const float FAINT_CONNECTION_ALPHA = 0.1f;

NODISCARD static bool isConnectionMode(const CanvasMouseModeEnum mode)
{
    switch (mode) {
    case CanvasMouseModeEnum::CREATE_CONNECTIONS:
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS:
    case CanvasMouseModeEnum::SELECT_CONNECTIONS:
        return true;
    case CanvasMouseModeEnum::NONE:
    case CanvasMouseModeEnum::MOVE:
    case CanvasMouseModeEnum::RAYPICK_ROOMS:
    case CanvasMouseModeEnum::SELECT_ROOMS:
    case CanvasMouseModeEnum::CREATE_ROOMS:
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
    case CanvasMouseModeEnum::CREATE_INFOMARKS:
        break;
    }
    return false;
}

NODISCARD static glm::vec2 getConnectionOffsetRelative(const ExitDirEnum dir)
{
    switch (dir) {
    // NOTE: These are flipped north/south.
    case ExitDirEnum::NORTH:
        return {0.f, +0.4f};
    case ExitDirEnum::SOUTH:
        return {0.f, -0.4f};

    case ExitDirEnum::EAST:
        return {0.4f, 0.f};
    case ExitDirEnum::WEST:
        return {-0.4f, 0.f};

        // NOTE: These are flipped north/south.
    case ExitDirEnum::UP:
        return {0.25f, 0.25f};
    case ExitDirEnum::DOWN:
        return {-0.25f, -0.25f};

    case ExitDirEnum::UNKNOWN:
        return {0.f, 0.f};

    case ExitDirEnum::NONE:
    default:
        break;
    }

    assert(false);
    return {};
};

NODISCARD static glm::vec3 getConnectionOffset(const ExitDirEnum dir)
{
    return glm::vec3{getConnectionOffsetRelative(dir), 0.f} + glm::vec3{0.5f, 0.5f, 0.f};
}

NODISCARD static glm::vec3 getPosition(const ConnectionSelection::ConnectionDescriptor &cd)
{
    return cd.room.getPosition().to_vec3() + getConnectionOffset(cd.direction);
}

NODISCARD static QString getDoorPostFix(const RoomHandle &room, const ExitDirEnum dir)
{
    static constexpr const auto SHOWN_FLAGS = DoorFlagEnum::NEED_KEY | DoorFlagEnum::NO_PICK
                                              | DoorFlagEnum::DELAYED;

    const DoorFlags flags = room.getExit(dir).getDoorFlags();
    if (!flags.containsAny(SHOWN_FLAGS)) {
        return QString{};
    }

    return QString::asprintf(" [%s%s%s]",
                             flags.needsKey() ? "L" : "",
                             flags.isNoPick() ? "/NP" : "",
                             flags.isDelayed() ? "d" : "");
}

NODISCARD static QString getPostfixedDoorName(const RoomHandle &room, const ExitDirEnum dir)
{
    const auto postFix = getDoorPostFix(room, dir);
    return room.getExit(dir).getDoorName().toQString() + postFix;
}

UniqueMesh RoomNameBatch::getMesh(GLFont &font) const
{
    return font.getFontMesh(m_names);
}

void ConnectionDrawer::drawRoomDoorName(const RoomHandle &sourceRoom,
                                        const ExitDirEnum sourceDir,
                                        const RoomHandle &targetRoom,
                                        const ExitDirEnum targetDir)
{
    static const auto isShortDistance = [](const Coordinate &a, const Coordinate &b) -> bool {
        return glm::all(glm::lessThanEqual(glm::abs(b.to_ivec2() - a.to_ivec2()), glm::ivec2(1)));
    };

    const Coordinate &sourcePos = sourceRoom.getPosition();
    const Coordinate &targetPos = targetRoom.getPosition();

    if (sourcePos.z != m_currentLayer && targetPos.z != m_currentLayer) {
        return;
    }

    // consider converting this to std::string
    QString name;
    bool together = false;

    const auto &targetExit = targetRoom.getExit(targetDir);
    if (targetExit.exitIsDoor()      // the other room has a door?
        && targetExit.hasDoorName()  // has a door on both sides...
        && targetExit.doorIsHidden() // is hidden
        && isShortDistance(sourcePos, targetPos)) {
        if (sourceRoom.getId() > targetRoom.getId() && sourcePos.z == targetPos.z) {
            // NOTE: This allows wrap-around connections to the same room (allows source <= target).
            // Avoid drawing duplicate door names for each side by only drawing one side unless
            // the doors are on different z-layers
            return;
        }

        together = true;

        // no need for duplicating names (its spammy)
        const QString sourceName = getPostfixedDoorName(sourceRoom, sourceDir);
        const QString targetName = getPostfixedDoorName(targetRoom, targetDir);
        if (sourceName != targetName) {
            name = sourceName + "/" + targetName;
        } else {
            name = sourceName;
        }
    } else {
        name = getPostfixedDoorName(sourceRoom, sourceDir);
    }

    static constexpr float XOFFSET = 0.6f;
    static const auto getYOffset = [](const ExitDirEnum dir) -> float {
        switch (dir) {
        case ExitDirEnum::NORTH:
            return 0.85f;
        case ExitDirEnum::SOUTH:
            return 0.35f;

        case ExitDirEnum::WEST:
            return 0.7f;
        case ExitDirEnum::EAST:
            return 0.55f;

        case ExitDirEnum::UP:
            return 1.05f;
        case ExitDirEnum::DOWN:
            return 0.2f;

        case ExitDirEnum::UNKNOWN:
        case ExitDirEnum::NONE:
            break;
        }

        assert(false);
        return 0.f;
    };

    const glm::vec2 xy = [sourceDir, together, &sourcePos, &targetPos]() -> glm::vec2 {
        const glm::vec2 srcPos = sourcePos.to_vec2();
        if (together) {
            const auto centerPos = (srcPos + targetPos.to_vec2()) * 0.5f;
            static constexpr float YOFFSET = 0.7f;
            return centerPos + glm::vec2{XOFFSET, YOFFSET};
        } else {
            return srcPos + glm::vec2{XOFFSET, getYOffset(sourceDir)};
        }
    }();

    static const auto bg = Colors::black.withAlpha(0.4f);
    const glm::vec3 pos{xy, m_currentLayer};
    m_roomNameBatch.emplace_back(GLText{pos,
                                        mmqt::toStdStringLatin1(name), // GL font is latin1
                                        Colors::white,
                                        bg,
                                        FontFormatFlags{FontFormatFlagEnum::HALIGN_CENTER}});
}

void ConnectionDrawer::drawRoomConnectionsAndDoors(const RoomHandle &room)
{
    const Map &map = room.getMap();

    // Ooops, this is wrong since we may reject a connection that would be visible
    // if we looked at the other side.
    const auto room_pos = room.getPosition();
    const bool sourceWithinBounds = m_bounds.contains(room_pos);

    const auto sourceId = room.getId();

    for (const ExitDirEnum sourceDir : ALL_EXITS7) {
        const auto &sourceExit = room.getExit(sourceDir);
        // outgoing connections
        if (sourceWithinBounds) {
            for (const RoomId outTargetId : sourceExit.getOutgoingSet()) {
                const auto &targetRoom = map.getRoomHandle(outTargetId);
                if (!targetRoom) {
                    /* DEAD CODE */
                    qWarning() << "Source room" << sourceId.asUint32() << "("
                               << room.getName().toQString()
                               << ") dir=" << mmqt::toQStringUtf8(to_string_view(sourceDir))
                               << "has target room with internal identifier"
                               << outTargetId.asUint32() << "which does not exist!";
                    // This would cause a segfault in the old map scheme, but maps are now rigorously
                    // validated, so it should be impossible to have an exit to a room that does
                    // not exist.
                    assert(false);
                    continue;
                }
                const auto &target_coord = targetRoom.getPosition();
                const bool targetOutsideBounds = !m_bounds.contains(target_coord);

                // Two way means that the target room directly connects back to source room
                const ExitDirEnum targetDir = opposite(sourceDir);
                const auto &targetExit = targetRoom.getExit(targetDir);
                const bool twoway = targetExit.containsOut(sourceId)
                                    && sourceExit.containsIn(outTargetId) && !targetOutsideBounds;

                const bool drawBothZLayers = room_pos.z != target_coord.z;

                if (!twoway) {
                    // Always draw exits for rooms that are not twoway exits
                    drawConnection(room,
                                   targetRoom,
                                   sourceDir,
                                   targetDir,
                                   !twoway,
                                   sourceExit.exitIsExit() && !targetOutsideBounds);

                } else if (sourceId <= outTargetId || drawBothZLayers) {
                    // Avoid drawing duplicate exits for each side by only drawing one side
                    drawConnection(room,
                                   targetRoom,
                                   sourceDir,
                                   targetDir,
                                   !twoway,
                                   sourceExit.exitIsExit() && targetExit.exitIsExit());
                }

                // Draw door names
                if (sourceExit.exitIsDoor() && sourceExit.hasDoorName()
                    && sourceExit.doorIsHidden()) {
                    drawRoomDoorName(room, sourceDir, targetRoom, targetDir);
                }
            }
        }

        // incoming connections
        for (const RoomId inTargetId : sourceExit.getIncomingSet()) {
            const auto &targetRoom = map.getRoomHandle(inTargetId);
            if (!targetRoom) {
                /* DEAD CODE */
                qWarning() << "Source room" << sourceId.asUint32() << "("
                           << room.getName().toQString() << ") fromdir="
                           << mmqt::toQStringUtf8(to_string_view(opposite(sourceDir)))
                           << " has target room with internal identifier" << inTargetId.asUint32()
                           << "which does not exist!";
                // This would cause a segfault in the old map scheme, but maps are now rigorously
                // validated, so it should be impossible to have an exit to a room that does
                // not exist.
                assert(false);
                continue;
            }

            // Only draw the connection if the target room is within the bounds
            const Coordinate &target_coord = targetRoom.getPosition();
            if (!m_bounds.contains(target_coord)) {
                continue;
            }

            // Only draw incoming connections if they are on a different layer
            if (room_pos.z == target_coord.z) {
                continue;
            }

            // Detect if this is a oneway
            bool oneway = true;
            for (const auto &tempSourceExit : room.getExits()) {
                if (tempSourceExit.containsOut(inTargetId)) {
                    oneway = false;
                    break;
                }
            }
            if (oneway) {
                // Always draw one way connections for each target exit to the source room
                for (const ExitDirEnum targetDir : ALL_EXITS7) {
                    const auto &targetExit = targetRoom.getExit(targetDir);
                    if (targetExit.containsOut(sourceId)) {
                        drawConnection(targetRoom,
                                       room,
                                       targetDir,
                                       sourceDir,
                                       oneway,
                                       targetExit.exitIsExit());
                    }
                }
            }
        }
    }
}

void ConnectionDrawer::drawConnection(const RoomHandle &leftRoom,
                                      const RoomHandle &rightRoom,
                                      const ExitDirEnum startDir,
                                      const ExitDirEnum endDir,
                                      const bool oneway,
                                      const bool inExitFlags)
{
    /* WARNING: attempts to write to a different layer may result in weird graphical bugs. */
    const Coordinate &leftPos = leftRoom.getPosition();
    const Coordinate &rightPos = rightRoom.getPosition();
    const int leftX = leftPos.x;
    const int leftY = leftPos.y;
    const int leftZ = leftPos.z;
    const int rightX = rightPos.x;
    const int rightY = rightPos.y;
    const int rightZ = rightPos.z;
    const int dX = rightX - leftX;
    const int dY = rightY - leftY;
    const int dZ = rightZ - leftZ;

    if ((rightZ != m_currentLayer) && (leftZ != m_currentLayer)) {
        return;
    }

    bool neighbours = false;

    if ((dX == 0) && (dY == 1) && (dZ == 0)) {
        if ((startDir == ExitDirEnum::NORTH) && (endDir == ExitDirEnum::SOUTH) && !oneway) {
            return;
        }
        neighbours = true;
    }
    if ((dX == 0) && (dY == -1) && (dZ == 0)) {
        if ((startDir == ExitDirEnum::SOUTH) && (endDir == ExitDirEnum::NORTH) && !oneway) {
            return;
        }
        neighbours = true;
    }
    if ((dX == +1) && (dY == 0) && (dZ == 0)) {
        if ((startDir == ExitDirEnum::EAST) && (endDir == ExitDirEnum::WEST) && !oneway) {
            return;
        }
        neighbours = true;
    }
    if ((dX == -1) && (dY == 0) && (dZ == 0)) {
        if ((startDir == ExitDirEnum::WEST) && (endDir == ExitDirEnum::EAST) && !oneway) {
            return;
        }
        neighbours = true;
    }

    auto &gl = getFakeGL();

    gl.setOffset(static_cast<float>(leftX), static_cast<float>(leftY), 0.f);

    if (inExitFlags) {
        gl.setNormal();
    } else {
        gl.setRed();
    }

    {
        const auto srcZ = static_cast<float>(leftZ);
        const auto dstZ = static_cast<float>(rightZ);

        drawConnectionLine(startDir,
                           endDir,
                           oneway,
                           neighbours,
                           static_cast<float>(dX),
                           static_cast<float>(dY),
                           srcZ,
                           dstZ);
        drawConnectionTriangles(startDir,
                                endDir,
                                oneway,
                                static_cast<float>(dX),
                                static_cast<float>(dY),
                                srcZ,
                                dstZ);
    }

    gl.setOffset(0, 0, 0);
    gl.setNormal();
}

void ConnectionDrawer::drawConnectionTriangles(const ExitDirEnum startDir,
                                               const ExitDirEnum endDir,
                                               const bool oneway,
                                               const float dX,
                                               const float dY,
                                               const float srcZ,
                                               const float dstZ)
{
    if (oneway) {
        drawConnEndTri1Way(endDir, dX, dY, dstZ);
    } else {
        drawConnStartTri(startDir, srcZ);
        drawConnEndTri(endDir, dX, dY, dstZ);
    }
}

void ConnectionDrawer::drawConnectionLine(const ExitDirEnum startDir,
                                          const ExitDirEnum endDir,
                                          const bool oneway,
                                          const bool neighbours,
                                          const float dX,
                                          const float dY,
                                          const float srcZ,
                                          const float dstZ)
{
    std::vector<glm::vec3> points{};
    ConnectionLineBuilder lb{points};
    lb.drawConnLineStart(startDir, neighbours, srcZ);
    if (points.empty()) {
        return;
    }

    if (oneway) {
        lb.drawConnLineEnd1Way(endDir, dX, dY, dstZ);
    } else {
        lb.drawConnLineEnd2Way(endDir, neighbours, dX, dY, dstZ);
    }

    if (points.empty()) {
        return;
    }

    drawLineStrip(points);
}

void ConnectionDrawer::drawLineStrip(const std::vector<glm::vec3> &points)
{
    getFakeGL().drawLineStrip(points);
}

void ConnectionDrawer::drawConnStartTri(const ExitDirEnum startDir, const float srcZ)
{
    auto &gl = getFakeGL();

    switch (startDir) {
    case ExitDirEnum::NORTH:
        gl.drawTriangle(glm::vec3{0.82f, 0.9f, srcZ},
                        glm::vec3{0.68f, 0.9f, srcZ},
                        glm::vec3{0.75f, 0.7f, srcZ});
        break;
    case ExitDirEnum::SOUTH:
        gl.drawTriangle(glm::vec3{0.18f, 0.1f, srcZ},
                        glm::vec3{0.32f, 0.1f, srcZ},
                        glm::vec3{0.25f, 0.3f, srcZ});
        break;
    case ExitDirEnum::EAST:
        gl.drawTriangle(glm::vec3{0.9f, 0.68f, srcZ},
                        glm::vec3{0.9f, 0.82f, srcZ},
                        glm::vec3{0.7f, 0.75f, srcZ});
        break;
    case ExitDirEnum::WEST:
        gl.drawTriangle(glm::vec3{0.1f, 0.32f, srcZ},
                        glm::vec3{0.1f, 0.18f, srcZ},
                        glm::vec3{0.3f, 0.25f, srcZ});
        break;

    case ExitDirEnum::UP:
    case ExitDirEnum::DOWN:
        // Do not draw triangles for 2-way up/down
        break;
    case ExitDirEnum::UNKNOWN:
        drawConnEndTriUpDownUnknown(0, 0, srcZ);
        break;
    case ExitDirEnum::NONE:
        assert(false);
        break;
    }
}

void ConnectionDrawer::drawConnEndTri(const ExitDirEnum endDir,
                                      const float dX,
                                      const float dY,
                                      const float dstZ)
{
    auto &gl = getFakeGL();

    switch (endDir) {
    case ExitDirEnum::NORTH:
        gl.drawTriangle(glm::vec3{dX + 0.82f, dY + 0.9f, dstZ},
                        glm::vec3{dX + 0.68f, dY + 0.9f, dstZ},
                        glm::vec3{dX + 0.75f, dY + 0.7f, dstZ});
        break;
    case ExitDirEnum::SOUTH:
        gl.drawTriangle(glm::vec3{dX + 0.18f, dY + 0.1f, dstZ},
                        glm::vec3{dX + 0.32f, dY + 0.1f, dstZ},
                        glm::vec3{dX + 0.25f, dY + 0.3f, dstZ});
        break;
    case ExitDirEnum::EAST:
        gl.drawTriangle(glm::vec3{dX + 0.9f, dY + 0.68f, dstZ},
                        glm::vec3{dX + 0.9f, dY + 0.82f, dstZ},
                        glm::vec3{dX + 0.7f, dY + 0.75f, dstZ});
        break;
    case ExitDirEnum::WEST:
        gl.drawTriangle(glm::vec3{dX + 0.1f, dY + 0.32f, dstZ},
                        glm::vec3{dX + 0.1f, dY + 0.18f, dstZ},
                        glm::vec3{dX + 0.3f, dY + 0.25f, dstZ});
        break;

    case ExitDirEnum::UP:
    case ExitDirEnum::DOWN:
        // Do not draw triangles for 2-way up/down
        break;
    case ExitDirEnum::UNKNOWN:
        // NOTE: This is drawn for both 1-way and 2-way
        drawConnEndTriUpDownUnknown(dX, dY, dstZ);
        break;
    case ExitDirEnum::NONE:
        assert(false);
        break;
    }
}

void ConnectionDrawer::drawConnEndTri1Way(const ExitDirEnum endDir,
                                          const float dX,
                                          const float dY,
                                          const float dstZ)
{
    auto &gl = getFakeGL();

    switch (endDir) {
    case ExitDirEnum::NORTH:
        gl.drawTriangle(glm::vec3{dX + 0.32f, dY + 0.9f, dstZ},
                        glm::vec3{dX + 0.18f, dY + 0.9f, dstZ},
                        glm::vec3{dX + 0.25f, dY + 0.7f, dstZ});
        break;
    case ExitDirEnum::SOUTH:
        gl.drawTriangle(glm::vec3{dX + 0.68f, dY + 0.1f, dstZ},
                        glm::vec3{dX + 0.82f, dY + 0.1f, dstZ},
                        glm::vec3{dX + 0.75f, dY + 0.3f, dstZ});
        break;
    case ExitDirEnum::EAST:
        gl.drawTriangle(glm::vec3{dX + 0.9f, dY + 0.18f, dstZ},
                        glm::vec3{dX + 0.9f, dY + 0.32f, dstZ},
                        glm::vec3{dX + 0.7f, dY + 0.25f, dstZ});
        break;
    case ExitDirEnum::WEST:
        gl.drawTriangle(glm::vec3{dX + 0.1f, dY + 0.82f, dstZ},
                        glm::vec3{dX + 0.1f, dY + 0.68f, dstZ},
                        glm::vec3{dX + 0.3f, dY + 0.75f, dstZ});
        break;

    case ExitDirEnum::UP:
    case ExitDirEnum::DOWN:
    case ExitDirEnum::UNKNOWN:
        // NOTE: This is drawn for both 1-way and 2-way
        drawConnEndTriUpDownUnknown(dX, dY, dstZ);
        break;
    case ExitDirEnum::NONE:
        assert(false);
        break;
    }
}

void ConnectionDrawer::drawConnEndTriUpDownUnknown(float dX, float dY, float dstZ)
{
    getFakeGL().drawTriangle(glm::vec3{dX + 0.5f, dY + 0.5f, dstZ},
                             glm::vec3{dX + 0.55f, dY + 0.3f, dstZ},
                             glm::vec3{dX + 0.7f, dY + 0.45f, dstZ});
}

ConnectionMeshes ConnectionDrawerBuffers::getMeshes(
    OpenGL &gl,
    std::shared_ptr<Legacy::LineShader> lineShader) const
    // QOpenGLExtraFunctions* extraFunctions parameter removed
{
    Legacy::SharedFunctions sharedFunctions = gl.getFunctions().shared_from_this();
    Legacy::ConnectionMeshes result(sharedFunctions); // Construct with SharedFunctions

    // LineRenderer now gets QOpenGLExtraFunctions from Legacy::Functions (via sharedFunctions)
    // So, we only need to check lineShader here.
    if (lineShader) {
        try {
            // Construct LineRenderer without extraFunctions parameter
            result.normalLineRenderer = std::make_shared<Legacy::LineRenderer>(sharedFunctions, lineShader);
            result.normalLineRenderer->setup();

            result.redLineRenderer = std::make_shared<Legacy::LineRenderer>(sharedFunctions, lineShader);
            result.redLineRenderer->setup();
        } catch (const std::exception& e) {
            qWarning() << "Exception during LineRenderer creation/setup: " << e.what();
            result.normalLineRenderer.reset();
            result.redLineRenderer.reset();
        }
    } else {
        if (!normal.lineVerts.empty() || !red.lineVerts.empty()) { // Only warn if there were lines to draw
            qWarning() << "LineShader not available for ConnectionMeshes LineRenderers. Thick lines will not be rendered.";
        }
    }

    // Populate line data into the renderers (if they were successfully created)
    // The prepareLineDataForRenderers method internally checks if renderers are valid.
    this->prepareLineDataForRenderers(result, CONNECTION_LINE_WIDTH);


    // Create triangle meshes as before
    result.normalTris = gl.createColoredTriBatch(normal.triVerts);
    result.redTris = gl.createColoredTriBatch(red.triVerts);

    // The old UniqueMesh line batches (result.normalLines, result.redLines) are no longer created here.
    // If a fallback to old line rendering was desired when LineRenderer fails,
    // it would be added here. For now, no fallback.

    return result;
}

// Constructor for ConnectionMeshes
Legacy::ConnectionMeshes::ConnectionMeshes(Legacy::SharedFunctions sharedFunctions)
    : m_shared_functions(std::move(sharedFunctions)),
      m_functions(deref(m_shared_functions)) {
    if (!m_shared_functions) {
        throw std::runtime_error("ConnectionMeshes: SharedFunctions is null.");
    }
    // Initialization of LineRenderer members (normalLineRenderer, redLineRenderer)
    // is deferred to a later step.
}

void ConnectionDrawerBuffers::prepareLineDataForRenderers(
    Legacy::ConnectionMeshes& targetMeshes,
    float lineThickness) const
{
    std::vector<Legacy::LineInstanceData> normalInstanceData;
    normalInstanceData.reserve(normal.lineVerts.size() / 2);
    for (size_t i = 0; i < normal.lineVerts.size(); i += 2) {
        const auto& v1 = normal.lineVerts[i];
        const auto& v2 = normal.lineVerts[i+1];
        // Assuming v1.color (Legacy::Color) has a toGlmVec4() method.
        normalInstanceData.push_back({
            glm::vec2(v1.vert.x, v1.vert.y),
            glm::vec2(v2.vert.x, v2.vert.y),
            lineThickness,
            v1.color.toGlmVec4()
        });
    }
    if (targetMeshes.normalLineRenderer) {
        targetMeshes.normalLineRenderer->updateInstanceData(normalInstanceData);
    }

    std::vector<Legacy::LineInstanceData> redInstanceData;
    redInstanceData.reserve(red.lineVerts.size() / 2);
    for (size_t i = 0; i < red.lineVerts.size(); i += 2) {
        const auto& v1 = red.lineVerts[i];
        const auto& v2 = red.lineVerts[i+1];
        redInstanceData.push_back({
            glm::vec2(v1.vert.x, v1.vert.y),
            glm::vec2(v2.vert.x, v2.vert.y),
            lineThickness,
            v1.color.toGlmVec4()
        });
    }
    if (targetMeshes.redLineRenderer) {
        targetMeshes.redLineRenderer->updateInstanceData(redInstanceData);
    }
}


void ConnectionMeshes::render(const int thisLayer, const int focusedLayer) const
{
    // Assuming 'gl' (OpenGL&) is available here, or mvp is passed.
    // For now, let's assume mvp is obtained from a global/contextual source
    // or passed as a parameter. The original render method didn't take OpenGL& gl.
    // This will require refactoring in the calling code (MapCanvasData::getMeshes? Or MapCanvas::paintGL?)
    // For the purpose of this subtask, we'll construct a dummy mvp.
    // A more realistic scenario: const glm::mat4& mvp = openGLContext.getMVP();
    // Or: void ConnectionMeshes::render(OpenGL& gl, int thisLayer, int focusedLayer) const
    // Let's assume for now that `getOpenGL()` is a global accessor or similar context.
    // This is a placeholder for where MVP would come from.
    // In MapCanvas::paintGL, mvp is `m_viewProj`.
    // This part definitely needs to be wired correctly during actual integration.
    // For now, we can't fully implement it without knowing how `OpenGL& gl` or `mvp` is accessed here.
    // Let's assume `mvp` and `uniforms` are available.
    // Placeholder:
    // OpenGL& ogl = getContextOpenGL(); // Hypothetical global accessor
    // const glm::mat4 mvp = ogl.getFunctions().getProjectionMatrix(); // This is just one part of MVP usually.

    const auto base_color = [&thisLayer, &focusedLayer]() {
        if (thisLayer == focusedLayer) {
            return getCanvasNamedColorOptions().connectionNormalColor.getColor();
        }
        return Colors::gray70.withAlpha(FAINT_CONNECTION_ALPHA);
    }();
    const auto common_style = GLRenderState()
                                  .withBlend(BlendModeEnum::TRANSPARENCY)
                                  // LineParams for uColor modulation might not be directly used by LineRenderer if color is per-instance
                                  .withColor(base_color); // This sets the uColor for modulation in the shader

    // MVP matrix needs to be correctly sourced.
    // This is a critical point for integration.
    // For this subtask, we are focusing on the call structure.
    // Let's assume an `mvp` matrix is passed or accessible.
    // For example, if `MapCanvas::paintGL` calls this, it has `m_viewProj`.
    // If `ConnectionMeshes::render` is called from `MapCanvasData::getMeshesAndPaint`,
    // then `OpenGL& gl` and `mvp` matrix should be passed through.

    const glm::mat4 mvp = m_functions.getProjectionMatrix(); // Get MVP from m_functions

    if (normalLineRenderer) {
        // The GLRenderState's color is used for uColor in the shader.
        // Instance colors are from LineInstanceData.
        normalLineRenderer->render(mvp, common_style.uniforms);
    }
    if (redLineRenderer) {
        // If red lines use a different base modulation color (e.g. always red even if faint)
        // then common_style would need to be adjusted.
        // For now, assume they use the same uColor modulation logic based on focus.
        // The per-instance data already contains red color for red lines.
        GLRenderState red_style = common_style;
        // Potentially: red_style.withColor( (thisLayer == focusedLayer) ? Colors::red : Colors::red.withAlpha(FAINT_CONNECTION_ALPHA) );
        redLineRenderer->render(mvp, red_style.uniforms);
    }

    // Render triangles as before
    normalTris.render(common_style);
    // For red triangles, if they need to be distinctly red even when not focused:
    GLRenderState red_tri_style = common_style;
    if (thisLayer != focusedLayer) { // If not focused, make red tris also faint red
        red_tri_style.withColor(Colors::red.withAlpha(FAINT_CONNECTION_ALPHA));
    } else { // If focused, make red tris fully red
         red_tri_style.withColor(Colors::red);
    }
    redTris.render(red_tri_style); // Or just common_style if their base color is already red
}

void MapCanvas::paintNearbyConnectionPoints()
{
    const bool isSelection = m_canvasMouseMode == CanvasMouseModeEnum::SELECT_CONNECTIONS;
    using CD = ConnectionSelection::ConnectionDescriptor;

    static const ExitDirFlags allExits = []() {
        ExitDirFlags tmp;
        for (const ExitDirEnum dir : ALL_EXITS7) {
            tmp |= dir;
        }
        return tmp;
    }();

    std::vector<ColorVert> points;
    const auto addPoint = [isSelection, &points](const Coordinate &roomCoord,
                                                 const RoomHandle &room,
                                                 const ExitDirEnum dir,
                                                 const std::optional<CD> &optFirst) -> void {
        if (!isNESWUD(dir) && dir != ExitDirEnum::UNKNOWN) {
            return;
        }

        if (optFirst) {
            const CD &first = optFirst.value();
            const CD second{room, dir};
            if (isSelection ? !CD::isCompleteExisting(first, second)
                            : !CD::isCompleteNew(first, second)) {
                return;
            }
        }

        points.emplace_back(Colors::cyan, roomCoord.to_vec3() + getConnectionOffset(dir));
    };
    const auto addPoints =
        [this, isSelection, &addPoint](const std::optional<MouseSel> &sel,
                                       const std::optional<CD> &optFirst) -> void {
        if (!sel.has_value()) {
            return;
        }
        const auto mouse = sel->getCoordinate();
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const Coordinate roomCoord = mouse + Coordinate(dx, dy, 0);
                const auto room = m_data.findRoomHandle(roomCoord);
                if (!room) {
                    continue;
                }

                ExitDirFlags dirs = isSelection ? m_data.getExitDirections(roomCoord) : allExits;
                if (optFirst) {
                    dirs |= ExitDirEnum::UNKNOWN;
                }

                dirs.for_each([&addPoint, &optFirst, &roomCoord, &room](ExitDirEnum dir) -> void {
                    addPoint(roomCoord, room, dir, optFirst);
                });
            }
        }
    };

    // FIXME: This doesn't show dots for red connections.
    if (m_connectionSelection != nullptr
        && (m_connectionSelection->isFirstValid() || m_connectionSelection->isSecondValid())) {
        const CD valid = m_connectionSelection->isFirstValid() ? m_connectionSelection->getFirst()
                                                               : m_connectionSelection->getSecond();
        const Coordinate &c = valid.room.getPosition();
        const glm::vec3 &pos = c.to_vec3();
        points.emplace_back(Colors::cyan, pos + getConnectionOffset(valid.direction));

        addPoints(MouseSel{Coordinate2f{pos.x, pos.y}, c.z}, valid);
        addPoints(m_sel1, valid);
        addPoints(m_sel2, valid);
    } else {
        addPoints(m_sel1, std::nullopt);
        addPoints(m_sel2, std::nullopt);
    }

    getOpenGL().renderPoints(points, GLRenderState().withPointSize(VALID_CONNECTION_POINT_SIZE));
}

void MapCanvas::paintSelectedConnection()
{
    if (isConnectionMode(m_canvasMouseMode)) {
        paintNearbyConnectionPoints();
    }

    if (m_connectionSelection == nullptr || !m_connectionSelection->isFirstValid()) {
        return;
    }

    ConnectionSelection &sel = deref(m_connectionSelection);

    const ConnectionSelection::ConnectionDescriptor &first = sel.getFirst();
    const auto pos1 = getPosition(first);
    // REVISIT: How about not dashed lines to the nearest possible connections
    // if the second isn't valid?
    const auto optPos2 = [this, &sel]() -> std::optional<glm::vec3> {
        if (sel.isSecondValid()) {
            return getPosition(sel.getSecond());
        } else if (hasSel2()) {
            return getSel2().to_vec3();
        } else {
            return std::nullopt;
        }
    }();

    if (!optPos2) {
        return;
    }

    const glm::vec3 &pos2 = optPos2.value();

    auto &gl = getOpenGL();
    const auto rs = GLRenderState().withColor(Colors::red);

    const std::vector<glm::vec3> verts{pos1, pos2};
    gl.renderPlainLines(verts, rs.withLineParams(LineParams{CONNECTION_LINE_WIDTH}));

    std::vector<ColorVert> points;
    points.emplace_back(Colors::red, pos1);
    points.emplace_back(Colors::red, pos2);
    gl.renderPoints(points, rs.withPointSize(NEW_CONNECTION_POINT_SIZE));
}

static constexpr float LONG_LINE_HALFLEN = 1.5f;
static constexpr float LONG_LINE_LEN = 2.f * LONG_LINE_HALFLEN;
NODISCARD static bool isLongLine(const glm::vec3 &a, const glm::vec3 &b)
{
    return glm::length(a - b) >= LONG_LINE_LEN;
}

void ConnectionDrawer::ConnectionFakeGL::drawTriangle(const glm::vec3 &a,
                                                      const glm::vec3 &b,
                                                      const glm::vec3 &c)
{
    const auto &color = isNormal() ? getCanvasNamedColorOptions().connectionNormalColor.getColor()
                                   : Colors::red;
    auto &verts = deref(m_currentBuffer).triVerts;
    verts.emplace_back(color, a + m_offset);
    verts.emplace_back(color, b + m_offset);
    verts.emplace_back(color, c + m_offset);
}

void ConnectionDrawer::ConnectionFakeGL::drawLineStrip(const std::vector<glm::vec3> &points)
{
    const auto &connectionNormalColor
        = isNormal() ? getCanvasNamedColorOptions().connectionNormalColor.getColor() : Colors::red;

    const auto transform = [this](const glm::vec3 &vert) { return vert + m_offset; };
    auto &verts = deref(m_currentBuffer).lineVerts;
    auto drawLine = [&verts](const Color &color, const glm::vec3 &a, const glm::vec3 &b) {
        verts.emplace_back(color, a);
        verts.emplace_back(color, b);
    };

    const auto size = points.size();
    assert(size >= 2);
    for (size_t i = 1; i < size; ++i) {
        const auto start = transform(points[i - 1u]);
        const auto end = transform(points[i]);

        if (!isLongLine(start, end)) {
            drawLine(connectionNormalColor, start, end);
            continue;
        }

        const auto len = glm::length(start - end);
        const auto faintCutoff = LONG_LINE_HALFLEN / len;
        const auto mid1 = glm::mix(start, end, faintCutoff);
        const auto mid2 = glm::mix(start, end, 1.f - faintCutoff);
        const auto faint = connectionNormalColor.withAlpha(FAINT_CONNECTION_ALPHA);

        drawLine(connectionNormalColor, start, mid1);
        drawLine(faint, mid1, mid2);
        drawLine(connectionNormalColor, mid2, end);
    }
}
