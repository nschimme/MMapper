// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "Shaders.h"

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
FontShader::~FontShader() = default;
PointShader::~PointShader() = default;
AColorThickLineShader::~AColorThickLineShader() = default;
UColorThickLineShader::~UColorThickLineShader() = default;

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
                                            ShaderUtils::Source{}, // No geometry shader
                                            getSource("frag.glsl"));
    return std::make_shared<T>(dir, functions.shared_from_this(), std::move(program));
}

// essentially a private member of ShaderPrograms
template<typename T>
NODISCARD static std::shared_ptr<T> loadShaderProgramWithGeometry(
    Functions &functions,
    const std::string &vertDir, const std::string &vertFile,
    const std::string &geomDir, const std::string &geomFile,
    const std::string &fragDir, const std::string &fragFile,
    const std::string &programName // For unique AbstractShaderProgram name
) {
    static_assert(std::is_base_of_v<AbstractShaderProgram, T>);

    const auto getSource = [](const std::string &dir, const std::string &name) -> ShaderUtils::Source {
        if (dir.empty() || name.empty()) return ShaderUtils::Source{};
        return ::readWholeShader(dir, name);
    };

    auto program = ShaderUtils::loadShaders(functions,
                                          getSource(vertDir, vertFile),
                                          getSource(geomDir, geomFile),
                                          getSource(fragDir, fragFile));
    return std::make_shared<T>(programName, functions.shared_from_this(), std::move(program));
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
    return getInitialized<AColorPlainShader>(aColorShader, getFunctions(), "plain/acolor");
}

const std::shared_ptr<UColorPlainShader> &ShaderPrograms::getPlainUColorShader()
{
    return getInitialized<UColorPlainShader>(uColorShader, getFunctions(), "plain/ucolor");
}

const std::shared_ptr<AColorTexturedShader> &ShaderPrograms::getTexturedAColorShader()
{
    return getInitialized<AColorTexturedShader>(aTexturedShader, getFunctions(), "tex/acolor");
}

const std::shared_ptr<UColorTexturedShader> &ShaderPrograms::getTexturedUColorShader()
{
    return getInitialized<UColorTexturedShader>(uTexturedShader, getFunctions(), "tex/ucolor");
}

const std::shared_ptr<FontShader> &ShaderPrograms::getFontShader()
{
    return getInitialized<FontShader>(font, getFunctions(), "font");
}

const std::shared_ptr<PointShader> &ShaderPrograms::getPointShader()
{
    return getInitialized<PointShader>(point, getFunctions(), "point");
}

const std::shared_ptr<AColorThickLineShader> &ShaderPrograms::getPlainAColorThickLineShader() {
    if (!aColorThickLineShader) {
        aColorThickLineShader = loadShaderProgramWithGeometry<AColorThickLineShader>(
            getFunctions(),
            "plain/acolor", "vert.glsl",      // Vertex shader
            "lines_thick", "geom.glsl",       // Geometry shader
            "plain/acolor", "frag.glsl",      // Fragment shader
            "plain/acolor_thickline"          // Program name
        );
    }
    return aColorThickLineShader;
}

const std::shared_ptr<UColorThickLineShader> &ShaderPrograms::getPlainUColorThickLineShader() {
    if (!uColorThickLineShader) {
        uColorThickLineShader = loadShaderProgramWithGeometry<UColorThickLineShader>(
            getFunctions(),
            "plain/ucolor", "vert.glsl",      // Vertex shader
            "lines_thick", "geom.glsl",       // Geometry shader
            "plain/ucolor", "frag.glsl",      // Fragment shader
            "plain/ucolor_thickline"          // Program name
        );
    }
    return uColorThickLineShader;
}

} // namespace Legacy
