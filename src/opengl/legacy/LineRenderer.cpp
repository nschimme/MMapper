// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "LineRenderer.h" // Updated include
#include "Legacy.h" // For Functions class definition
#include <QtGui/QOpenGLExtraFunctions> // For QOpenGLExtraFunctions
#include <QDebug> // For qWarning
#include <vector>
#include <stdexcept> // For std::runtime_error

namespace Legacy {

// Updated class name for constructor and destructor
LineRenderer::LineRenderer(
    SharedFunctions sharedFunctions,
    std::shared_ptr<LineShader> shader)
    : m_shared_functions(std::move(sharedFunctions)),
      m_functions(deref(m_shared_functions)), // Initialize reference
      m_shader(std::move(shader)) {
    // m_extraFunctions member removed, no longer initialized here
    if (!m_shared_functions) {
        // Updated error message to reflect new class name
        throw std::runtime_error("LineRenderer: SharedFunctions is null.");
    }
    if (!m_shader) {
        // Updated error message and type name
        throw std::runtime_error("LineRenderer: LineShader is null.");
    }
    // Null check for extraFunctions removed
}

LineRenderer::~LineRenderer() { // Updated class name
    if (m_vao != 0) {
        QOpenGLExtraFunctions* ef = m_functions.getExtraFunctions();
        if (ef) {
            ef->glDeleteVertexArrays(1, &m_vao);
        } else {
            // qWarning might be problematic in a destructor if it throws or causes allocations
            // For now, just skip if extra functions are not available.
        }
        m_vao = 0;
    }
}

// Updated class name for methods
void LineRenderer::setup() {
    m_baseQuadVBO.emplace(m_shared_functions);
    m_instanceDataVBO.emplace(m_shared_functions);
    setupVAO();
}

void LineRenderer::setupVAO() { // Updated class name
    std::vector<glm::vec2> baseQuadVertices = {
        {0.0f, -0.5f},
        {1.0f, -0.5f},
        {0.0f,  0.5f},
        {1.0f,  0.5f}
    };

    QOpenGLExtraFunctions* ef = m_functions.getExtraFunctions();
    if (!ef) {
        qWarning("LineRenderer::setupVAO: QOpenGLExtraFunctions not available. VAO setup cannot proceed.");
        // If VAO cannot be set up, m_vao will remain 0, and render() will bail out.
        return;
    }

    // Use ef for VAO operations
    ef->glGenVertexArrays(1, &m_vao);
    ef->glBindVertexArray(m_vao);

    // VBO operations remain with m_functions (QOpenGLFunctions)
    m_functions.glBindBuffer(GL_ARRAY_BUFFER, m_baseQuadVBO.get());
    m_functions.glBufferData(GL_ARRAY_BUFFER, baseQuadVertices.size() * sizeof(glm::vec2), baseQuadVertices.data(), GL_STATIC_DRAW);

    m_functions.glEnableVertexAttribArray(LOC_BASE_VERTEX_POS);
    m_functions.glVertexAttribPointer(LOC_BASE_VERTEX_POS, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
    ef->glVertexAttribDivisor(LOC_BASE_VERTEX_POS, 0); // Use ef

    // VBO operations remain with m_functions
    m_functions.glBindBuffer(GL_ARRAY_BUFFER, m_instanceDataVBO.get());

    m_functions.glEnableVertexAttribArray(LOC_INSTANCE_START_POINT);
    m_functions.glVertexAttribPointer(LOC_INSTANCE_START_POINT, 2, GL_FLOAT, GL_FALSE, sizeof(LineInstanceData), (void*)offsetof(LineInstanceData, startPoint));
    ef->glVertexAttribDivisor(LOC_INSTANCE_START_POINT, 1); // Use ef

    m_functions.glEnableVertexAttribArray(LOC_INSTANCE_END_POINT);
    m_functions.glVertexAttribPointer(LOC_INSTANCE_END_POINT, 2, GL_FLOAT, GL_FALSE, sizeof(LineInstanceData), (void*)offsetof(LineInstanceData, endPoint));
    ef->glVertexAttribDivisor(LOC_INSTANCE_END_POINT, 1); // Use ef

    m_functions.glEnableVertexAttribArray(LOC_INSTANCE_THICKNESS);
    m_functions.glVertexAttribPointer(LOC_INSTANCE_THICKNESS, 1, GL_FLOAT, GL_FALSE, sizeof(LineInstanceData), (void*)offsetof(LineInstanceData, thickness));
    ef->glVertexAttribDivisor(LOC_INSTANCE_THICKNESS, 1); // Use ef

    m_functions.glEnableVertexAttribArray(LOC_INSTANCE_COLOR);
    m_functions.glVertexAttribPointer(LOC_INSTANCE_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof(LineInstanceData), (void*)offsetof(LineInstanceData, color));
    ef->glVertexAttribDivisor(LOC_INSTANCE_COLOR, 1); // Use ef

    // Use ef for VAO operations
    ef->glBindVertexArray(0);
    // Unbind VBO remains with m_functions
    m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void LineRenderer::updateInstanceData(const std::vector<LineInstanceData>& instanceData) { // Updated class name
    if (!m_instanceDataVBO) {
        if(m_shared_functions) {
             // m_shared_functions->logger().warning("LineRenderer: Instance VBO not valid in updateInstanceData."); // Updated class name in comment
        }
        return;
    }
    m_functions.glBindBuffer(GL_ARRAY_BUFFER, m_instanceDataVBO.get());
    m_functions.glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(LineInstanceData), instanceData.data(), GL_DYNAMIC_DRAW);
    m_functions.glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_numInstances = static_cast<GLsizei>(instanceData.size());
}

void LineRenderer::render(const glm::mat4& mvp, const GLRenderState::Uniforms& uniforms) { // Updated class name
    if (m_numInstances == 0 || m_vao == 0 || !m_shader) {
        return;
    }

    auto programUnbinder = m_shader->bind();
    m_shader->setUniforms(mvp, uniforms);

    QOpenGLExtraFunctions* ef = m_functions.getExtraFunctions();
    if (!ef) {
        // This should ideally not happen if setupVAO succeeded and m_vao is non-zero.
        // If it does, rendering will likely be incorrect or crash.
        // No qWarning here to avoid spamming logs every frame.
        return;
    }
    // Use ef for VAO operations
    ef->glBindVertexArray(m_vao);
    // Drawing remains with m_functions (or could be ef, glDrawArraysInstanced is in both)
    m_functions.glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, m_numInstances);
    ef->glBindVertexArray(0); // Use ef
}

} // namespace Legacy
