#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../global/Color.h"
#include "../global/utils.h"
#include "../opengl/FontFormatFlags.h"
#include "../opengl/GLText.h"
#include "../opengl/OpenGLTypes.h"

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include <QString>

class OpenGL;

struct NODISCARD InfomarksMeshes final
{
    UniqueMesh points;
    UniqueMesh tris;
    UniqueMesh quads;
    std::vector<GLText> text;
    bool isValid = false;
    void render(class ImGuiRenderer &imgui, const struct MapCanvasViewport &viewport);
};

using BatchedInfomarksMeshes = std::unordered_map<int, InfomarksMeshes>;

struct NODISCARD InfomarksBatch final
{
private:
    OpenGL &m_realGL;
    glm::vec3 m_offset{0};
    Color m_color;

    std::vector<ColorVert> m_points;
    std::vector<ColorVert> m_tris;
    std::vector<ColorVert> m_quads;

    std::vector<GLText> m_text;

public:
    explicit InfomarksBatch(OpenGL &gl)
        : m_realGL{gl}
    {}

    void setColor(const Color color) { m_color = color; }
    void setOffset(const glm::vec3 &offset) { m_offset = offset; }
    void drawPoint(const glm::vec3 &a);
    void drawLine(const glm::vec3 &a, const glm::vec3 &b);
    void drawLineStrip(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c)
    {
        drawLine(a, b);
        drawLine(b, c);
    }
    void drawTriangle(const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c);
    void renderText(const glm::vec3 &pos,
                    const std::string &text,
                    const Color color,
                    std::optional<Color> bgcolor,
                    FontFormatFlags fontFormatFlag,
                    int rotationAngle);

    NODISCARD InfomarksMeshes getMeshes();
    void renderImmediate(const GLRenderState &state,
                         class ImGuiRenderer &imgui,
                         const struct MapCanvasViewport &viewport);
};
