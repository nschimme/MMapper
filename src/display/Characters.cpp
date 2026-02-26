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

    const int layerDifference = c.z - m_currentLayer;

    auto &gl = getOpenGL();
    gl.setColor(color);

    const bool isFar = m_scale <= settings.charBeaconScaleCutoff;
    const bool wantBeacons = settings.drawCharBeacons && isFar;

    const bool differentLayer = layerDifference != 0;
    if (differentLayer) {
        const glm::vec3 roomCenter = c.to_vec3() + glm::vec3{0.5f, 0.5f, 0.f};
        const glm::vec3 centerOnCurrentLayer{static_cast<glm::vec2>(roomCenter),
                                             static_cast<float>(m_currentLayer)};
        gl.glPushMatrix();
        gl.glTranslatef(centerOnCurrentLayer);
        gl.glRotateZ((layerDifference > 0) ? 90.f : 270.f);
        gl.drawArrow(fill, false);
        gl.glPopMatrix();
    }

    const bool beacon = !differentLayer && wantBeacons;
    gl.drawBox(c, fill, beacon, isFar);

    gl.addPlayer(c, color, fill);
}

void CharacterBatch::CharFakeGL::drawPathSegment(const glm::vec3 &p1,
                                                 const glm::vec3 &p2,
                                                 const Color color)
{
    mmgl::generateLineQuadsSafe(m_pathLineQuads, p1, p2, PATH_LINE_WIDTH, color);
}

void CharacterBatch::drawPreSpammedPath(const Coordinate &c1,
                                        const std::vector<Coordinate> &path,
                                        const Color color)
{
    if (path.empty()) {
        return;
    }

    using TranslatedVerts = std::vector<glm::vec3>;
    const auto verts = std::invoke([&c1, &path]() -> TranslatedVerts {
        TranslatedVerts translated;
        translated.reserve(path.size() + 1);

        const auto add = [&translated](const Coordinate &c) -> void {
            static const glm::vec3 PATH_OFFSET{0.5f, 0.5f, 0.f};
            translated.push_back(c.to_vec3() + PATH_OFFSET);
        };

        add(c1);
        for (const Coordinate &c2 : path) {
            add(c2);
        }
        return translated;
    });

    auto &gl = getOpenGL();

    for (size_t i = 0; i < verts.size() - 1; ++i) {
        const glm::vec3 p1 = verts[i];
        const glm::vec3 p2 = verts[i + 1];
        gl.drawPathSegment(p1, p2, color);
    }
    gl.drawPathPoint(color, verts.back());
}

void CharacterBatch::CharFakeGL::drawQuadCommon(const glm::vec2 &in_a,
                                                const glm::vec2 &in_b,
                                                const glm::vec2 &in_c,
                                                const glm::vec2 &in_d,
                                                const QuadOptsEnum options)
{
    const auto &m = m_stack.top().modelView;
    const auto transform = [&m](const glm::vec2 &vin) -> glm::vec3 {
        const auto vtmp = m * glm::vec4(vin, 0.f, 1.f);
        return glm::vec3{vtmp / vtmp.w};
    };

    const auto a = transform(in_a);
    const auto b = transform(in_b);
    const auto c = transform(in_c);
    const auto d = transform(in_d);

    if (::utils::isSet(options, QuadOptsEnum::FILL)) {
        const auto color = m_color.withAlpha(FILL_ALPHA);
        auto emitVert = [this, &color](const auto &x) -> void { m_charTris.emplace_back(color, x); };
        auto emitTri = [&emitVert](const auto &v0, const auto &v1, const auto &v2) -> void {
            emitVert(v0);
            emitVert(v1);
            emitVert(v2);
        };
        emitTri(a, b, c);
        emitTri(a, c, d);
    }

    if (::utils::isSet(options, QuadOptsEnum::BEACON)) {
        const auto color = m_color.withAlpha(BEACON_ALPHA);
        const glm::vec3 heightOffset{0.f, 0.f, 50.f};
        const auto e = a + heightOffset;
        const auto f = b + heightOffset;
        const auto g = c + heightOffset;
        const auto h = d + heightOffset;
        auto emitVert = [this, &color](const auto &x) -> void {
            m_charBeaconQuads.emplace_back(color, x);
        };
        auto emitQuad =
            [&emitVert](const auto &v0, const auto &v1, const auto &v2, const auto &v3) -> void {
            emitVert(v0);
            emitVert(v1);
            emitVert(v2);
            emitVert(v3);
        };
        emitQuad(a, e, f, b);
        emitQuad(b, f, g, c);
        emitQuad(c, g, h, d);
        emitQuad(d, h, e, a);
    }

    if (::utils::isSet(options, QuadOptsEnum::OUTLINE)) {
        const auto color = m_color.withAlpha(LINE_ALPHA);
        auto emitVert = [this, &color](const auto &x) -> void {
            m_charLines.emplace_back(color, x);
        };
        auto emitLine = [&emitVert](const auto &v0, const auto &v1) -> void {
            emitVert(v0);
            emitVert(v1);
        };
        emitLine(a, b);
        emitLine(b, c);
        emitLine(c, d);
        emitLine(d, a);
    }
}

void CharacterBatch::CharFakeGL::drawBox(const Coordinate &coord,
                                         bool fill,
                                         bool beacon,
                                         const bool isFar)
{
    const bool dontFillRotatedQuads = true;
    const bool shrinkRotatedQuads = false;

    const int numAlreadyInRoom = m_coordCounts[coord]++;

    glPushMatrix();
    glTranslatef(coord.to_vec3());

    if (numAlreadyInRoom != 0) {
        static constexpr const float MAGIC_ANGLE = 45.f / float(M_PI);
        const float degrees = static_cast<float>(numAlreadyInRoom) * MAGIC_ANGLE;
        const glm::vec3 quadCenter{0.5f, 0.5f, 0.f};
        glTranslatef(quadCenter);
        if ((shrinkRotatedQuads)) {
            glScalef(0.7f, 0.7f, 1.f);
        }
        glRotateZ(degrees);
        glTranslatef(-quadCenter);
        if (dontFillRotatedQuads) {
            fill = false;
        }
        beacon = false;
    }

    const glm::vec2 a{0, 0};
    const glm::vec2 b{1, 0};
    const glm::vec2 c{1, 1};
    const glm::vec2 d{0, 1};

    if (isFar) {
        const auto options = QuadOptsEnum::OUTLINE
                             | (fill ? QuadOptsEnum::FILL : QuadOptsEnum::NONE)
                             | (beacon ? QuadOptsEnum::BEACON : QuadOptsEnum::NONE);
        drawQuadCommon(a, b, c, d, options);
    } else {
        const auto &color = m_color;
        const auto &m = m_stack.top().modelView;
        const auto addTransformed = [this, &color, &m](const glm::vec2 &in_vert) -> void {
            const auto tmp = m * glm::vec4(in_vert, 0.f, 1.f);
            m_charRoomQuads.emplace_back(color, glm::vec3{in_vert, 0}, glm::vec3{tmp / tmp.w});
        };
        addTransformed(a);
        addTransformed(b);
        addTransformed(c);
        addTransformed(d);

        if (beacon) {
            drawQuadCommon(a, b, c, d, QuadOptsEnum::BEACON);
        }
    }
    glPopMatrix();
}

void CharacterBatch::CharFakeGL::drawArrow(const bool fill, const bool beacon)
{
    const glm::vec2 a{-0.5f, 0.f};
    const glm::vec2 b{0.75f, -0.5f};
    const glm::vec2 c{0.25f, 0.f};
    const glm::vec2 d{0.75f, 0.5f};

    const auto options = QuadOptsEnum::OUTLINE | (fill ? QuadOptsEnum::FILL : QuadOptsEnum::NONE)
                         | (beacon ? QuadOptsEnum::BEACON : QuadOptsEnum::NONE);
    drawQuadCommon(a, b, c, d, options);
}

void CharacterBatch::CharFakeGL::bake(OpenGL &gl, const MapCanvasTextures &textures)
{
    m_meshes.tris = gl.createColoredTriBatch(m_charTris);
    m_meshes.beaconQuads = gl.createColoredQuadBatch(m_charBeaconQuads);
    m_meshes.lines = gl.createColoredLineBatch(m_charLines);
    m_meshes.pathPoints = gl.createPointBatch(m_pathPoints);
    m_meshes.pathLineQuads = gl.createColoredQuadBatch(m_pathLineQuads);

    m_meshes.roomQuads = gl.createColoredTexturedQuadBatch(m_charRoomQuads,
                                                          textures.char_room_sel->getId());

    m_meshes.isValid = true;
}

void CharacterBatch::CharFakeGL::reallyDrawMeshes(OpenGL &, const MapCanvasTextures &)
{
    const auto blended_noDepth
        = GLRenderState().withDepthFunction(std::nullopt).withBlend(BlendModeEnum::TRANSPARENCY);

    if (m_meshes.isValid) {
        m_meshes.beaconQuads.render(blended_noDepth.withCulling(CullingEnum::FRONT));
        m_meshes.roomQuads.render(blended_noDepth);
        m_meshes.tris.render(blended_noDepth);
        m_meshes.lines.render(blended_noDepth.withLineParams(LineParams{CHAR_ARROW_LINE_WIDTH}));
        m_meshes.pathPoints.render(blended_noDepth.withPointSize(PATH_POINT_SIZE));
        m_meshes.pathLineQuads.render(blended_noDepth);
    }
}

void CharacterBatch::CharFakeGL::addName(const Coordinate &c,
                                         const std::string &name,
                                         const Color color)
{
    const glm::vec3 roomCenter = c.to_vec3() + glm::vec3{0.5f, 0.5f, 0.f};
    m_names.emplace_back(BatchedName{roomCenter, name, textColor(color), color.withAlpha(0.6f), 0});
}

void CharacterBatch::CharFakeGL::reallyDrawNames(OpenGL &gl,
                                                 GLFont &font,
                                                 const MapScreen &mapScreen)
{
    if (m_names.empty()) {
        return;
    }

    const auto &viewport = mapScreen.getViewport();
    const float dpr = gl.getDevicePixelRatio();
    const float fontPhysicalHeight = static_cast<float>(font.getFontHeight());
    const float physicalScreenWidth = static_cast<float>(gl.getPhysicalViewport().size.x);
    const float marginPixels = MapScreen::DEFAULT_MARGIN_PIXELS;

    std::vector<GLText> physicalNames;
    physicalNames.reserve(m_names.size());

    std::map<Coordinate, int, CoordCompare> stackCounts;

    for (const auto &batchName : m_names) {
        const Coordinate c{static_cast<int>(std::floor(batchName.worldPos.x)),
                           static_cast<int>(std::floor(batchName.worldPos.y)),
                           static_cast<int>(std::round(batchName.worldPos.z))};
        const bool visible = mapScreen.isRoomVisible(c, marginPixels / 2.f);

        std::optional<glm::vec3> optScreen;
        float verticalOffset;

        if (visible) {
            optScreen = viewport.project(batchName.worldPos);
            const auto optScreenTop = viewport.project(batchName.worldPos + glm::vec3{0.f, 0.5f, 0.f});
            if (optScreen && optScreenTop) {
                verticalOffset = glm::distance(glm::vec2(*optScreen), glm::vec2(*optScreenTop)) + 2.f;
            } else {
                verticalOffset = 10.f;
            }
        } else {
            const glm::vec3 proxyWorld = mapScreen.getProxyLocation(batchName.worldPos, marginPixels);
            optScreen = viewport.project(proxyWorld);
            verticalOffset = 15.f;
        }

        if (!optScreen) {
            continue;
        }

        const float screenX = optScreen->x;
        const float screenY = static_cast<float>(viewport.height()) - optScreen->y;

        const int stackIdx = stackCounts[c]++;

        GLText physicalText{glm::vec3{screenX, screenY - verticalOffset, 0.f},
                            batchName.text,
                            batchName.color,
                            batchName.bgcolor,
                            FontFormatFlags{FontFormatFlagEnum::HALIGN_CENTER}};

        const float physicalWidth = static_cast<float>(font.measureWidth(physicalText.text));

        float px = physicalText.pos.x * dpr;
        const float py = physicalText.pos.y * dpr - static_cast<float>(stackIdx) * fontPhysicalHeight;

        const float halfWidth = physicalWidth / 2.0f;
        const float margin = 4.0f;

        if (px - halfWidth < margin) {
            px = halfWidth + margin;
        } else if (px + halfWidth > physicalScreenWidth - margin) {
            px = physicalScreenWidth - halfWidth - margin;
        }

        physicalText.pos.x = px;
        physicalText.pos.y = py;

        physicalNames.push_back(std::move(physicalText));
    }

    font.render2dTextImmediate(physicalNames);
}

void CharacterBatch::CharFakeGL::reallyDrawArrows(OpenGL &gl,
                                                  const MapCanvasTextures &textures,
                                                  const MapScreen &mapScreen)
{
    if (m_players.empty()) {
        return;
    }

    const float marginPixels = MapScreen::DEFAULT_MARGIN_PIXELS;
    const float dpr = gl.getDevicePixelRatio();

    std::vector<FontVert3d> physicalArrows;

    for (const auto &p : m_players) {
        if (mapScreen.isRoomVisible(p.pos, marginPixels / 2.f)) {
            continue;
        }

        const glm::vec3 roomCenter = p.pos.to_vec3() + glm::vec3{0.5f, 0.5f, 0.f};
        const auto dot = DistantObjectTransform::construct(roomCenter, mapScreen, marginPixels);

        std::array<glm::vec2, 4> texCoords{
            glm::vec2{0, 0},
            glm::vec2{1, 0},
            glm::vec2{1, 1},
            glm::vec2{0, 1},
        };

        const float scale = MapScreen::DEFAULT_MARGIN_PIXELS * dpr;
        const float radians = glm::radians(dot.rotationDegrees);
        const glm::vec3 z{0, 0, 1};
        const glm::mat4 rotation = glm::rotate(glm::mat4(1), radians, z);

        for (size_t i = 0; i < 4; ++i) {
            const glm::vec2 &tc = texCoords[i];
            const auto tmp = rotation * glm::vec4(tc * 2.f - 1.f, 0, 1);
            const glm::vec2 screenSpaceOffset = scale * glm::vec2(tmp) / tmp.w;
            const glm::vec2 tcOffset = tc * 0.5f
                                       + (p.fill ? glm::vec2(0.5f, 0.5f) : glm::vec2(0.f, 0.0f));
            physicalArrows.emplace_back(dot.offset * dpr, p.color, tcOffset, screenSpaceOffset);
        }
    }

    if (!physicalArrows.empty()) {
        gl.renderFont3d(textures.char_arrows, physicalArrows);
    }
}

void MapCanvas::paintCharacters()
{
    if (m_data.isEmpty()) {
        return;
    }

    updateGroupBatch();
    m_groupBatch.reallyDraw(getOpenGL(), m_textures, getGLFont());

    CharacterBatch localBatch{m_mapScreen, m_currentLayer, getTotalScaleFactor()};

    if (const std::optional<RoomId> opt_pos = m_data.getCurrentRoomId()) {
        const auto &id = opt_pos.value();
        if (const auto room = m_data.findRoomHandle(id)) {
            const auto &pos = room.getPosition();
            const Color color{getConfig().groupManager.color};
            localBatch.drawCharacter(pos, color);
            const auto prespam = m_data.getPath(id, m_prespammedPath.getQueue());
            localBatch.drawPreSpammedPath(pos, prespam, color);
        } else {
            m_data.clearSelectedRoom();
        }
    }

    localBatch.reallyDraw(getOpenGL(), m_textures, getGLFont());
}

void MapCanvas::updateGroupBatch()
{
    if (!m_groupBatchDirty) {
        return;
    }

    m_groupBatch.clear();
    drawGroupCharacters(m_groupBatch, INVALID_SERVER_ROOMID);
    m_groupBatchDirty = false;
}

void MapCanvas::drawGroupCharacters(CharacterBatch &batch, ServerRoomId yourServerId)
{
    if (m_data.isEmpty()) {
        return;
    }

    RoomIdSet drawnRoomIds;
    const Map &map = m_data.getCurrentMap();
    for (const auto &pCharacter : m_groupManager.selectAll()) {
        if (pCharacter->isYou()) {
            continue;
        }

        const CGroupChar &character = deref(pCharacter);
        const ServerRoomId srvId = character.getServerId();

        const auto &r = std::invoke([srvId, &map]() -> RoomHandle {
            if (srvId != INVALID_SERVER_ROOMID) {
                if (const auto &room = map.findRoomHandle(srvId)) {
                    return room;
                }
            }
            return RoomHandle{};
        });

        if (!r) {
            continue;
        }

        const RoomId id = r.getId();
        const auto &pos = r.getPosition();
        const auto color = Color{character.getColor()};
        const bool fill = !drawnRoomIds.contains(id);

        batch.drawCharacter(pos, color, fill);

        if (srvId != INVALID_SERVER_ROOMID && srvId != yourServerId) {
            QString name = character.getLabel().isEmpty() ? character.getName().toQString()
                                                          : character.getLabel().toQString();
            if (!name.isEmpty()) {
                batch.drawName(pos, mmqt::toStdStringLatin1(name), color);
            }
        }

        drawnRoomIds.insert(id);
    }
}
