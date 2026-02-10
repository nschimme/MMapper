// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/EnumIndexedArray.h"
#include "../map/coordinate.h"
#include "../map/room.h"
#include "../mapdata/roomselection.h"
#include "../opengl/OpenGL.h"
#include "../opengl/OpenGLTypes.h"
#include "Characters.h"
#include "MapCanvasData.h"
#include "Textures.h"
#include "mapcanvas.h"

#include <cstdlib>
#include <memory>
#include <optional>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class NODISCARD RoomSelFakeGL final
{
public:
    enum class NODISCARD SelTypeEnum { Near, Distant, MoveBad, MoveGood };
    static constexpr size_t NUM_SEL_TYPES = 4;

private:
    glm::mat4 m_modelView = glm::mat4(1);

    struct Instance
    {
        SelTypeEnum type;
        glm::vec3 base;
        float rotation;
        glm::vec2 scale;
    };
    std::vector<Instance> m_instances;

public:
    void resetMatrix() { m_modelView = glm::mat4(1); }
    void glRotatef(float degrees, float x, float y, float z)
    {
        auto &m = m_modelView;
        m = glm::rotate(m, glm::radians(degrees), glm::vec3(x, y, z));
    }
    void glScalef(float x, float y, float z)
    {
        auto &m = m_modelView;
        m = glm::scale(m, glm::vec3(x, y, z));
    }
    void glTranslatef(int x, int y, int z)
    {
        glTranslatef(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
    }
    void glTranslatef(float x, float y, float z)
    {
        auto &m = m_modelView;
        m = glm::translate(m, glm::vec3(x, y, z));
    }

    void drawColoredQuad(const SelTypeEnum type)
    {
        const auto &m = m_modelView;
        // Extract translation from matrix
        const glm::vec3 base{m[3][0], m[3][1], m[3][2]};
        // Extract scale (approximate if rotated, but room sel quads are usually not rotated here)
        const float sx = glm::length(glm::vec3(m[0]));
        const float sy = glm::length(glm::vec3(m[1]));
        // Extract rotation around Z
        const float rotation = glm::degrees(std::atan2(m[0][1], m[0][0]));

        m_instances.emplace_back(Instance{type, base, rotation, glm::vec2(sx, sy)});
    }

    void draw(OpenGL &gl, const MapCanvasTextures &textures)
    {
        if (m_instances.empty()) {
            return;
        }

        static constexpr MMapper::Array<SelTypeEnum, NUM_SEL_TYPES> ALL_SEL_TYPES{
            SelTypeEnum::Near, SelTypeEnum::Distant, SelTypeEnum::MoveBad, SelTypeEnum::MoveGood};

        std::vector<IconMetrics> metrics(256);
        // Room selection icons are full quads [0,1]
        for (size_t i = 0; i < 256; ++i) {
            metrics[i].sizeAnchor = glm::vec4(1.0, 1.0, 0.0, 0.0); // Default 1.0 world units
            metrics[i].flags = 0;
        }

        std::vector<IconInstanceData> iconBatch;
        iconBatch.reserve(m_instances.size());

        for (const auto &inst : m_instances) {
            const auto &texture = std::invoke([&textures, inst]() -> const SharedMMTexture & {
                switch (inst.type) {
                case SelTypeEnum::Near:
                    return textures.room_sel;
                case SelTypeEnum::Distant:
                    return textures.room_sel_distant;
                case SelTypeEnum::MoveBad:
                    return textures.room_sel_move_bad;
                case SelTypeEnum::MoveGood:
                    return textures.room_sel_move_good;
                default:
                    break;
                }
                std::abort();
            });

            const auto pos = texture->getArrayPosition();
            const auto idx = static_cast<size_t>(pos.position);

            int16_t sw = 0;
            int16_t sh = 0;

            if (inst.type == SelTypeEnum::Distant) {
                // Center anchor for distant indicators
                metrics[idx].sizeAnchor = glm::vec4(0.0, 0.0, -0.5, -0.5);
                metrics[idx].flags = IconMetrics::FIXED_SIZE | IconMetrics::CLAMP_TO_EDGE
                                     | IconMetrics::AUTO_ROTATE;

                const float margin = MapScreen::DEFAULT_MARGIN_PIXELS;
                sw = static_cast<int16_t>(2.f * margin);
                sh = static_cast<int16_t>(2.f * margin);
            } else {
                // Fixed-size world-space selection quads.
                // We use sizeAnchor.xy = (1.0, 1.0) for world-units scaling.
                metrics[idx].sizeAnchor = glm::vec4(1.0, 1.0, 0.0, 0.0);
                metrics[idx].flags = 0;

                sw = static_cast<int16_t>(inst.scale.x);
                sh = static_cast<int16_t>(inst.scale.y);
            }

            iconBatch.emplace_back(inst.base,
                                   0xFFFFFFFF, // white
                                   sw,
                                   sh,
                                   static_cast<uint16_t>(pos.position),
                                   static_cast<int16_t>(inst.rotation));
        }

        if (!iconBatch.empty()) {
            gl.renderIcon3d(textures.room_sel_Array, iconBatch, metrics, gl.getDevicePixelRatio());
        }

        m_instances.clear();
    }
};

void MapCanvas::paintSelectedRoom(RoomSelFakeGL &gl, const RawRoom &room)
{
    const Coordinate &roomPos = room.getPosition();
    const int x = roomPos.x;
    const int y = roomPos.y;
    const int z = roomPos.z;

    // This fake GL uses resetMatrix() before this function.
    gl.resetMatrix();

    const float marginPixels = MapScreen::DEFAULT_MARGIN_PIXELS;
    const bool isMoving = hasRoomSelectionMove();

    if (!isMoving && !m_mapScreen.isRoomVisible(roomPos, marginPixels / 2.f)) {
        const glm::vec3 roomCenter = roomPos.to_vec3() + glm::vec3{0.5f, 0.5f, 0.f};
        gl.glTranslatef(roomCenter.x, roomCenter.y, roomCenter.z);
        // Anchor and rotation will be handled by the shader for Distant selections.
        gl.drawColoredQuad(RoomSelFakeGL::SelTypeEnum::Distant);
    } else {
        // Room is close
        gl.glTranslatef(x, y, z);
        gl.drawColoredQuad(RoomSelFakeGL::SelTypeEnum::Near);
        gl.resetMatrix();
    }

    if (isMoving) {
        gl.resetMatrix();
        const auto &relativeOffset = m_roomSelectionMove->pos;
        gl.glTranslatef(x + relativeOffset.x, y + relativeOffset.y, z);
        gl.drawColoredQuad(m_roomSelectionMove->wrongPlace ? RoomSelFakeGL::SelTypeEnum::MoveBad
                                                           : RoomSelFakeGL::SelTypeEnum::MoveGood);
    }
}

void MapCanvas::paintSelectedRooms()
{
    if (!m_roomSelection || m_roomSelection->empty()) {
        return;
    }

    RoomSelFakeGL gl;

    for (const RoomId id : deref(m_roomSelection)) {
        if (const auto room = m_data.findRoomHandle(id)) {
            gl.resetMatrix();
            paintSelectedRoom(gl, room.getRaw());
        }
    }

    gl.draw(getOpenGL(), m_textures);
}
