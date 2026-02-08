// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "Shaders.h"

#include "../../display/Textures.h"
#include "FunctionsES30.h"
#include "FunctionsGL33.h"
#include "Legacy.h"
#include "ShaderUtils.h"

#include <memory>

#include <QFile>

NODISCARD static std::string readWholeResourceFile(const std::string &fullPath)
{
    QFile f{QString(fullPath.c_str())};
    if (!f.exists() || !f.open(QIODevice::OpenModeFlag::ReadOnly | QIODevice::OpenModeFlag::Text)) {
        throw std::runtime_error("error opening file");
    }
    QTextStream in(&f);
    return in.readAll().toUtf8().toStdString();
}

NODISCARD static ShaderUtils::Source readWholeShader(const std::string &dir, const std::string &name)
{
    const auto fullPathName = ":/shaders/legacy/" + dir + "/" + name;
    return ShaderUtils::Source{fullPathName, readWholeResourceFile(fullPathName)};
}

namespace Legacy {

AColorPlainShader::~AColorPlainShader() = default;
UColorPlainShader::~UColorPlainShader() = default;
AColorTexturedShader::~AColorTexturedShader() = default;
UColorTexturedShader::~UColorTexturedShader() = default;

RoomQuadTexShader::~RoomQuadTexShader() = default;

FontShader::~FontShader() = default;

void FontShader::virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms)
{
    assert(uniforms.textures[0] != INVALID_MM_TEXTURE_ID);
    auto shared_functions = m_functions.lock();
    Functions &functions = deref(shared_functions);

    setMatrix("uMVP3D", mvp);
    setTexture("uFontTexture", 0);
    const Viewport vp = functions.getPhysicalViewport();
    setViewport("uPhysViewport", vp);
    setFloat("uDevicePixelRatio", uniforms.dprScale);

    const auto buffer = SharedVboEnum::GlyphMetricsBlock;
    const auto shared = functions.getSharedVbos().get(buffer);
    VBO &vbo = deref(shared);
    if (vbo) {
        functions.glBindBufferBase(GL_UNIFORM_BUFFER, buffer, vbo.get());
    }
}
PointShader::~PointShader() = default;

IconShader::~IconShader() = default;

void IconShader::virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms)
{
    assert(uniforms.textures[0] != INVALID_MM_TEXTURE_ID);
    auto shared_functions = m_functions.lock();
    Functions &functions = deref(shared_functions);

    setMatrix("uMVP3D", mvp);
    setVec3("uMapCenter", functions.getMapCenter());
    setFloat("uBaseSize", functions.getBaseSize());
    setTexture("uIconTexture", 0);
    const Viewport vp = functions.getPhysicalViewport();
    setViewport("uPhysViewport", vp);
    setFloat("uDevicePixelRatio", uniforms.dprScale);

    const auto buffer = SharedVboEnum::IconMetricsBlock;
    const auto shared = functions.getSharedVbos().get(buffer);
    VBO &vbo = deref(shared);
    if (vbo) {
        functions.glBindBufferBase(GL_UNIFORM_BUFFER, buffer, vbo.get());
    }
}

void ShaderPrograms::early_init()
{
    std::ignore = getPlainAColorShader();
    std::ignore = getPlainUColorShader();
    std::ignore = getTexturedAColorShader();
    std::ignore = getTexturedUColorShader();

    std::ignore = getRoomQuadTexShader();

    std::ignore = getFontShader();
    std::ignore = getPointShader();
    std::ignore = getIconShader();
}

void ShaderPrograms::resetAll()
{
    m_aColorShader.reset();
    m_uColorShader.reset();
    m_aTexturedShader.reset();
    m_uTexturedShader.reset();

    m_roomQuadTexShader.reset();

    m_font.reset();
    m_point.reset();
    m_icon.reset();
}

// essentially a private member of ShaderPrograms
template<typename T>
NODISCARD static std::shared_ptr<T> loadSimpleShaderProgram(Functions &functions,
                                                            const std::string &dir)
{
    static_assert(std::is_base_of_v<AbstractShaderProgram, T>);

    const auto getSource = [&dir](const std::string &name) -> ShaderUtils::Source {
        return ::readWholeShader(dir, name);
    };

    auto program = ShaderUtils::loadShaders(functions,
                                            getSource("vert.glsl"),
                                            getSource("frag.glsl"));
    return std::make_shared<T>(dir, functions.shared_from_this(), std::move(program));
}

// essentially a private member of ShaderPrograms
template<typename T>
NODISCARD static const std::shared_ptr<T> &getInitialized(std::shared_ptr<T> &shader,
                                                          Functions &functions,
                                                          const std::string &dir)
{
    if (!shader) {
        shader = Legacy::loadSimpleShaderProgram<T>(functions, dir);
    }
    return shader;
}

const std::shared_ptr<AColorPlainShader> &ShaderPrograms::getPlainAColorShader()
{
    return getInitialized<AColorPlainShader>(m_aColorShader, getFunctions(), "plain/acolor");
}

const std::shared_ptr<UColorPlainShader> &ShaderPrograms::getPlainUColorShader()
{
    return getInitialized<UColorPlainShader>(m_uColorShader, getFunctions(), "plain/ucolor");
}

const std::shared_ptr<AColorTexturedShader> &ShaderPrograms::getTexturedAColorShader()
{
    return getInitialized<AColorTexturedShader>(m_aTexturedShader, getFunctions(), "tex/acolor");
}

const std::shared_ptr<RoomQuadTexShader> &ShaderPrograms::getRoomQuadTexShader()
{
    return getInitialized<RoomQuadTexShader>(m_roomQuadTexShader, getFunctions(), "room/tex/acolor");
}

const std::shared_ptr<UColorTexturedShader> &ShaderPrograms::getTexturedUColorShader()
{
    return getInitialized<UColorTexturedShader>(m_uTexturedShader, getFunctions(), "tex/ucolor");
}

const std::shared_ptr<FontShader> &ShaderPrograms::getFontShader()
{
    return getInitialized<FontShader>(m_font, getFunctions(), "font");
}

const std::shared_ptr<PointShader> &ShaderPrograms::getPointShader()
{
    return getInitialized<PointShader>(m_point, getFunctions(), "point");
}

const std::shared_ptr<IconShader> &ShaderPrograms::getIconShader()
{
    return getInitialized<IconShader>(m_icon, getFunctions(), "icon");
}

} // namespace Legacy
