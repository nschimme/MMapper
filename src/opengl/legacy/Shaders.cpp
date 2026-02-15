// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "Shaders.h"

#include "../../display/Textures.h"
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
MegaRoomShader::~MegaRoomShader() = default;

void MegaRoomShader::virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms)
{
    setMatrix("uViewProj", mvp);

    if (uniforms.timeOfDayColor.has_value()) {
        setColor("uTimeOfDayColor", uniforms.timeOfDayColor.value());
    } else {
        setColor("uTimeOfDayColor", Colors::white);
    }

    setInt("uCurrentLayer", currentLayer);
    setInt("uMinZ", minZ);
    setInt("uMaxZ", maxZ);
    setBool("uDrawUpperLayersTextured", drawUpperLayersTextured);
    setVec2("uMinBounds", minBounds);
    setVec2("uMaxBounds", maxBounds);

    auto functions = m_functions.lock();
    if (functions) {
        // Textures
        auto bind = [&](const char *name, int unit, MMTextureId id) {
            if (id) {
                const auto *texPtr = functions->getTexLookup().find(id);
                if (texPtr && *texPtr) {
                    (*texPtr)->bind(static_cast<GLuint>(unit));
                    setTexture(name, unit);
                }
            }
        };

        bind("uTerrainRoadArray", 0, uTerrainTex);
        bind("uTrailArray", 1, uTrailTex);
        bind("uOverlayArray", 2, uOverlayTex);
        bind("uWallArray", 3, uWallTex);
        bind("uDottedWallArray", 4, uDottedWallTex);
        bind("uDoorArray", 5, uDoorTex);
        bind("uStreamInArray", 6, uStreamInTex);
        bind("uStreamOutArray", 7, uStreamOutTex);
        bind("uExitIconArray", 8, uExitTex);

        // Layers
        setIntArray("uWallLayers", uWallLayers, 4);
        setIntArray("uDottedWallLayers", uDottedWallLayers, 4);
        setIntArray("uDoorLayers", uDoorLayers, 6);
        setIntArray("uStreamInLayers", uStreamInLayers, 6);
        setIntArray("uStreamOutLayers", uStreamOutLayers, 6);
        setIntArray("uExitLayers", uExitLayers, 4);
    }
}

FontShader::~FontShader() = default;
PointShader::~PointShader() = default;
BlitShader::~BlitShader() = default;
FullScreenShader::~FullScreenShader() = default;

void ShaderPrograms::early_init()
{
    std::ignore = getPlainAColorShader();
    std::ignore = getPlainUColorShader();
    std::ignore = getTexturedAColorShader();
    std::ignore = getTexturedUColorShader();

    std::ignore = getRoomQuadTexShader();
    std::ignore = getMegaRoomShader();

    std::ignore = getFontShader();
    std::ignore = getPointShader();
    std::ignore = getBlitShader();
    std::ignore = getFullScreenShader();
}

void ShaderPrograms::resetAll()
{
    m_aColorShader.reset();
    m_uColorShader.reset();
    m_aTexturedShader.reset();
    m_uTexturedShader.reset();

    m_roomQuadTexShader.reset();
    m_megaRoomShader.reset();

    m_font.reset();
    m_point.reset();
    m_blit.reset();
    m_fullscreen.reset();
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

const std::shared_ptr<MegaRoomShader> &ShaderPrograms::getMegaRoomShader()
{
    return getInitialized<MegaRoomShader>(m_megaRoomShader, getFunctions(), "room/mega");
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

const std::shared_ptr<BlitShader> &ShaderPrograms::getBlitShader()
{
    return getInitialized<BlitShader>(m_blit, getFunctions(), "blit");
}

const std::shared_ptr<FullScreenShader> &ShaderPrograms::getFullScreenShader()
{
    return getInitialized<FullScreenShader>(m_fullscreen, getFunctions(), "fullscreen");
}

} // namespace Legacy
