#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../opengl/OpenGL.h"
#include "../opengl/OpenGLTypes.h"
#include "../opengl/legacy/Legacy.h"

#include <memory>

class WeatherRenderer;

namespace Legacy {

class NODISCARD WeatherAtmosphereMesh final : public IRenderable
{
private:
    const SharedFunctions m_shared_functions;
    Functions &m_functions;

public:
    explicit WeatherAtmosphereMesh(SharedFunctions shared_functions);
    ~WeatherAtmosphereMesh() override;

private:
    void virt_clear() override {}
    void virt_reset() override {}
    NODISCARD bool virt_isEmpty() const override { return false; }
    void virt_render(const GLRenderState &renderState) override;
};

class NODISCARD WeatherTimeOfDayMesh final : public IRenderable
{
private:
    const SharedFunctions m_shared_functions;
    Functions &m_functions;

public:
    explicit WeatherTimeOfDayMesh(SharedFunctions shared_functions);
    ~WeatherTimeOfDayMesh() override;

private:
    void virt_clear() override {}
    void virt_reset() override {}
    NODISCARD bool virt_isEmpty() const override { return false; }
    void virt_render(const GLRenderState &renderState) override;
};

class NODISCARD WeatherSimulationMesh final : public IRenderable
{
private:
    const SharedFunctions m_shared_functions;
    Functions &m_functions;
    WeatherRenderer &m_renderer;

public:
    explicit WeatherSimulationMesh(SharedFunctions shared_functions, WeatherRenderer &renderer);
    ~WeatherSimulationMesh() override;

private:
    void init();
    void virt_clear() override {}
    void virt_reset() override {}
    NODISCARD bool virt_isEmpty() const override { return false; }
    void virt_render(const GLRenderState &renderState) override;
};

class NODISCARD WeatherTorchMesh final : public IRenderable
{
private:
    const SharedFunctions m_shared_functions;
    Functions &m_functions;

public:
    explicit WeatherTorchMesh(SharedFunctions shared_functions);
    ~WeatherTorchMesh() override;

private:
    void virt_clear() override {}
    void virt_reset() override {}
    NODISCARD bool virt_isEmpty() const override { return false; }
    void virt_render(const GLRenderState &renderState) override;
};

class NODISCARD WeatherParticleMesh final : public IRenderable
{
private:
    const SharedFunctions m_shared_functions;
    Functions &m_functions;
    WeatherRenderer &m_renderer;

public:
    explicit WeatherParticleMesh(SharedFunctions shared_functions, WeatherRenderer &renderer);
    ~WeatherParticleMesh() override;

private:
    void virt_clear() override {}
    void virt_reset() override {}
    NODISCARD bool virt_isEmpty() const override { return false; }
    void virt_render(const GLRenderState &renderState) override;
};

} // namespace Legacy
