// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "../configuration/NamedConfig.h"
#include "../configuration/configuration.h"
#include "../global/Array.h"
#include "../global/ChangeMonitor.h"
#include "../global/ConfigConsts.h"
#include "../global/RuleOf5.h"
#include "../global/logging.h"
#include "../global/progresscounter.h"
#include "../global/utils.h"
#include "../map/coordinate.h"
#include "../mapdata/mapdata.h"
#include "../opengl/Font.h"
#include "../opengl/FontFormatFlags.h"
#include "../opengl/OpenGL.h"
#include "../opengl/OpenGLTypes.h"
#include "Connections.h"
#include "MapCanvasConfig.h"
#include "MapCanvasData.h"
#include "MapCanvasRoomDrawer.h" // For LayerMeshes::render signature
#include "MapBatches.h"          // For MapBatches, LayerMeshes types
#include "Textures.h"
#include "connectionselection.h"
#include "mapcanvas.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <QMessageBox>
#include <QMessageLogContext>
#include <QOpenGLWidget>
#include <QtCore>
#include <QtGui/qopengl.h>
#include <QtGui>

namespace MapCanvasConfig {

static std::mutex g_version_lock;
static std::string g_current_gl_version = "UN0.0";

static void setCurrentOpenGLVersion(std::string version)
{
    std::unique_lock<std::mutex> lock{g_version_lock};
    g_current_gl_version = std::move(version);
}

std::string getCurrentOpenGLVersion()
{
    std::unique_lock<std::mutex> lock{g_version_lock};
    return g_current_gl_version;
}

void registerChangeCallback(const ChangeMonitor::Lifetime &lifetime,
                            ChangeMonitor::Function callback)
{
    return setConfig().canvas.advanced.registerChangeCallback(lifetime, std::move(callback));
}

bool isIn3dMode()
{
    return getConfig().canvas.advanced.use3D.get();
}

void set3dMode(bool is3d)
{
    setConfig().canvas.advanced.use3D.set(is3d);
}

bool isAutoTilt()
{
    return getConfig().canvas.advanced.autoTilt.get();
}

void setAutoTilt(const bool val)
{
    setConfig().canvas.advanced.autoTilt.set(val);
}

bool getShowPerfStats()
{
    return getConfig().canvas.advanced.printPerfStats.get();
}

void setShowPerfStats(const bool show)
{
    setConfig().canvas.advanced.printPerfStats.set(show);
}

} // namespace MapCanvasConfig

class NODISCARD MakeCurrentRaii final
{
private:
    QOpenGLWidget &m_glWidget;

public:
    explicit MakeCurrentRaii(QOpenGLWidget &widget)
        : m_glWidget{widget}
    {
        m_glWidget.makeCurrent();
    }
    ~MakeCurrentRaii() { m_glWidget.doneCurrent(); }

    DELETE_CTORS_AND_ASSIGN_OPS(MakeCurrentRaii);
};

void MapCanvas::cleanupOpenGL()
{
    MakeCurrentRaii makeCurrentRaii{*this};
    m_batches.resetExistingMeshesAndIgnorePendingRemesh();
    m_textures.destroyAll();
    getGLFont().cleanup();
    getOpenGL().cleanup();
    m_logger.reset();
}

void MapCanvas::reportGLVersion()
{
    auto &gl = getOpenGL();
    auto logMsg = [this](const QByteArray &prefix, const QByteArray &msg) -> void {
        qInfo() << prefix << msg;
        emit sig_log("MapCanvas", prefix + " " + msg);
    };
    auto getString = [&gl](const GLenum name) -> QByteArray { return QByteArray{gl.glGetString(name)}; };
    auto logString = [&getString, &logMsg](const QByteArray &prefix, const GLenum name) -> void { logMsg(prefix, getString(name)); };

    logString("OpenGL Version:", GL_VERSION);
    logString("OpenGL Renderer:", GL_RENDERER);
    logString("OpenGL Vendor:", GL_VENDOR);
    logString("OpenGL GLSL:", GL_SHADING_LANGUAGE_VERSION);

    const auto version = [this]() -> std::string {
        const QSurfaceFormat &format = context()->format();
        std::ostringstream oss;
        switch (format.renderableType()) {
        case QSurfaceFormat::OpenGL: oss << "GL"; break;
        case QSurfaceFormat::OpenGLES: oss << "ES"; break;
        case QSurfaceFormat::OpenVG: oss << "VG"; break;
        default: oss << "UN"; break;
        }
        oss << format.majorVersion() << "." << format.minorVersion();
        return std::move(oss).str();
    }();
    MapCanvasConfig::setCurrentOpenGLVersion(version);

    logMsg("Current OpenGL Context:", QString("%1 (%2)").arg(version.c_str()).arg(context()->isValid() ? "valid" : "invalid").toUtf8());
    logMsg("Display:", QString("%1 DPI").arg(QPaintDevice::devicePixelRatioF()).toUtf8());
}

bool MapCanvas::isBlacklistedDriver() { /* ... as original ... */ return false;}
void MapCanvas::initializeGL() { /* ... as original, ensures m_defaultVao is bound ... */
    auto &gl = getOpenGL();
    gl.initializeOpenGLFunctions();
    reportGLVersion();
    if (isBlacklistedDriver()) { /* ... */ }
    initLogger();
    gl.initializeRenderer(static_cast<float>(QPaintDevice::devicePixelRatioF()));
    updateMultisampling();
    initTextures();
    auto &font = getGLFont();
    font.setTextureId(allocateTextureId());
    font.init();
    // Callbacks for config changes
    setConfig().canvas.showUnsavedChanges.registerChangeCallback(m_lifetime, [this]() { if (setConfig().canvas.showUnsavedChanges.get() && m_diff.highlight.has_value() && m_diff.highlight->needsUpdate.empty()) this->forceUpdateMeshes(); });
    setConfig().canvas.showMissingMapId.registerChangeCallback(m_lifetime, [this]() { if (setConfig().canvas.showMissingMapId.get() && m_diff.highlight.has_value() && m_diff.highlight->needsUpdate.empty()) this->forceUpdateMeshes(); });
    setConfig().canvas.showUnmappedExits.registerChangeCallback(m_lifetime, [this]() { this->forceUpdateMeshes(); });

    QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();
    f->glGenVertexArrays(1, &m_defaultVao);
    f->glBindVertexArray(m_defaultVao);
}
void MapCanvas::slot_onMessageLoggedDirect(const QOpenGLDebugMessage &message) { /* ... as original ... */ }
void MapCanvas::initLogger() { /* ... as original ... */ }
glm::mat4 MapCanvas::getViewProj_old(const glm::vec2 &scrollPos, const glm::ivec2 &size, const float zoomScale, const int currentLayer) { /* ... as original ... */
    constexpr float FIXED_VIEW_DISTANCE = 60.f;
    constexpr float ROOM_Z_SCALE = 7.f;
    constexpr auto baseSize = static_cast<float>(BASESIZE);
    const float swp = zoomScale * baseSize / static_cast<float>(size.x);
    const float shp = zoomScale * baseSize / static_cast<float>(size.y);
    QMatrix4x4 proj;
    proj.frustum(-0.5f, +0.5f, -0.5f, 0.5f, 5.f, 10000.f);
    proj.scale(swp, shp, 1.f);
    proj.translate(-scrollPos.x, -scrollPos.y, -FIXED_VIEW_DISTANCE);
    proj.scale(1.f, 1.f, ROOM_Z_SCALE);
    return glm::make_mat4(proj.constData());
}

NODISCARD static float getPitchDegrees(const float zoomScale) { /* ... as original ... */
    const float degrees = getConfig().canvas.advanced.verticalAngle.getFloat();
    if (!MapCanvasConfig::isAutoTilt()) return degrees;
    static_assert(ScaleFactor::MAX_VALUE >= 2.f);
    return glm::smoothstep(0.5f, 2.f, zoomScale) * degrees;
}

glm::mat4 MapCanvas::getViewProj(const glm::vec2 &scrollPos, const glm::ivec2 &size, const float zoomScale, const int currentLayer) { /* ... as original ... */
    static_assert(GLM_CONFIG_CLIP_CONTROL == GLM_CLIP_CONTROL_RH_NO);
    const int width = size.x; const int height = size.y;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const auto &advanced = getConfig().canvas.advanced;
    const float fovDegrees = advanced.fov.getFloat();
    const auto pitchRadians = glm::radians(getPitchDegrees(zoomScale));
    const auto yawRadians = glm::radians(advanced.horizontalAngle.getFloat());
    const auto layerHeight = advanced.layerHeight.getFloat();
    const auto pixelScale = [aspect, fovDegrees, width]() -> float {
        constexpr float HARDCODED_LOGICAL_PIXELS = 44.f;
        const auto dummyProj = glm::perspective(glm::radians(fovDegrees), aspect, 1.f, 10.f);
        const auto centerRoomProj = glm::inverse(dummyProj) * glm::vec4(0.f, 0.f, 0.f, 1.f);
        const auto centerRoom = glm::vec3(centerRoomProj) / centerRoomProj.w;
        const auto oneRoomEast = dummyProj * glm::vec4(centerRoom + glm::vec3(1.f, 0.f, 0.f), 1.f);
        const float clipDist = std::abs(oneRoomEast.x / oneRoomEast.w);
        const float ndcDist = clipDist * 0.5f;
        const float screenDist = ndcDist * static_cast<float>(width);
        const auto pixels = std::abs(centerRoom.z) * screenDist;
        return pixels / HARDCODED_LOGICAL_PIXELS;
    }();
    const float ZSCALE = layerHeight;
    const float camDistance = pixelScale / zoomScale;
    const auto center = glm::vec3(scrollPos, static_cast<float>(currentLayer) * ZSCALE);
    const auto rotateHorizontal = glm::mat3(glm::rotate(glm::mat4(1), -yawRadians, glm::vec3(0, 0, 1)));
    const auto forward = rotateHorizontal * glm::vec3(0.f, std::sin(pitchRadians), -std::cos(pitchRadians));
    const auto right = rotateHorizontal * glm::vec3(1, 0, 0);
    const auto up = glm::cross(right, glm::normalize(forward));
    const auto eye = center - camDistance * forward;
    const auto proj = glm::perspective(glm::radians(fovDegrees), aspect, 0.25f, 1024.f);
    const auto view = glm::lookAt(eye, center, up);
    const auto scaleZ = glm::scale(glm::mat4(1), glm::vec3(1.f, 1.f, ZSCALE));
    return proj * view * scaleZ;
}

void MapCanvas::setMvp(const glm::mat4 &viewProj) { /* ... as original ... */ }
void MapCanvas::setViewportAndMvp(int width, int height) { /* ... as original ... */ }
void MapCanvas::resizeGL(int width, int height) { /* ... as original ... */ }
void MapCanvas::setAnimating(bool value) { /* ... as original ... */ }
void MapCanvas::renderLoop() { /* ... as original ... */ }
void MapCanvas::updateBatches() { /* ... as original ... */ }
void MapCanvas::updateMapBatches() { /* ... as original ... */ }
void MapCanvas::finishPendingMapBatches() { /* ... as original ... */ }
void MapCanvas::actuallyPaintGL() { /* ... as original ... */ }

NODISCARD bool MapCanvas::Diff::isUpToDate(const Map &saved, const Map &current) const { /* ... as original ... */ return highlight && highlight->saved.isSamePointer(saved) && highlight->current.isSamePointer(current); }
NODISCARD bool MapCanvas::Diff::hasRelatedDiff(const Map &saved) const { /* ... as original ... */ return highlight && highlight->saved.isSamePointer(saved); }
void MapCanvas::Diff::cancelUpdates(const Map &saved) { /* ... as original ... */ }
void MapCanvas::Diff::maybeAsyncUpdate(const Map &saved, const Map &current) { /* ... as original ... */ }

void MapCanvas::paintDifferences() { /* ... as original ... */ }
void MapCanvas::paintMap() { /* ... as original ... */ }
void MapCanvas::paintSelections() { /* ... as original ... */ }
void MapCanvas::paintGL() { /* ... as original ... */ }
void MapCanvas::paintSelectionArea() { /* ... as original ... */ }
void MapCanvas::updateMultisampling() { /* ... as original ... */ }

void MapCanvas::renderMapBatches()
{
    std::optional<MapBatches> &mapBatches = m_batches.mapBatches;
    if (!mapBatches.has_value()) {
        throw std::runtime_error("called in the wrong order");
    }

    MapBatches &batches = mapBatches.value();
    const Configuration::CanvasSettings &settings = getConfig().canvas;

    const float totalScaleFactor = getTotalScaleFactor();
    const auto wantExtraDetail = totalScaleFactor >= settings.extraDetailScaleCutoff;
    const auto wantDoorNames = settings.drawDoorNames && (totalScaleFactor >= settings.doorNameScaleCutoff);

    auto &gl = getOpenGL(); // Get reference to OpenGL object
    BatchedMeshes &batchedMeshes = batches.batchedMeshes;

    // Updated drawLayer lambda
    const auto drawLayer =
        [&gl, &batches, &batchedMeshes, wantExtraDetail, wantDoorNames](const int thisLayer, const int currentLayer) { // Added gl to capture
            const auto it_mesh = batchedMeshes.find(thisLayer);
            if (it_mesh != batchedMeshes.end()) {
                LayerMeshes &meshes = it_mesh->second;
                meshes.render(thisLayer, currentLayer, gl); // Pass gl
            }

            if (wantExtraDetail) {
                {
                    BatchedConnectionMeshes &connectionMeshes = batches.connectionMeshes;
                    const auto it_conn = connectionMeshes.find(thisLayer);
                    if (it_conn != connectionMeshes.end()) {
                        ConnectionMeshes &conn_meshes = it_conn->second; // Renamed meshes to conn_meshes
                        conn_meshes.render(thisLayer, currentLayer);
                    }
                }
                if (wantDoorNames && thisLayer == currentLayer) {
                    BatchedRoomNames &roomNameBatches = batches.roomNameBatches;
                    const auto it_name = roomNameBatches.find(thisLayer);
                    if (it_name != roomNameBatches.end()) {
                        auto &roomNameBatch = it_name->second;
                        roomNameBatch.render(GLRenderState());
                    }
                }
            }
        };

    const auto fadeBackground = [&gl, &settings]() {
        auto bgColor = Color{settings.backgroundColor.getColor(), 0.5f};
        const auto blendedWithBackground = GLRenderState().withBlend(BlendModeEnum::TRANSPARENCY).withColor(bgColor);
        gl.renderPlainFullScreenQuad(blendedWithBackground);
    };

    for (const auto &layer_pair : batchedMeshes) { // Renamed layer to layer_pair
        const int thisLayer = layer_pair.first;
        if (thisLayer == m_currentLayer) {
            gl.clearDepth();
            fadeBackground();
        }
        drawLayer(thisLayer, m_currentLayer);
    }
}
