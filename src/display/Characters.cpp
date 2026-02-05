// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "Characters.h"

#include "../configuration/configuration.h"
#include "../group/CGroupChar.h"
#include "../group/mmapper2group.h"
#include "../map/roomid.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/roomselection.h"
#include "../opengl/LineRendering.h"
#include "../opengl/OpenGL.h"
#include "../opengl/OpenGLTypes.h"
#include "MapCanvasData.h"
#include "Textures.h"
#include "mapcanvas.h"
#include "prespammedpath.h"
#include "MapCanvasViewModel.h"

#include <array>
#include <cmath>
#include <optional>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include <QtCore>

static constexpr float CHAR_ARROW_LINE_WIDTH = 2.f;
static constexpr float PATH_LINE_WIDTH = 0.1f;
static constexpr float PATH_POINT_SIZE = 8.f;
static constexpr float CHAR_SIZE = 0.4f;
static constexpr float CHAR_BEACON_SIZE = 0.2f;

DistantObjectTransform DistantObjectTransform::construct(const glm::vec3 &pos,
                                                         const MapScreen &mapScreen,
                                                         const float marginPixels)
{
    assert(marginPixels > 0.f);
    const glm::vec3 viewCenter = mapScreen.getCenter();
    const auto delta = pos - viewCenter;
    const float radians = std::atan2(delta.y, delta.x);
    const auto hint = mapScreen.getProxyLocation(pos, marginPixels);
    const float degrees = glm::degrees(radians);
    return DistantObjectTransform{hint, degrees};
}

bool CharacterBatch::isVisible(const Coordinate &c, float margin) const
{
    return m_mapScreen.isRoomVisible(c, margin);
}

void CharacterBatch::drawCharacter(const Coordinate &c, const Color color, bool fill)
{
    const Configuration::CanvasSettings &settings = getConfig().canvas;

    const glm::vec3 roomCenter = c.to_vec3() + glm::vec3{0.5f, 0.5f, 0.f};
    const int layerDifference = c.z - m_currentLayer;

    auto &gl = getOpenGL();
    gl.setColor(color);

    // REVISIT: The margin probably needs to be modified for high-dpi.
    const float marginPixels = MapScreen::DEFAULT_MARGIN_PIXELS;
    const bool visible = isVisible(c, marginPixels / 2.f);
    const bool isFar = m_scale <= settings.charBeaconScaleCutoff;
    const bool wantBeacons = settings.drawCharBeacons && isFar;
    if (!visible) {
        static const bool useScreenSpacePlayerArrow = std::invoke([]() -> bool {
            auto opt = utils::getEnvBool("MMAPPER_SCREEN_SPACE_ARROW");
            return opt ? opt.value() : true;
        });
        const auto dot = DistantObjectTransform::construct(roomCenter, m_mapScreen, marginPixels);
        // Player is distant
        if (useScreenSpacePlayerArrow) {
            gl.addScreenSpaceArrow(dot.offset, dot.rotationDegrees, color, fill);
        } else {
            gl.glPushMatrix();
            gl.glTranslatef(dot.offset);
            // NOTE: 180 degrees of additional rotation flips the arrow to point right instead of left.
            gl.glRotateZ(dot.rotationDegrees + 180.f);
            // NOTE: arrow is centered, so it doesn't need additional translation.
            gl.drawArrow(fill, wantBeacons);
            gl.glPopMatrix();
        }
    }

    const bool differentLayer = layerDifference != 0;
    if (differentLayer) {
        const glm::vec3 centerOnCurrentLayer{glm::vec2(roomCenter),
                                             static_cast<float>(m_currentLayer)};
        // Draw any arrow on the current layer pointing in either up or down
        // (this may not make sense graphically in an angled 3D view).
        gl.glPushMatrix();
        gl.glTranslatef(centerOnCurrentLayer);
        // Arrow points up or down.
        // REVISIT: billboard this in 3D?
        gl.glRotateZ((layerDifference > 0) ? 90.f : 270.f);
        gl.drawArrow(fill, false);
        gl.glPopMatrix();
    }

    const bool beacon = visible && !differentLayer && wantBeacons;
    gl.drawBox(c, fill, beacon, isFar);
}

void CharacterBatch::CharFakeGL::drawPathSegment(const glm::vec3 &p1,
                                                 const glm::vec3 &p2,
                                                 const Color color)
{
    mmgl::generateLineQuadsSafe(m_pathLineQuads, p1, p2, PATH_LINE_WIDTH, color);
}

void CharacterBatch::drawPreSpammedPath(const Coordinate &coordinate,
                                        const std::vector<Coordinate> &path,
                                        const Color color)
{
    auto &gl = getOpenGL();

    glm::vec3 p1 = coordinate.to_vec3() + glm::vec3{0.5f, 0.5f, 0.5f};
    gl.drawPathPoint(color, p1);

    for (const auto &c : path) {
        const glm::vec3 p2 = c.to_vec3() + glm::vec3{0.5f, 0.5f, 0.5f};
        gl.drawPathPoint(color, p2);
        gl.drawPathSegment(p1, p2, color);
        p1 = p2;
    }
}

void CharacterBatch::CharFakeGL::reallyDrawCharacters(OpenGL &gl, const MapCanvasTextures &textures)
{
    if (m_charTris.empty() && m_charBeaconQuads.empty() && m_charLines.empty()
        && m_charRoomQuads.empty() && m_screenSpaceArrows.empty()) {
        return;
    }

    const GLRenderState state = gl.getDefaultRenderState()
        .withDepthFunction(std::nullopt)
        .withBlend(BlendModeEnum::TRANSPARENCY);

    if (!m_charRoomQuads.empty()) {
        gl.renderColoredTexturedQuads(m_charRoomQuads, state.withTexture0(textures.char_room_sel->getId()));
        m_charRoomQuads.clear();
    }

    if (!m_charTris.empty()) {
        gl.renderColoredTris(m_charTris, state);
        m_charTris.clear();
    }

    if (!m_charBeaconQuads.empty()) {
        gl.renderColoredQuads(m_charBeaconQuads, state);
        m_charBeaconQuads.clear();
    }

    if (!m_charLines.empty()) {
        gl.renderColoredLines(m_charLines, state);
        m_charLines.clear();
    }

    if (!m_screenSpaceArrows.empty()) {
        gl.renderFont3d(textures.char_arrows, m_screenSpaceArrows);
        m_screenSpaceArrows.clear();
    }
}

void CharacterBatch::CharFakeGL::reallyDrawPaths(OpenGL &gl)
{
    if (m_pathPoints.empty() && m_pathLineQuads.empty()) {
        return;
    }

    const GLRenderState state = gl.getDefaultRenderState()
        .withDepthFunction(std::nullopt)
        .withBlend(BlendModeEnum::TRANSPARENCY);

    if (!m_pathLineQuads.empty()) {
        gl.renderColoredQuads(m_pathLineQuads, state);
        m_pathLineQuads.clear();
    }

    if (!m_pathPoints.empty()) {
        gl.renderPoints(m_pathPoints, state.withPointSize(PATH_POINT_SIZE));
        m_pathPoints.clear();
    }
}

void CharacterBatch::CharFakeGL::drawArrow(bool fill, bool beacon)
{
    const float alpha = beacon ? BEACON_ALPHA : (fill ? FILL_ALPHA : LINE_ALPHA);
    const Color color = m_color.withAlpha(alpha);

    const auto &m = m_stack.top().modelView;

    // Triangle pointing left, centered at (0,0,0)
    const glm::vec3 p1 = m * glm::vec4(-0.4f, 0.0f, 0.0f, 1.0f);
    const glm::vec3 p2 = m * glm::vec4(0.4f, 0.3f, 0.0f, 1.0f);
    const glm::vec3 p3 = m * glm::vec4(0.4f, -0.3f, 0.0f, 1.0f);

    if (fill || beacon) {
        m_charTris.emplace_back(color, p1);
        m_charTris.emplace_back(color, p2);
        m_charTris.emplace_back(color, p3);
    } else {
        m_charLines.emplace_back(color, p1);
        m_charLines.emplace_back(color, p2);

        m_charLines.emplace_back(color, p2);
        m_charLines.emplace_back(color, p3);

        m_charLines.emplace_back(color, p3);
        m_charLines.emplace_back(color, p1);
    }
}

void CharacterBatch::CharFakeGL::drawBox(const Coordinate &coord,
                                         bool fill,
                                         bool beacon,
                                         bool isFar)
{
    const float alpha = beacon ? BEACON_ALPHA : (fill ? FILL_ALPHA : LINE_ALPHA);
    const Color color = m_color.withAlpha(alpha);

    const float boxSize = isFar ? CHAR_BEACON_SIZE : CHAR_SIZE;

    const float x = static_cast<float>(coord.x) + 0.5f;
    const float y = static_cast<float>(coord.y) + 0.5f;
    const float z = static_cast<float>(coord.z) + 0.5f;

    const float x1 = x - boxSize / 2.f;
    const float x2 = x + boxSize / 2.f;
    const float y1 = y - boxSize / 2.f;
    const float y2 = y + boxSize / 2.f;

    const glm::vec2 p1{x1, y1};
    const glm::vec2 p2{x2, y1};
    const glm::vec2 p3{x2, y2};
    const glm::vec2 p4{x1, y2};

    auto options = QuadOptsEnum::NONE;
    if (fill)
        options = options | QuadOptsEnum::FILL;
    if (beacon)
        options = options | QuadOptsEnum::BEACON;
    if (!fill && !beacon)
        options = options | QuadOptsEnum::OUTLINE;

    drawQuadCommon(p1, p2, p3, p4, options);

    const int count = m_coordCounts[coord];
    if (count > 1) {
        const float margin = 0.05f;
        const glm::vec2 inner_p1{x1 + margin, y1 + margin};
        const glm::vec2 inner_p2{x2 - margin, y1 + margin};
        const glm::vec2 inner_p3{x2 - margin, y2 - margin};
        const glm::vec2 inner_p4{x1 + margin, y2 - margin};
        drawQuadCommon(inner_p1, inner_p2, inner_p3, inner_p4, options);
    }

    if (!isFar) {
        const float z_offset = 0.01f;
        m_charRoomQuads.emplace_back(color, glm::vec3{0, 0, 0}, glm::vec3{p1, z + z_offset});
        m_charRoomQuads.emplace_back(color, glm::vec3{1, 0, 0}, glm::vec3{p2, z + z_offset});
        m_charRoomQuads.emplace_back(color, glm::vec3{1, 1, 0}, glm::vec3{p3, z + z_offset});
        m_charRoomQuads.emplace_back(color, glm::vec3{0, 1, 0}, glm::vec3{p4, z + z_offset});
    }
}

void CharacterBatch::CharFakeGL::drawQuadCommon(const glm::vec2 &a,
                                                const glm::vec2 &b,
                                                const glm::vec2 &c,
                                                const glm::vec2 &d,
                                                QuadOptsEnum options)
{
    const bool fill = (options & QuadOptsEnum::FILL) != QuadOptsEnum::NONE;
    const bool beacon = (options & QuadOptsEnum::BEACON) != QuadOptsEnum::NONE;
    const bool outline = (options & QuadOptsEnum::OUTLINE) != QuadOptsEnum::NONE;

    const float alpha = beacon ? BEACON_ALPHA : (fill ? FILL_ALPHA : LINE_ALPHA);
    const Color color = m_color.withAlpha(alpha);

    const float z = 0.5f; // REVISIT

    if (fill || beacon) {
        m_charBeaconQuads.emplace_back(color, glm::vec3{a, z});
        m_charBeaconQuads.emplace_back(color, glm::vec3{b, z});
        m_charBeaconQuads.emplace_back(color, glm::vec3{c, z});
        m_charBeaconQuads.emplace_back(color, glm::vec3{d, z});
    }

    if (outline) {
        m_charLines.emplace_back(color, glm::vec3{a, z});
        m_charLines.emplace_back(color, glm::vec3{b, z});

        m_charLines.emplace_back(color, glm::vec3{b, z});
        m_charLines.emplace_back(color, glm::vec3{c, z});

        m_charLines.emplace_back(color, glm::vec3{c, z});
        m_charLines.emplace_back(color, glm::vec3{d, z});

        m_charLines.emplace_back(color, glm::vec3{d, z});
        m_charLines.emplace_back(color, glm::vec3{a, z});
    }
}

void CharacterBatch::CharFakeGL::addScreenSpaceArrow(const glm::vec3 &pos,
                                                    float degrees,
                                                    const Color color,
                                                    bool fill)
{
    static constexpr const std::array<glm::vec2, 4> texCoords = {
        glm::vec2{0, 0}, glm::vec2{1, 0}, glm::vec2{1, 1}, glm::vec2{0, 1}};

    const float scale = 16.f; // REVISIT
    const float radians = glm::radians(degrees);
    const glm::vec3 z{0, 0, 1};
    const glm::mat4 rotation = glm::rotate(glm::mat4(1), radians, z);
    for (size_t i = 0; i < 4; ++i) {
        const glm::vec2 &tc = texCoords[i];
        const auto tmp = rotation * glm::vec4(tc * 2.f - 1.f, 0, 1);
        const glm::vec2 screenSpaceOffset = scale * glm::vec2(tmp) / tmp.w;
        // solid   |filled
        // --------+--------
        // outline | n/a
        const glm::vec2 tcOffset = tc * 0.5f
                                   + (fill ? glm::vec2(0.5f, 0.5f) : glm::vec2(0.f, 0.0f));
        m_screenSpaceArrows.emplace_back(pos, color, tcOffset, screenSpaceOffset);
    }
}

void MapCanvas::paintCharacters()
{
    if (m_data.isEmpty()) {
        return;
    }

    CharacterBatch characterBatch{m_mapScreen, m_viewModel->layer(), m_viewModel->getTotalScaleFactor()};

    // IIFE to abuse return to avoid duplicate else branches
    std::invoke([this, &characterBatch]() -> void {
        if (const std::optional<RoomId> opt_pos = m_data.getCurrentRoomId()) {
            const auto &id = opt_pos.value();
            if (const auto room = m_data.findRoomHandle(id)) {
                const auto &pos = room.getPosition();
                // draw the characters before the current position
                characterBatch.incrementCount(pos);
                drawGroupCharacters(characterBatch);
                characterBatch.resetCount(pos);

                // paint char current position
                const Color color{getConfig().groupManager.color};
                characterBatch.drawCharacter(pos, color);

                // paint prespam
                const auto prespam = m_data.getPath(id, m_viewModel->m_prespammedPath.getQueue());
                characterBatch.drawPreSpammedPath(pos, prespam, color);
                return;
            } else {
                // this can happen if the "current room" is deleted
                // and we failed to clear it elsewhere.
                m_data.clearSelectedRoom();
            }
        }
        drawGroupCharacters(characterBatch);
    });

    characterBatch.reallyDraw(getOpenGL(), m_textures);
}

void MapCanvas::drawGroupCharacters(CharacterBatch &batch)
{
    if (m_data.isEmpty()) {
        return;
    }

    const Map &map = m_data.getCurrentMap();
    for (const auto &pCharacter : m_groupManager.selectAll()) {
        // Omit player so that they know group members are below them
        if (pCharacter->isYou())
            continue;

        const CGroupChar &character = deref(pCharacter);

        const auto &r = std::invoke([&character, &map]() -> RoomHandle {
            const auto &id = character.getServerId();
            if (id != INVALID_SERVER_ROOMID) {
                return map.findRoomHandle(id);
            }
            return RoomHandle();
        });

        if (r) {
            const auto &pos = r.getPosition();
            const Color color{character.getColor()};
            batch.drawCharacter(pos, color);
        }
    }
}
