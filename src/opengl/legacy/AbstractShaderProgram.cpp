// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "AbstractShaderProgram.h"
#include <QDebug> // For qCritical

namespace Legacy {

AbstractShaderProgram::AbstractShaderProgram(std::string dirName,
                                             SharedFunctions functions,
                                             Program program)
    : m_dirName{std::move(dirName)}
    , m_functions{std::move(functions)}
    , m_program{std::move(program)}
{}

AbstractShaderProgram::~AbstractShaderProgram()
{
    assert(!m_isBound);
}

AbstractShaderProgram::ProgramUnbinder AbstractShaderProgram::bind()
{
    assert(!m_isBound);
    if (auto f = m_functions.lock()) {
        f->glUseProgram(getProgram());
    }
    m_isBound = true;
    return ProgramUnbinder{*this};
}

void AbstractShaderProgram::unbind()
{
    assert(m_isBound);
    if (auto f = m_functions.lock()) {
        f->glUseProgram(0);
    }
    m_isBound = false;
}

void AbstractShaderProgram::setUniforms(const glm::mat4 &mvp,
                                        const GLRenderState::Uniforms &uniforms)
{
    assert(m_isBound);
    virt_setUniforms(mvp, uniforms);

    if (uniforms.pointSize) {
        setPointSize(uniforms.pointSize.value());
    }
}

GLuint AbstractShaderProgram::getAttribLocation(const char *const name) const
{
    assert(name != nullptr);
    // Program does not need to be bound to call glGetAttribLocation,
    // but it must have been successfully linked.
    // assert(m_isBound); // Removing this assert for now.

    auto functions = m_functions.lock();
    if (!functions) {
        qCritical("AbstractShaderProgram: Functions context is null when getting attribute '%s' for shader '%s'.", name, m_dirName.c_str());
        return Legacy::INVALID_ATTRIB_LOCATION; // Or appropriate global INVALID_ATTRIB_LOCATION
    }

    const GLint location = functions->glGetAttribLocation(getProgram(), name);

    if (location == -1) {
        // It's common for shaders to optimize out unused attributes.
        // This might not always be a critical error, but a warning is useful.
        // qWarning("AbstractShaderProgram: Attribute '%s' not found or not active in shader program '%s' (dir: '%s'). Location: %d",
        //          name, m_program.get(), m_dirName.c_str(), location);
        // For now, let's keep it as a qCritical if it's unexpected by calling code.
        // The original error GL_INVALID_VALUE in glEnableVertexAttribArray implies it *was* unexpected.
        qCritical("AbstractShaderProgram: Attribute '%s' not found or not active in shader program %u (dir: '%s'). Returned location -1.",
                  name, getProgram(), m_dirName.c_str());
        return Legacy::INVALID_ATTRIB_LOCATION;
    }
    return static_cast<GLuint>(location);
}

GLint AbstractShaderProgram::getUniformLocation(const char *const name) const
{
    assert(name != nullptr);
    assert(m_isBound);
    auto functions = m_functions.lock();
    const auto result = deref(functions).glGetUniformLocation(getProgram(), name);
    assert(result != INVALID_UNIFORM_LOCATION);
    return result;
}

bool AbstractShaderProgram::hasUniform(const char *const name) const
{
    assert(name != nullptr);
    auto functions = m_functions.lock();
    const auto result = deref(functions).glGetUniformLocation(getProgram(), name);
    return result != INVALID_UNIFORM_LOCATION;
}

void AbstractShaderProgram::setUniform1iv(const GLint location,
                                          const GLsizei count,
                                          const GLint *const value)
{
    assert(m_isBound);
    auto functions = m_functions.lock();
    deref(functions).glUniform1iv(location, count, value);
}

void AbstractShaderProgram::setUniform1fv(const GLint location,
                                          const GLsizei count,
                                          const GLfloat *const value)
{
    assert(m_isBound);
    auto functions = m_functions.lock();
    deref(functions).glUniform1fv(location, count, value);
}

void AbstractShaderProgram::setUniform4fv(const GLint location,
                                          const GLsizei count,
                                          const GLfloat *const value)
{
    assert(m_isBound);
    auto functions = m_functions.lock();
    deref(functions).glUniform4fv(location, count, value);
}

void AbstractShaderProgram::setUniform4iv(const GLint location,
                                          const GLsizei count,
                                          const GLint *const value)
{
    assert(m_isBound);
    auto functions = m_functions.lock();
    deref(functions).glUniform4iv(location, count, value);
}

void AbstractShaderProgram::setUniformMatrix4fv(const GLint location,
                                                const GLsizei count,
                                                const GLboolean transpose,
                                                const GLfloat *const value)
{
    assert(m_isBound);
    auto functions = m_functions.lock();
    deref(functions).glUniformMatrix4fv(location, count, transpose, value);
}

float AbstractShaderProgram::getDevicePixelRatio() const
{
    auto functions = m_functions.lock();
    return deref(functions).getDevicePixelRatio();
}

void AbstractShaderProgram::setPointSize(const GLfloat in_pointSize)
{
    const auto location = getUniformLocation("uPointSize");
    if (location != INVALID_UNIFORM_LOCATION) {
        const GLfloat pointSizeValue = in_pointSize * static_cast<GLfloat>(getDevicePixelRatio());
        setUniform1fv(location, 1, &pointSizeValue);
    }
}

void AbstractShaderProgram::setColor(const char *const name, const Color &color)
{
    const auto location = getUniformLocation(name);
    const glm::vec4 v = color.getVec4();
    setUniform4fv(location, 1, glm::value_ptr(v));
}

void AbstractShaderProgram::setMatrix(const char *const name, const glm::mat4 &m)
{
    const auto location = getUniformLocation(name);
    setUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(m));
}

void AbstractShaderProgram::setTexture(const char *const name, const int textureUnit)
{
    assert(textureUnit >= 0);
    const GLint uFontTextureLoc = getUniformLocation(name);
    setUniform1iv(uFontTextureLoc, 1, &textureUnit);
}

void AbstractShaderProgram::setViewport(const char *const name, const Viewport &input_viewport)
{
    const glm::ivec4 viewport{input_viewport.offset, input_viewport.size};
    const GLint location = getUniformLocation(name);
    setUniform4iv(location, 1, glm::value_ptr(viewport));
}

void AbstractShaderProgram::setFloat(const char *const name, const GLfloat value)
{
    const auto location = getUniformLocation(name);
    // It's good practice to check if the uniform location is valid before using it,
    // though existing methods like setColor don't always do this.
    // For consistency with setColor, we can omit the check, but be aware.
    if (location != INVALID_UNIFORM_LOCATION) { // Optional: check if uniform exists
        setUniform1fv(location, 1, &value);
    }
}

} // namespace Legacy
