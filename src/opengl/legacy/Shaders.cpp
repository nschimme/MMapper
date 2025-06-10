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
TexturedArrayProgram::~TexturedArrayProgram() = default;
// InstancedArrayIconProgram destructor definition
InstancedArrayIconProgram::~InstancedArrayIconProgram() = default;


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

const std::shared_ptr<TexturedArrayProgram> &ShaderPrograms::getTexturedArrayProgram()
{
    // TODO: Ensure ShaderUtils::loadShaders (or a new specialized version for this program)
    // calls glBindAttribLocation(programId, Attributes::TEX_LAYER, "aTexLayer");
    // BEFORE glLinkProgram.
    return getInitialized<TexturedArrayProgram>(m_texturedArrayProgram, getFunctions(), "tex_array/ucolor"); // Updated path
}

// --- InstancedArrayIconProgram ---
// --- TexturedArrayProgram implementation (if different from header) ---
void TexturedArrayProgram::virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms)
{
    assert(uniforms.textures[0] != INVALID_MM_TEXTURE_ID);
    setMatrix("uMVP", mvp); // uMVP from base class or specific if needed
    setTexture("tex_array_sampler", 0);
}


// --- InstancedArrayIconProgram Implementation ---
std::shared_ptr<InstancedArrayIconProgram> InstancedArrayIconProgram::create(
    Functions &functions, const std::string &vert_resource_path, const std::string &frag_resource_path)
{
    // This callback will be executed by loadShaders before glLinkProgram
    auto preLinkCallback = [&](GLuint programId) {
        functions.bindAttribLocation(programId, AttributesEnum::ATTR_BASE_QUAD_POSITION, "a_quad_pos");
        functions.bindAttribLocation(programId, AttributesEnum::ATTR_BASE_QUAD_UV, "a_quad_uv");
        functions.bindAttribLocation(programId, AttributesEnum::ATTR_INSTANCE_WORLD_POS_CENTER, "a_instance_world_pos_center");
        functions.bindAttribLocation(programId, AttributesEnum::ATTR_INSTANCE_TEX_LAYER_INDEX, "a_instance_tex_layer");
    };

    // Note: ShaderUtils::loadShaders expects resource paths that QFile can read.
    // The existing loadSimpleShaderProgram helper constructs these from a directory.
    // Here, we assume vert_resource_path and frag_resource_path are direct resource paths.
    ShaderUtils::Source vertSource{vert_resource_path, ::readWholeResourceFile(vert_resource_path)};
    ShaderUtils::Source fragSource{frag_resource_path, ::readWholeResourceFile(frag_resource_path)};

    Program program = ShaderUtils::loadShaders(functions, vertSource, fragSource, preLinkCallback);

    auto instance = std::make_shared<InstancedArrayIconProgram>(
        "instanced_icons", // dirName, used for logging/debugging, can be extracted from path
        functions.shared_from_this(),
        std::move(program)
    );
    instance->cacheUniformLocations(); // Cache locations after linking
    return instance;
}

void InstancedArrayIconProgram::cacheUniformLocations() {
    // Bind program to query locations
    ProgramUnbinder unbinder = bind(); // Ensures unbind on scope exit
    m_u_projection_view_matrix_loc = getUniformLocation("u_projection_view_matrix");
    m_u_icon_base_size_loc = getUniformLocation("u_icon_base_size");
    m_u_tex_array_sampler_loc = getUniformLocation("u_tex_array_sampler");
}

void InstancedArrayIconProgram::virt_setUniforms(const glm::mat4 &mvp, const GLRenderState::Uniforms &uniforms) {
    // This method is called when the shader is already bound.
    // It's part of the generic rendering path.
    // We use the cached locations for efficiency.

    setUniformMatrix4fv(m_u_projection_view_matrix_loc, 1, GL_FALSE, glm::value_ptr(mvp));

    // GLRenderState.uniforms needs an iconSize field. Using pointSize as a temporary placeholder.
    float iconSize = uniforms.pointSize.value_or(1.0f); // Default to 1.0 world unit for icon size
    setUniform1fv(m_u_icon_base_size_loc, 1, &iconSize);

    assert(uniforms.textures[0] != INVALID_MM_TEXTURE_ID); // This is the texture array ID
    int textureUnit = uniforms.textures[0].value();
    setUniform1iv(m_u_tex_array_sampler_loc, 1, &textureUnit); // Texture unit 0
}

void InstancedArrayIconProgram::setProjectionViewMatrix(const glm::mat4& matrix) {
    setUniformMatrix4fv(m_u_projection_view_matrix_loc, 1, GL_FALSE, glm::value_ptr(matrix));
}

void InstancedArrayIconProgram::setIconBaseSize(float size) {
    setUniform1fv(m_u_icon_base_size_loc, 1, &size);
}

void InstancedArrayIconProgram::setTextureSampler(int texture_unit) {
    setUniform1iv(m_u_tex_array_sampler_loc, 1, &texture_unit);
}


// ShaderPrograms class implementation
std::shared_ptr<InstancedArrayIconProgram> ShaderPrograms::getInstancedArrayIconProgram()
{
    if (!m_instancedArrayIconProgram) {
        // Path should correspond to how files are in QRC and how readWholeShader forms paths
        // Or pass full resource paths directly if create method is adapted.
        // The create method now takes full paths.
        m_instancedArrayIconProgram = InstancedArrayIconProgram::create(
            getFunctions(),
            ":/shaders/legacy/instanced_icons/vert.glsl",
            ":/shaders/legacy/instanced_icons/frag.glsl"
        );
    }
    return m_instancedArrayIconProgram;
}

void ShaderPrograms::resetAll()
{
    aColorShader.reset();
    uColorShader.reset();
    aTexturedShader.reset();
    uTexturedShader.reset();
    font.reset();
    point.reset();
    m_texturedArrayProgram.reset();
    m_instancedArrayIconProgram.reset(); // Reset new program
}


} // namespace Legacy
