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

NODISCARD static ShaderUtils::Source readWholeShader(const std::string &shader,
                                                 const std::string &dir,
                                                 const std::string &name)
{
    const auto fullPathName = ":/shaders/" + shader + "/" + dir + "/" + name;
    return ShaderUtils::Source{fullPathName, readWholeResourceFile(fullPathName)};
}

namespace Legacy {

AColorPlainShader::~AColorPlainShader() = default;
UColorPlainShader::~UColorPlainShader() = default;
AColorTexturedShader::~AColorTexturedShader() = default;
UColorTexturedShader::~UColorTexturedShader() = default;
FontShader::~FontShader() = default;
PointShader::~PointShader() = default;
LineShader::~LineShader() = default;

// essentially a private member of ShaderPrograms
template<typename T>
NODISCARD static std::shared_ptr<T> loadSimpleShaderProgram(Functions &functions,
                                                            const std::string &shader,
                                                            const std::string &dir)
{
    static_assert(std::is_base_of_v<AbstractShaderProgram, T>);

    const auto getSource = [&shader, &dir](const std::string &name) -> ShaderUtils::Source {
        return ::readWholeShader(shader, dir, name);
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
                                                          const std::string &shaderName,
                                                          const std::string &dir)
{
    if (!shader) {
        shader = Legacy::loadSimpleShaderProgram<T>(functions, shaderName, dir);
    }
    return shader;
}

const std::shared_ptr<AColorPlainShader> &ShaderPrograms::getPlainAColorShader()
{
    return getInitialized<AColorPlainShader>(m_aColorShader,
                                             getFunctions(),
                                             "legacy",
                                             "plain/acolor");
}

const std::shared_ptr<UColorPlainShader> &ShaderPrograms::getPlainUColorShader()
{
    return getInitialized<UColorPlainShader>(m_uColorShader,
                                             getFunctions(),
                                             "legacy",
                                             "plain/ucolor");
}

const std::shared_ptr<AColorTexturedShader> &ShaderPrograms::getTexturedAColorShader()
{
    return getInitialized<AColorTexturedShader>(m_aTexturedShader,
                                                getFunctions(),
                                                "legacy",
                                                "tex/acolor");
}

const std::shared_ptr<UColorTexturedShader> &ShaderPrograms::getTexturedUColorShader()
{
    return getInitialized<UColorTexturedShader>(m_uTexturedShader,
                                                getFunctions(),
                                                "legacy",
                                                "tex/ucolor");
}

const std::shared_ptr<FontShader> &ShaderPrograms::getFontShader()
{
    return getInitialized<FontShader>(m_font, getFunctions(), "legacy", "font");
}

const std::shared_ptr<PointShader> &ShaderPrograms::getPointShader()
{
    return getInitialized<PointShader>(m_point, getFunctions(), "legacy", "point");
}

const std::shared_ptr<LineShader> &ShaderPrograms::getLineShader()
{
    return getInitialized<LineShader>(m_line, getFunctions(), "legacy", "line");
}

} // namespace Legacy
