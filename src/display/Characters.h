#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/RuleOf5.h"
#include "../global/utils.h"
#include "../map/coordinate.h"
#include "../opengl/Font.h"
#include "../opengl/OpenGLTypes.h"

#include <cassert>
#include <cstddef>
#include <map>
#include <memory>
#include <stack>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <QColor>

class MapScreen;
class OpenGL;
struct MapCanvasTextures;

// TODO: find a better home for this. It's common to characters and room selections.
class NODISCARD DistantObjectTransform final
{
public:
    const glm::vec3 offset{};
    // rotation counterclockwise around the Z axis, starting at the +X axis.
    const float rotationDegrees = 0.f;

public:
    explicit DistantObjectTransform(const glm::vec3 &offset_, const float rotationDegrees_)
        : offset{offset_}
        , rotationDegrees{rotationDegrees_}
    {}

public:
    // Caller must apply the correct translation and rotation.
    NODISCARD static DistantObjectTransform construct(const glm::vec3 &pos,
                                                      const MapScreen &mapScreen,
                                                      float marginPixels);
};

class NODISCARD CharacterBatch final
{
private:
    static constexpr const float FILL_ALPHA = 0.1f;
    static constexpr const float BEACON_ALPHA = 0.10f;
    static constexpr const float LINE_ALPHA = 0.9f;

private:
    struct NODISCARD Matrices final
    {
        // glm::mat4 proj = glm::mat4(1);
        glm::mat4 modelView = glm::mat4(1);
    };

    class NODISCARD MatrixStack final : private std::stack<Matrices>
    {
    private:
        using base = std::stack<Matrices>;

    public:
        MatrixStack() { base::push(Matrices()); }
        ~MatrixStack() { assert(base::size() == 1); }
        void push() { base::push(top()); }
        using base::pop;
        using base::top;
    };

    class NODISCARD CharFakeGL final
    {
    public:
        struct NODISCARD CharacterMeshes final
        {
            UniqueMesh tris;
            UniqueMesh beaconQuads;
            UniqueMesh lines;
            UniqueMesh roomQuads;
            UniqueMesh pathPoints;
            UniqueMesh pathLineQuads;
            bool isValid = false;
        };

    private:
        struct NODISCARD CoordCompare final
        {
            // REVISIT: just make this the global operator<(),
            // so it can be picked up by std::less<Coordinate>.
            bool operator()(const Coordinate &lhs, const Coordinate &rhs) const
            {
                if (lhs.x < rhs.x) {
                    return true;
                }
                if (rhs.x < lhs.x) {
                    return false;
                }
                if (lhs.y < rhs.y) {
                    return true;
                }
                if (rhs.y < lhs.y) {
                    return false;
                }
                return lhs.z < rhs.z;
            }
        };

        struct NODISCARD BatchedName final
        {
            glm::vec3 worldPos;
            std::string text;
            Color color;
            Color bgcolor;
            int stackIdx;
        };

        struct NODISCARD BatchedPlayer final
        {
            Coordinate pos;
            Color color;
            bool fill;
        };

    private:
        Color m_color;
        MatrixStack m_stack;
        std::vector<ColorVert> m_charTris;
        std::vector<ColorVert> m_charBeaconQuads;
        std::vector<ColorVert> m_charLines;
        std::vector<ColoredTexVert> m_charRoomQuads;
        std::vector<ColorVert> m_pathPoints;
        std::vector<ColorVert> m_pathLineQuads;
        std::vector<BatchedName> m_names;
        std::vector<BatchedPlayer> m_players;
        std::map<Coordinate, int, CoordCompare> m_coordCounts;
        std::map<Coordinate, int, CoordCompare> m_nameStackCounts;
        CharacterMeshes m_meshes;

    public:
        CharFakeGL() = default;
        ~CharFakeGL() = default;
        DELETE_CTORS_AND_ASSIGN_OPS(CharFakeGL);

    public:
        void clear()
        {
            m_charTris.clear();
            m_charBeaconQuads.clear();
            m_charLines.clear();
            m_charRoomQuads.clear();
            m_pathPoints.clear();
            m_pathLineQuads.clear();
            m_names.clear();
            m_players.clear();
            m_coordCounts.clear();
            m_nameStackCounts.clear();
            m_meshes.isValid = false;
        }

        void reallyDraw(OpenGL &gl,
                        const MapCanvasTextures &textures,
                        GLFont &font,
                        const MapScreen &mapScreen)
        {
            if (!m_meshes.isValid && !empty()) {
                bake(gl, textures);
            }
            reallyDrawMeshes(gl, textures);
            reallyDrawNames(gl, font, mapScreen);
            reallyDrawArrows(gl, textures, mapScreen);
        }

        bool empty() const
        {
            return m_charTris.empty() && m_charBeaconQuads.empty() && m_charLines.empty()
                   && m_charRoomQuads.empty() && m_pathPoints.empty() && m_pathLineQuads.empty()
                   && m_names.empty() && m_players.empty();
        }

    private:
        void bake(OpenGL &gl, const MapCanvasTextures &textures);
        void reallyDrawMeshes(OpenGL &gl, const MapCanvasTextures &textures);
        void reallyDrawNames(OpenGL &gl, GLFont &font, const MapScreen &mapScreen);
        void reallyDrawArrows(OpenGL &gl,
                              const MapCanvasTextures &textures,
                              const MapScreen &mapScreen);

    public:
        void setColor(const Color color) { m_color = color; }
        void reserve(const Coordinate c) { m_coordCounts[c]++; }
        void clear(const Coordinate c) { m_coordCounts[c] = 0; }

    public:
        void glPushMatrix() { m_stack.push(); }
        void glPopMatrix() { m_stack.pop(); }
        void glRotateZ(float degrees)
        {
            auto &m = m_stack.top().modelView;
            m = glm::rotate(m, glm::radians(degrees), glm::vec3{0, 0, 1});
        }
        void glScalef(float x, float y, float z)
        {
            auto &m = m_stack.top().modelView;
            m = glm::scale(m, glm::vec3{x, y, z});
        }
        void glTranslatef(const glm::vec3 &v)
        {
            auto &m = m_stack.top().modelView;
            m = glm::translate(m, v);
        }
        void drawArrow(bool fill, bool beacon);
        void drawBox(const Coordinate &coord, bool fill, bool beacon, bool isFar);
        void addScreenSpaceArrow(const glm::vec3 &pos, float degrees, const Color color, bool fill);
        void addName(const Coordinate &c, const std::string &name, const Color color);
        void addPlayer(const Coordinate &c, const Color color, bool fill)
        {
            m_players.emplace_back(BatchedPlayer{c, color, fill});
        }
        void drawPathSegment(const glm::vec3 &p1, const glm::vec3 &p2, const Color color);

        // with blending, without depth; always size 8
        void drawPathPoint(const Color color, const glm::vec3 &pos)
        {
            m_pathPoints.emplace_back(color, pos);
        }

    private:
        enum class NODISCARD QuadOptsEnum : uint32_t {
            NONE = 0,
            OUTLINE = 1,
            FILL = 2,
            BEACON = 4
        };
        NODISCARD friend QuadOptsEnum operator|(const QuadOptsEnum lhs, const QuadOptsEnum rhs)
        {
            return static_cast<QuadOptsEnum>(static_cast<uint32_t>(lhs)
                                             | static_cast<uint32_t>(rhs));
        }
        NODISCARD friend QuadOptsEnum operator&(const QuadOptsEnum lhs, const QuadOptsEnum rhs)
        {
            return static_cast<QuadOptsEnum>(static_cast<uint32_t>(lhs)
                                             & static_cast<uint32_t>(rhs));
        }
        void drawQuadCommon(const glm::vec2 &a,
                            const glm::vec2 &b,
                            const glm::vec2 &c,
                            const glm::vec2 &d,
                            QuadOptsEnum options);
    };

private:
    const MapScreen &m_mapScreen;
    const int m_currentLayer;
    const float m_scale;
    CharFakeGL m_fakeGL;

public:
    explicit CharacterBatch(const MapScreen &mapScreen, const int currentLayer, const float scale)
        : m_mapScreen(mapScreen)
        , m_currentLayer(currentLayer)
        , m_scale(scale)
    {}

protected:
    NODISCARD CharFakeGL &getOpenGL() { return m_fakeGL; }

public:
    void incrementCount(const Coordinate &c) { getOpenGL().reserve(c); }

    void resetCount(const Coordinate &c) { getOpenGL().clear(c); }

    NODISCARD bool isVisible(const Coordinate &c, float margin) const;

public:
    void clear() { getOpenGL().clear(); }
    NODISCARD bool empty() const { return m_fakeGL.empty(); }

    void drawCharacter(const Coordinate &coordinate, const Color color, bool fill = true);

    void drawName(const Coordinate &c, const std::string &name, const Color color)
    {
        getOpenGL().addName(c, name, color);
    }

    void drawPreSpammedPath(const Coordinate &coordinate,
                            const std::vector<Coordinate> &path,
                            const Color color);

public:
    void reallyDraw(OpenGL &gl, const MapCanvasTextures &textures, GLFont &font)
    {
        m_fakeGL.reallyDraw(gl, textures, font, m_mapScreen);
    }
};
