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
#include "MapCanvasRoomDrawer.h"
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
    // Make sure the context is current and then explicitly
    // destroy all underlying OpenGL resources.
    MakeCurrentRaii makeCurrentRaii{*this}; // Assuming MakeCurrentRaii correctly handles QOpenGLWidget context

    // note: m_batchedMeshes co-owns textures created by MapCanvasData,
    // and it also owns the lifetime of some OpenGL objects (e.g. VBOs).
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

    auto getString = [&gl](const GLenum name) -> QByteArray {
        return QByteArray{gl.glGetString(name)};
    };

    auto logString = [&getString, &logMsg](const QByteArray &prefix, const GLenum name) -> void {
        logMsg(prefix, getString(name));
    };

    logString("OpenGL Version:", GL_VERSION);
    logString("OpenGL Renderer:", GL_RENDERER);
    logString("OpenGL Vendor:", GL_VENDOR);
    logString("OpenGL GLSL:", GL_SHADING_LANGUAGE_VERSION);

    const auto version = [this]() -> std::string {
        // context() is a method of QOpenGLWidget
        const QSurfaceFormat &format = this->QOpenGLWidget::context()->format();
        std::ostringstream oss;
        switch (format.renderableType()) {
        case QSurfaceFormat::OpenGL:
            oss << "GL";
            break;
        case QSurfaceFormat::OpenGLES:
            oss << "ES";
            break;
        case QSurfaceFormat::OpenVG:
            oss << "VG";
            break;
        case QSurfaceFormat::DefaultRenderableType:
        default:
            oss << "UN";
            break;
        }
        oss << format.majorVersion() << "." << format.minorVersion();
        return std::move(oss).str();
    }();

    MapCanvasConfig::setCurrentOpenGLVersion(version);

    logMsg("Current OpenGL Context:",
           QString("%1 (%2)")
               .arg(version.c_str())
               // FIXME: This is a bit late to report an invalid context.
                // context() is a method of QOpenGLWidget
               .arg(this->QOpenGLWidget::context()->isValid() ? "valid" : "invalid")
               .toUtf8());
    logMsg("Display:", QString("%1 DPI").arg(QPaintDevice::devicePixelRatioF()).toUtf8());
}

bool MapCanvas::isBlacklistedDriver()
{
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Windows) {
        auto &gl = getOpenGL();
        auto getString = [&gl](const GLenum name) -> QByteArray {
            return QByteArray{gl.glGetString(name)};
        };

        const QByteArray &vendor = getString(GL_VENDOR);
        const QByteArray &renderer = getString(GL_RENDERER);
        if (vendor == "Microsoft Corporation" && renderer == "GDI Generic") {
            return true;
        }
    }

    return false;
}

void MapCanvas::initializeGL()
{
    auto &gl = getOpenGL();
    gl.initializeOpenGLFunctions();

    reportGLVersion();

    // TODO: Perform the blacklist test as a call from main() to minimize player headache.
    if (isBlacklistedDriver()) {
        setConfig().canvas.softwareOpenGL = true;
        setConfig().write();
        this->QWidget::hide(); // QWidget method
        this->QOpenGLWidget::doneCurrent(); // QOpenGLWidget method
        QMessageBox::critical(static_cast<QWidget*>(this), // Explicit cast for QMessageBox parent
                              "OpenGL Driver Blacklisted",
                              "Please restart MMapper to enable software rendering");
        return;
    }

    // NOTE: If you're adding code that relies on generating OpenGL errors (e.g. ANGLE),
    // you *MUST* force it to complete those error probes before calling initLogger(),
    // because the logger purposely calls std::abort() when it receives an error.
    initLogger();

    gl.initializeRenderer(static_cast<float>(QPaintDevice::devicePixelRatioF()));
    updateMultisampling();

    // REVISIT: should the font texture have the lowest ID?
    initTextures();
    auto &font = getGLFont();
    font.setTextureId(allocateTextureId());
    font.init();

    setConfig().canvas.showUnsavedChanges.registerChangeCallback(m_lifetime, [this]() {
        // if (setConfig().canvas.showUnsavedChanges.get() && m_diff.highlight.has_value()
        //     && m_diff.highlight->needsUpdate.empty()) {
        //     this->forceUpdateMeshes();
        // }
        this->QWidget::update(); // QWidget::update
    });

    setConfig().canvas.showMissingMapId.registerChangeCallback(m_lifetime, [this]() {
        // if (setConfig().canvas.showMissingMapId.get() && m_diff.highlight.has_value()
        //     && m_diff.highlight->needsUpdate.empty()) {
        //     this->forceUpdateMeshes();
        // }
        this->QWidget::update(); // QWidget::update
    });

    setConfig().canvas.showUnmappedExits.registerChangeCallback(m_lifetime, [this]() {
        this->forceUpdateMeshes();
    });

    // context() is QOpenGLWidget method
    QOpenGLExtraFunctions *f = this->QOpenGLWidget::context()->extraFunctions();
    f->glGenVertexArrays(1, &m_defaultVao);
    f->glBindVertexArray(m_defaultVao);

    // Calculate initial calibrated line width // Removed
    // this->updateCalibratedWorldLineWidth(); // Removed

    // Register callback for FoV changes // Removed
    // getConfig().canvas.advanced.fov.registerChangeCallback(m_lifetime, [this]() {
    //     this->updateCalibratedWorldLineWidth();
    //     this->update(); // Or this->forceUpdateMeshes(); if more direct update is needed
    // });
}

// Removed MapCanvas::updateCalibratedWorldLineWidth() method definition
/*
void MapCanvas::updateCalibratedWorldLineWidth() {
    constexpr float TARGET_LOGICAL_WIDTH = 2.0f;
    // BASESIZE is defined in mapcanvas.h as 1024
    constexpr float BASESIZE_LOGICAL_PIXELS = static_cast<float>(MapCanvas::BASESIZE);
    constexpr float Z_REF = 60.0f; // From FIXED_VIEW_DISTANCE in old getViewProj

    float dpr = getOpenGL().getDevicePixelRatio();
    if (dpr <= 0.0f) dpr = 1.0f; // Safety

    float targetPhysicalPixelWidth = TARGET_LOGICAL_WIDTH * dpr;
    float basesizePhysicalPixelH = BASESIZE_LOGICAL_PIXELS * dpr;
    if (basesizePhysicalPixelH == 0.0f) basesizePhysicalPixelH = 1.0f; // Avoid division by zero

    float fovYRad = glm::radians(getConfig().canvas.advanced.fov.getFloat());
    if (fovYRad < 0.01f) fovYRad = 0.01f; // Prevent too small FoV
    if (fovYRad > glm::pi<float>() - 0.01f) fovYRad = glm::pi<float>() - 0.01f; // Prevent too large FoV

    float tanFovDiv2 = tan(fovYRad / 2.0f);
    if (std::abs(tanFovDiv2) < 1e-6) { // if tan is very close to zero (FoV near 0)
         m_calibratedWorldHalfLineWidth = 0.01f; // a small default world width
         // Optionally log a warning: qWarning() << "FoV is near zero, using default line width";
         return;
    }
    if (std::isinf(tanFovDiv2) || std::isnan(tanFovDiv2)) { // if FoV is pi or problematic
         m_calibratedWorldHalfLineWidth = 0.01f; // a small default world width
         // Optionally log a warning: qWarning() << "FoV is problematic (e.g., PI), using default line width";
         return;
    }

    // worldUnitsPerPhysicalPixelYAtRef calculation:
    // Half height of view frustum at Z_REF = Z_REF * tan(fovYRad / 2.0f)
    // Total height = 2.0f * Z_REF * tan(fovYRad / 2.0f)
    // This total height corresponds to basesizePhysicalPixelH pixels on screen.
    // So, worldUnitsPerPhysicalPixelYAtRef = (Total height in world units) / (Total height in physical pixels)
    float worldUnitsPerPhysicalPixelYAtRef = (2.0f * Z_REF * tanFovDiv2) / basesizePhysicalPixelH;

    // Check for division by zero if basesizePhysicalPixelH was zero (already handled) or tanFovDiv2 is zero.
    if (basesizePhysicalPixelH == 0.0f) { // tanFovDiv2 already checked
         m_calibratedWorldHalfLineWidth = 0.01f; // fallback
         return;
    }

    m_calibratedWorldHalfLineWidth = (targetPhysicalPixelWidth / 2.0f) * worldUnitsPerPhysicalPixelYAtRef;

    // Sanity clamps
    if (m_calibratedWorldHalfLineWidth < 0.001f) m_calibratedWorldHalfLineWidth = 0.001f;
    if (m_calibratedWorldHalfLineWidth > 0.5f) {
        // qWarning() << "Calibrated world half line width is very large:" << m_calibratedWorldHalfLineWidth;
        m_calibratedWorldHalfLineWidth = 0.5f;
    }
}
*/

/* Direct means it is always called from the emitter's thread */
void MapCanvas::slot_onMessageLoggedDirect(const QOpenGLDebugMessage &message)
{
    using Type = QOpenGLDebugMessage::Type;
    switch (message.type()) {
    case Type::InvalidType:
    case Type::ErrorType:
    case Type::UndefinedBehaviorType:
        break;
    case Type::DeprecatedBehaviorType:
    case Type::PortabilityType:
    case Type::PerformanceType:
    case Type::OtherType:
    case Type::MarkerType:
    case Type::GroupPushType:
    case Type::GroupPopType:
    case Type::AnyType:
        qWarning() << message;
        return;
    }

    qCritical() << message;

    QMessageBox box;
    box.setWindowTitle("Fatal OpenGL error");
    box.setText(message.message());
    box.exec();

    std::abort();
}

void MapCanvas::initLogger()
{
    m_logger = std::make_unique<QOpenGLDebugLogger>(this);
    connect(m_logger.get(),
            &QOpenGLDebugLogger::messageLogged,
            this,
            &MapCanvas::slot_onMessageLoggedDirect,
            Qt::DirectConnection /* NOTE: executed in emitter's thread */);

    if (!m_logger->initialize()) {
        m_logger.reset();
        qWarning() << "Failed to initialize OpenGL debug logger";
        return;
    }

    m_logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
    m_logger->disableMessages();
    m_logger->enableMessages(QOpenGLDebugMessage::AnySource,
                             (QOpenGLDebugMessage::ErrorType
                              | QOpenGLDebugMessage::UndefinedBehaviorType),
                             QOpenGLDebugMessage::AnySeverity);
}

glm::mat4 MapCanvas::getViewProj_old(const glm::vec2 &scrollPos,
                                     const glm::ivec2 &size,
                                     const float zoomScale,
                                     const int /*currentLayer*/)
{
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

NODISCARD static float getPitchDegrees(const float zoomScale)
{
    const float degrees = getConfig().canvas.advanced.verticalAngle.getFloat();
    if (!MapCanvasConfig::isAutoTilt()) {
        return degrees;
    }

    static_assert(ScaleFactor::MAX_VALUE >= 2.f);
    return glm::smoothstep(0.5f, 2.f, zoomScale) * degrees;
}

glm::mat4 MapCanvas::getViewProj(const glm::vec2 &scrollPos,
                                 const glm::ivec2 &size,
                                 const float zoomScale,
                                 const int currentLayer)
{
    static_assert(GLM_CONFIG_CLIP_CONTROL == GLM_CLIP_CONTROL_RH_NO);

    const int width = size.x;
    const int height = size.y;

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

        // Use east instead of north, so that tilted perspective matches horizontally.
        const auto oneRoomEast = dummyProj * glm::vec4(centerRoom + glm::vec3(1.f, 0.f, 0.f), 1.f);
        const float clipDist = std::abs(oneRoomEast.x / oneRoomEast.w);
        const float ndcDist = clipDist * 0.5f;

        // width is in logical pixels
        const float screenDist = ndcDist * static_cast<float>(width);
        const auto pixels = std::abs(centerRoom.z) * screenDist;
        return pixels / HARDCODED_LOGICAL_PIXELS;
    }();

    const float ZSCALE = layerHeight;
    const float camDistance = pixelScale / zoomScale;
    const auto center = glm::vec3(scrollPos, static_cast<float>(currentLayer) * ZSCALE);

    // The view matrix will transform from world space to eye-space.
    // Eye space has the camera at the origin, with +X right, +Y up, and -Z forward.
    //
    // Our camera's orientation is based on the world-space ENU coordinates.
    // We'll define right-handed basis vectors forward, right, and up.

    // The horizontal rotation in the XY plane will affect both forward and right vectors.
    // Currently the convention is: -45 is northwest, and +45 is northeast.
    //
    // If you want to modify this, keep in mind that the angle is inverted since the
    // camera is subtracted from the center, so the result is that positive angle
    // appears clockwise (backwards) on screen.
    const auto rotateHorizontal = glm::mat3(
        glm::rotate(glm::mat4(1), -yawRadians, glm::vec3(0, 0, 1)));

    // Our unrotated pitch is defined so that 0 is straight down, and 90 degrees is north,
    // but the yaw rotation can cause it to point northeast or northwest.
    //
    // Here we use an ENU coordinate system, so we have:
    //   forward(pitch= 0 degrees) = -Z (down), and
    //   forward(pitch=90 degrees) = +Y (north).
    const auto forward = rotateHorizontal
                         * glm::vec3(0.f, std::sin(pitchRadians), -std::cos(pitchRadians));
    // Unrotated right is east (+X).
    const auto right = rotateHorizontal * glm::vec3(1, 0, 0);
    // right x forward = up
    const auto up = glm::cross(right, glm::normalize(forward));

    // Subtract because camera looks at the center.
    const auto eye = center - camDistance * forward;

    // NOTE: may need to modify near and far planes by pixelScale and zoomScale.
    // Be aware that a 24-bit depth buffer only gives about 12 bits of usable
    // depth range; we may need to reduce this for people with 16-bit depth buffers.
    // Keep in mind: Arda is about 600x300 rooms, so viewing the blue mountains
    // from mordor requires approx 700 room units of view distance.
    const auto proj = glm::perspective(glm::radians(fovDegrees), aspect, 0.25f, 1024.f);
    const auto view = glm::lookAt(eye, center, up);
    const auto scaleZ = glm::scale(glm::mat4(1), glm::vec3(1.f, 1.f, ZSCALE));

    return proj * view * scaleZ;
}

void MapCanvas::setMvp(const glm::mat4 &viewProj)
{
    auto &gl = getOpenGL();
    m_viewProj = viewProj;
    gl.setProjectionMatrix(m_viewProj);
}

void MapCanvas::setViewportAndMvp(int width, int height)
{
    const bool want3D = getConfig().canvas.advanced.use3D.get();

    auto &gl = getOpenGL();

    gl.glViewport(0, 0, width, height);
    const auto size = this->MapCanvasViewport::getViewport().size;
    assert(size.x == width);
    assert(size.y == height);

    const float zoomScale = this->MapCanvasViewport::getTotalScaleFactor();
    const auto viewProj = (!want3D) ? getViewProj_old(m_scroll, size, zoomScale, m_currentLayer)
                                    : getViewProj(m_scroll, size, zoomScale, m_currentLayer);
    setMvp(viewProj);
    this->MapCanvasViewport::updateFrustumPlanes();
    launchVisibilityUpdateTask();
}

void MapCanvas::resizeGL(int width, int height)
{
    if (m_textures.room_modified == nullptr) {
        // resizeGL called but initializeGL was not called yet
        return;
    }

    setViewportAndMvp(width, height); // Ensure projection is set before unprojecting
    // updateVisibleChunks(); // Calls launchVisibilityUpdateTask
    // requestMissingChunks(); // Handled by async flow
    launchVisibilityUpdateTask(); // Explicitly trigger for resize too

    // Render
    this->QWidget::update(); // QWidget method
}

void MapCanvas::setAnimating(bool value)
{
    if (m_frameRateController.animating == value)
        return;

    m_frameRateController.animating = value;

    if (m_frameRateController.animating) {
        QTimer::singleShot(0, this, &MapCanvas::renderLoop);
    }
}

void MapCanvas::renderLoop()
{
    if (!m_frameRateController.animating) {
        return;
    }

    // REVISIT: Make this configurable later when its not just used for the remesh flash
    static constexpr int TARGET_FRAMES_PER_SECOND = 20;
    auto targetFrameTime = std::chrono::milliseconds(1000 / TARGET_FRAMES_PER_SECOND);

    auto now = std::chrono::steady_clock::now();
    this->QWidget::update(); // QWidget method
    auto afterPaint = std::chrono::steady_clock::now();

    // Render the next frame at the appropriate time or now if we're behind
    auto timeSinceLastFrame = std::chrono::duration_cast<std::chrono::milliseconds>(afterPaint
                                                                                    - now);
    auto delay = std::max(targetFrameTime - timeSinceLastFrame, std::chrono::milliseconds::zero());

    QTimer::singleShot(delay.count(), this, &MapCanvas::renderLoop);

    m_frameRateController.lastFrameTime = now;
}

void MapCanvas::updateBatches()
{
    updateMapBatches();
    updateInfomarkBatches();
}

void MapCanvas::updateMapBatches()
{
    RemeshCookie &remeshCookie = m_batches.remeshCookie;
    if (remeshCookie.isPending()) {
        return;
    }

    if (m_batches.mapBatches.has_value() && !m_data.getNeedsMapUpdate()) {
        return;
    }

    if (m_data.getNeedsMapUpdate()) {
        m_data.clearNeedsMapUpdate();
        assert(!m_data.getNeedsMapUpdate());
        MMLOG() << "[updateMapBatches] cleared 'needsUpdate' flag";
    }

    auto getFuture = [this]() {
        MMLOG() << "[updateMapBatches] calling generateBatches";
        return m_data.generateBatches(mctp::getProxy(m_textures));
    };

    remeshCookie.set(getFuture());
    assert(remeshCookie.isPending());

    m_diff.cancelUpdates(m_data.getSavedMap());
}

void MapCanvas::finishPendingMapBatches()
{
    std::string_view prefix = "[finishPendingMapBatches] ";

#define LOG() MMLOG() << prefix

    RemeshCookie &remeshCookie = m_batches.remeshCookie;
    if (!remeshCookie.isPending()) {
        return;
    } else if (!remeshCookie.isReady()) {
        return;
    }

    LOG() << "Waiting for the cookie. This shouldn't take long.";
    SharedMapBatchFinisher pFuture = remeshCookie.get();
    assert(!remeshCookie.isPending());

    setAnimating(false);

    if (pFuture == nullptr) {
        // REVISIT: Do we need to schedule another update now?
        LOG() << "Got NULL (means the update was flagged to be ignored)";
        return;
    }

    // REVISIT: should we pass a "fake" one and only swap to the correct one on success?
    LOG() << "Clearing the map batches and call the finisher to create new ones";

    DECL_TIMER(t, __FUNCTION__);
    const IMapBatchesFinisher &finisher_obj = *pFuture; // Renamed 'future' to 'finisher_obj' to avoid conflict

    std::optional<MapBatches> temporaryNewBatches_opt;
    // The global 'finish' function (from MapCanvasRoomDrawer.h) will emplace into temporaryNewBatches_opt
    // It calls finisher_obj.virt_finish(MapBatches &output, OpenGL &gl, GLFont &font)
    finish(finisher_obj, temporaryNewBatches_opt, getOpenGL(), getGLFont());

    if (temporaryNewBatches_opt.has_value()) {
        LOG() << "Successfully finished specific chunk batches into temporary MapBatches.";
        MapBatches& temporaryNewBatches = *temporaryNewBatches_opt;

        if (!m_batches.mapBatches.has_value()) {
            // This is the first batch of data, or main batches were previously cleared.
            LOG() << "Main mapBatches is empty, moving temporaryNewBatches.";
            m_batches.mapBatches.emplace(std::move(temporaryNewBatches));
            // Clear pending flags for all chunks that were just loaded by iterating the moved content
            if (m_batches.mapBatches.has_value()) {
                for (const auto& layer_pair : m_batches.mapBatches->batchedMeshes) {
                    for (const auto& chunk_pair : layer_pair.second) {
                        if (m_pendingChunkGenerations.erase({layer_pair.first, chunk_pair.first})) {
                            // LOG() << "Erased " << layer_pair.first << ":" << chunk_pair.first << " from pending (initial load).";
                        }
                    }
                }
            }
        } else {
            // Merge temporaryNewBatches into existing m_batches.mapBatches
            LOG() << "Merging temporaryNewBatches into existing main mapBatches.";
            MapBatches& mainMapBatches = *m_batches.mapBatches;

            for (auto& layer_pair : temporaryNewBatches.batchedMeshes) {
                int layerId = layer_pair.first;
                auto& new_chunks_in_layer = layer_pair.second; // Use auto& for non-const access
                for (auto& chunk_pair : new_chunks_in_layer) {
                        RoomAreaHash roomAreaHash = chunk_pair.first;
                        mainMapBatches.batchedMeshes[layerId].insert_or_assign(roomAreaHash, std::move(chunk_pair.second));
                        if (m_pendingChunkGenerations.erase({layerId, roomAreaHash})) {
                            // LOG() << "Erased " << layerId << ":" << roomAreaHash << " from pending (merge).";
                    }
                }
            }

            for (auto& layer_pair : temporaryNewBatches.connectionMeshes) {
                int layerId = layer_pair.first;
                auto& new_connection_chunks_in_layer = layer_pair.second; // Use auto& for non-const access
                for (auto& chunk_pair : new_connection_chunks_in_layer) {
                        RoomAreaHash roomAreaHash = chunk_pair.first;
                        mainMapBatches.connectionMeshes[layerId].insert_or_assign(roomAreaHash, std::move(chunk_pair.second));
                }
            }

            for (auto& layer_pair : temporaryNewBatches.roomNameBatches) {
                int layerId = layer_pair.first;
                auto& new_roomname_chunks_in_layer = layer_pair.second; // Use auto& for non-const access
                for (auto& chunk_pair : new_roomname_chunks_in_layer) {
                        RoomAreaHash roomAreaHash = chunk_pair.first;
                        mainMapBatches.roomNameBatches[layerId].insert_or_assign(roomAreaHash, std::move(chunk_pair.second));
                }
            }
        }
    } else {
        LOG() << "::finish with pFuture did not populate temporaryNewBatches_opt. This should not happen if pFuture is valid.";
        // If finish fails to emplace, specific chunks associated with pFuture remain pending.
        // No specific action needed here for m_pendingChunkGenerations, requestMissingChunks will handle retries.
    }
    // The broad m_pendingChunkGenerations.clear() that was here is removed as per subtask.
    // Chunks are erased individually above as they are processed.
    // If pFuture itself was null, m_pendingChunkGenerations is cleared in the block above for that case.
    // The original logic for pFuture == nullptr also cleared m_pendingChunkGenerations,
    // which is covered by clearing it after remeshCookie.get() if isReady() was true.
    // The update() call is also important.
    this->QWidget::update();


#undef LOG
}

void MapCanvas::actuallyPaintGL()
{
    // DECL_TIMER(t, __FUNCTION__);
    // width() and height() are QWidget methods, wrapped in mapcanvas.h to QOpenGLWidget::width/height
    setViewportAndMvp(this->QOpenGLWidget::width(), this->QOpenGLWidget::height());

    auto &gl = getOpenGL();
    gl.clear(Color{getConfig().canvas.backgroundColor});

    if (m_data.isEmpty()) {
        getGLFont().renderTextCentered("No map loaded");
        return;
    }

    paintMap();
    paintBatchedInfomarks();
    paintSelections();
    paintCharacters();
    paintDifferences();
}

NODISCARD bool MapCanvas::Diff::isUpToDate(const Map &saved, const Map &current) const
{
    return highlight && highlight->saved.isSamePointer(saved)
           && highlight->current.isSamePointer(current);
}

// this differs from isUpToDate in that it allows display of a diff based on the current saved map,
// but it allows the "current" to be different (e.g. during the async remesh for the current map).
NODISCARD bool MapCanvas::Diff::hasRelatedDiff(const Map &saved) const
{
    return highlight && highlight->saved.isSamePointer(saved);
}

void MapCanvas::Diff::cancelUpdates(const Map &saved)
{
    futureHighlight.reset();
    if (highlight) {
        if (!hasRelatedDiff(saved)) {
            highlight.reset();
        }
    }
}

void MapCanvas::Diff::maybeAsyncUpdate(const Map &saved, const Map &current)
{
    auto &diff = *this;

    // Pending takes precedence. This also usually guarantees at most one pending update at a time,
    // but calling resetExistingMeshesAndIgnorePendingRemesh() could result in more than one diff
    // mesh thread executing concurrently, where the old one will be ignored.
    if (diff.futureHighlight) {
        constexpr auto immediate = std::chrono::milliseconds(0);
        if (diff.futureHighlight->wait_for(immediate) != std::future_status::timeout) {
            try {
                diff.highlight = diff.futureHighlight->get();
            } catch (const std::exception &ex) {
                MMLOG_ERROR() << "Exception: " << ex.what();
            }
            diff.futureHighlight.reset();
        }
        return;
    }

    // no change necessary
    if (isUpToDate(saved, current)) {
        return;
    }

    const bool showNeedsServerId = getConfig().canvas.showMissingMapId.get();
    const bool showChanged = getConfig().canvas.showUnsavedChanges.get();

    diff.futureHighlight = std::async(
        std::launch::async,
        [saved, current, showNeedsServerId, showChanged]() -> Diff::HighlightDiff {
            DECL_TIMER(t2, "[async] actuallyPaintGL: highlight differences and needs update");
            // 3-2
            // |/|
            // 0-1
            static constexpr std::array<glm::vec3, 4> corners{
                glm::vec3{0, 0, 0},
                glm::vec3{1, 0, 0},
                glm::vec3{1, 1, 0},
                glm::vec3{0, 1, 0},
            };

            // REVISIT: Just send the position and convert from point to quad in a shader?
            auto getChanged = [&saved, &current, showChanged]() -> TexVertVector {
                if (!showChanged) {
                    return TexVertVector{};
                }
                DECL_TIMER(t3, "[async] actuallyPaintGL: compute differences");
                TexVertVector changed;
                auto drawQuad = [&changed](const RawRoom &room) {
                    const auto &pos = room.getPosition().to_vec3();
                    for (auto &corner : corners) {
                        changed.emplace_back(corner, pos + corner);
                    }
                };

                ProgressCounter dummyPc;
                Map::foreachChangedRoom(dummyPc, saved, current, drawQuad);
                return changed;
            };

            auto getNeedsUpdate = [&current, showNeedsServerId]() -> TexVertVector {
                if (!showNeedsServerId) {
                    return TexVertVector{};
                }
                DECL_TIMER(t3, "[async] actuallyPaintGL: compute needs update");

                TexVertVector needsUpdate;
                auto drawQuad = [&needsUpdate](const RoomHandle &h) {
                    const auto &pos = h.getPosition().to_vec3();
                    for (auto &corner : corners) {
                        needsUpdate.emplace_back(corner, pos + corner);
                    }
                };
                for (auto id : current.getRooms()) {
                    if (auto h = current.getRoomHandle(id)) {
                        if (h.getServerId() == INVALID_SERVER_ROOMID) {
                            drawQuad(h);
                        }
                    }
                }
                return needsUpdate;
            };

            return Diff::HighlightDiff{saved, current, getNeedsUpdate(), getChanged()};
        });
}

void MapCanvas::paintDifferences()
{
    auto &diff = m_diff;
    const auto &saved = m_data.getSavedMap();
    const auto &current = m_data.getCurrentMap();

    diff.maybeAsyncUpdate(saved, current);
    if (!diff.hasRelatedDiff(saved)) {
        return;
    }

    const auto &highlight = deref(diff.highlight);
    auto &gl = getOpenGL();

    auto tryRenderWithTexture = [&gl](const TexVertVector &points, const MMTextureId texid) {
        if (points.empty()) {
            return;
        }
        gl.renderTexturedQuads(points,
                               GLRenderState()
                                   .withColor(Colors::white)
                                   .withBlend(BlendModeEnum::TRANSPARENCY)
                                   .withTexture0(texid));
    };

    if (getConfig().canvas.showMissingMapId.get()) {
        tryRenderWithTexture(highlight.needsUpdate, m_textures.room_needs_update->getId());
    }
    tryRenderWithTexture(highlight.diff, m_textures.room_modified->getId());
}

void MapCanvas::paintMap()
{
    const bool pending = m_batches.remeshCookie.isPending();
    if (pending) {
        setAnimating(true);
    }

    if (!m_batches.mapBatches.has_value()) {
        const QString msg = pending ? "Please wait... the map isn't ready yet." : "Batch error";
        getGLFont().renderTextCentered(msg);
        if (!pending) {
            // REVISIT: does this need a better fix?
            // pending already scheduled an update, but now we realize we need an update.
            this->QWidget::update();
        }
        return;
    }

    // TODO: add a GUI indicator for pending update?
    renderMapBatches();

    if (pending) {
        if (m_batches.pendingUpdateFlashState.tick()) {
            const QString msg = "CAUTION: Async map update pending!";
            getGLFont().renderTextCentered(msg);
        }
    }
}

void MapCanvas::paintSelections()
{
    paintSelectedRooms();
    paintSelectedConnection();
    paintSelectionArea();
    paintSelectedInfoMarks();
}

void MapCanvas::paintGL()
{
    checkAndProcessVisibilityResult();
    static thread_local double longestBatchMs = 0.0;

    const bool showPerfStats = MapCanvasConfig::getShowPerfStats();

    using Clock = std::chrono::high_resolution_clock;
    std::optional<Clock::time_point> optStart;
    std::optional<Clock::time_point> optAfterTextures;
    std::optional<Clock::time_point> optAfterBatches;
    if (showPerfStats) {
        optStart = Clock::now();
    }

    {
        updateMultisampling();
        updateTextures();
        if (showPerfStats) {
            optAfterTextures = Clock::now();
        }

        // Note: The real work happens here!
        updateBatches();

        // And here
        finishPendingMapBatches();

        // For accurate timing of the update, we'd need to call glFinish(),
        // or at least set up an OpenGL query object. The update will send
        // a lot of data to the GPU, so it could take a while...
        if (showPerfStats) {
            optAfterBatches = Clock::now();
        }

        actuallyPaintGL();
    }

    if (!showPerfStats) {
        return; /* don't wait to finish */
    }

    const auto &start = optStart.value();
    const auto &afterTextures = optAfterTextures.value();
    const auto &afterBatches = optAfterBatches.value();
    const auto afterPaint = Clock::now();
    const bool calledFinish = [this]() -> bool {
        // context() is a QOpenGLWidget method
        if (auto *const ctxt = this->QOpenGLWidget::context()) {
            if (auto *const func = ctxt->functions()) {
                func->glFinish();
                return true;
            }
        }
        return false;
    }();

    const auto end = Clock::now();

    const auto ms = [](auto delta) -> double {
        return double(std::chrono::duration_cast<std::chrono::nanoseconds>(delta).count()) * 1e-6;
    };

    const auto w = this->QOpenGLWidget::width();
    const auto h = this->QOpenGLWidget::height();
    const auto dpr = getOpenGL().getDevicePixelRatio();

    auto &font = getGLFont();
    std::vector<GLText> text;

    const auto lineHeight = font.getFontHeight();
    const float rightMargin = float(w) * dpr
                              - static_cast<float>(font.getGlyphAdvance('e').value_or(5));

    // x and y are in physical (device) pixels
    // TODO: change API to use logical pixels.
    auto y = lineHeight;
    const auto print = [lineHeight, rightMargin, &text, &y](const QString &msg) {
        text.emplace_back(glm::vec3(rightMargin, y, 0),
                          mmqt::toStdStringLatin1(msg), // GL font is latin1
                          Colors::white,
                          Colors::black.withAlpha(0.4f),
                          FontFormatFlags{FontFormatFlagEnum::HALIGN_RIGHT});
        y += lineHeight;
    };

    const auto texturesTime = ms(afterTextures - start);
    const auto batchTime = ms(afterBatches - afterTextures);

    const auto total = ms(end - start);
    print(QString::asprintf(
        "%.1f (updateTextures) + %.1f (updateBatches) + %.1f (paintGL) + %.1f (glFinish%s) = %.1f ms",
        texturesTime,
        batchTime,
        ms(afterPaint - afterBatches),
        ms(end - afterPaint),
        calledFinish ? "" : "*",
        total));

    if (!calledFinish) {
        print("* = unable to call glFinish()");
    }

    longestBatchMs = std::max(batchTime, longestBatchMs);
    print(QString::asprintf("Worst updateBatches: %.1f ms", longestBatchMs));

    const auto &advanced = getConfig().canvas.advanced;
    const float zoom = getTotalScaleFactor();
    const bool is3d = advanced.use3D.get();
    if (is3d) {
        print(QString::asprintf("3d mode: %.1f fovy, %.1f pitch, %.1f yaw, %.1f zscale",
                                advanced.fov.getDouble(),
                                static_cast<double>(getPitchDegrees(zoom)),
                                advanced.horizontalAngle.getDouble(),
                                advanced.layerHeight.getDouble()));
    } else {
        const glm::vec3 c = unproject_raw(glm::vec3{w / 2, h / 2, 0});
        const glm::vec3 v = unproject_raw(glm::vec3{w / 2, 0, 0});
        const auto dy = std::abs((v - c).y);
        const auto dz = std::abs(c.z);
        const float fovy = 2.f * glm::degrees(std::atan2(dy, dz));
        print(QString::asprintf("2d mode; current fovy: %.1f", static_cast<double>(fovy)));
    }

    print(QString::asprintf("zoom: %.2f (1/%.1f)",
                            static_cast<double>(zoom),
                            1.0 / static_cast<double>(zoom)));

    const auto ctr = m_mapScreen.getCenter();
    print(QString::asprintf("center: %.1f, %.1f, %.1f",
                            static_cast<double>(ctr.x),
                            static_cast<double>(ctr.y),
                            static_cast<double>(ctr.z)));

    font.render2dTextImmediate(text);
}

void MapCanvas::paintSelectionArea()
{
    if (!hasSel1() || !hasSel2()) {
        return;
    }

    const auto pos1 = getSel1().pos.to_vec2();
    const auto pos2 = getSel2().pos.to_vec2();

    // Mouse selected area
    auto &gl = getOpenGL();
    const auto layer = static_cast<float>(m_currentLayer);

    if (m_selectedArea) {
        const glm::vec3 A{pos1, layer};
        const glm::vec3 B{pos2.x, pos1.y, layer};
        const glm::vec3 C{pos2, layer};
        const glm::vec3 D{pos1.x, pos2.y, layer};

        // REVISIT: why a dark colored selection?
        const Color selBgColor = Colors::black.withAlpha(0.5f);
        const auto rs
            = GLRenderState().withBlend(BlendModeEnum::TRANSPARENCY).withDepthFunction(std::nullopt);

        {
            const std::vector<glm::vec3> verts{A, B, C, D};
            const auto &fillStyle = rs;
            gl.renderPlainQuads(verts, fillStyle.withColor(selBgColor));
        }

        const auto selFgColor = Colors::yellow;
        {
            static constexpr float SELECTION_AREA_LINE_WIDTH = 2.f;
            const auto lineStyle = rs.withLineParams(LineParams{SELECTION_AREA_LINE_WIDTH});
            const std::vector<glm::vec3> verts{A, B, B, C, C, D, D, A};

            // FIXME: ASAN flags this as out-of-bounds memory access inside an assertion
            //
            //     Q_ASSERT(QOpenGLFunctions::isInitialized(d_ptr));
            //
            // in QOpenGLFunctions::glDrawArrays(). However, it works without ASAN,
            // so maybe the problem is in my OpenGL driver?
            //
            // "OpenGL Version:" "3.1 Mesa 20.2.6"
            // "OpenGL Renderer:" "llvmpipe (LLVM 11.0.0, 256 bits)"
            // "OpenGL Vendor:" "Mesa/X.org"
            // "OpenGL GLSL:" "1.40"
            // "Current OpenGL Context:" "3.1 (valid)"
            //
            gl.renderPlainLines(verts, lineStyle.withColor(selFgColor));
        }
    }

    paintNewInfomarkSelection();
}

void MapCanvas::updateMultisampling()
{
    const int wantMultisampling = getConfig().canvas.antialiasingSamples;
    std::optional<int> &activeStatus = m_graphicsOptionsStatus.multisampling;
    if (activeStatus == wantMultisampling) {
        return;
    }

    // REVISIT: check return value?
    MAYBE_UNUSED const bool enabled = getOpenGL().tryEnableMultisampling(wantMultisampling);
    activeStatus = wantMultisampling;
}

void MapCanvas::renderMapBatches()
{
    std::optional<MapBatches> &mapBatches = m_batches.mapBatches;
    if (!mapBatches.has_value()) {
        // Hint: Use CREATE_ONLY first.
        throw std::runtime_error("called in the wrong order");
    }

    MapBatches &batches = mapBatches.value();
    const Configuration::CanvasSettings &settings = getConfig().canvas;

    const float totalScaleFactor = getTotalScaleFactor();
    const auto wantExtraDetail = totalScaleFactor >= settings.extraDetailScaleCutoff;
    const auto wantDoorNames = settings.drawDoorNames
                               && (totalScaleFactor >= settings.doorNameScaleCutoff);

    auto &gl = getOpenGL();
    // batches.batchedMeshes is std::map<int, ChunkedLayerMeshes> (main terrain, etc.)
    // batches.connectionMeshes is std::map<int, std::map<RoomAreaHash, ConnectionMeshes>>
    // batches.roomNameBatches is std::map<int, std::map<RoomAreaHash, UniqueMesh>>
    const auto& allChunkedLayerMeshes = batches.batchedMeshes;

    const auto fadeBackground = [&gl, &settings]() {
        auto bgColor = Color{settings.backgroundColor.getColor(), 0.5f};
        const auto blendedWithBackground = GLRenderState().withBlend(BlendModeEnum::TRANSPARENCY).withColor(bgColor);
        gl.renderPlainFullScreenQuad(blendedWithBackground);
    };

    bool currentLayerHasBeenSetup = false;

    // Iterate through layers that have LayerMeshes (terrain, etc.)
    // We use this as the primary loop for determining which layers to process.
    for (const auto& layerMeshesPair : allChunkedLayerMeshes) {
        const int layerId = layerMeshesPair.first;
        const ChunkedLayerMeshes& terrainChunksInLayer = layerMeshesPair.second; // Map of RoomAreaHash -> LayerMeshes

        if (std::abs(layerId - m_currentLayer) > getConfig().canvas.mapRadius[2]) {
            continue;
        }

        // Setup for current layer (clear depth, fade background)
        if (layerId == m_currentLayer && !currentLayerHasBeenSetup) {
            gl.clearDepth();
            fadeBackground();
            currentLayerHasBeenSetup = true;
        }

        auto itVisibleLayer = m_visibleChunks.find(layerId);
        if (itVisibleLayer == m_visibleChunks.end() || itVisibleLayer->second.empty()) {
            // This layer isn't in m_visibleChunks or has no visible chunks calculated for it.
            continue;
        }
        const std::set<RoomAreaHash>& visibleChunkIdsForLayer = itVisibleLayer->second;

        bool fontShaderWasBound = false;
        GLRenderState nameRenderState; // Define once per layer
        nameRenderState = GLRenderState()
                              .withBlend(BlendModeEnum::TRANSPARENCY)
                              .withDepthFunction(DepthFunctionEnum::LEQUAL);

        // Check if any room names need rendering for this layer to bind shader once
        auto itLayerRoomNames = batches.roomNameBatches.find(layerId);
        if (wantDoorNames && layerId == m_currentLayer && itLayerRoomNames != batches.roomNameBatches.end()) {
            const auto& chunkedRoomNamesForLayer = itLayerRoomNames->second;
            for (const RoomAreaHash roomAreaHash : visibleChunkIdsForLayer) {
                if (chunkedRoomNamesForLayer.count(roomAreaHash)) {
                    const UniqueMesh& nameMesh = chunkedRoomNamesForLayer.at(roomAreaHash);
                    if (nameMesh.isValid()) { // Check UniqueMesh validity
                        // Font shader is managed internally by GLFont render methods
                        // m_glFont.getFontShader().bind(); // Removed
                        fontShaderWasBound = true; // Keep track if any font mesh exists in this layer
                        // No break here, need to check all chunks for this layer
                    }
                }
            }
        }

        // Iterate all visible chunks for this layer
        for (const RoomAreaHash roomAreaHash : visibleChunkIdsForLayer) {
            // Render LayerMeshes (terrain, etc.) for the chunk
            auto itChunkLayerMeshes = terrainChunksInLayer.find(roomAreaHash);
            if (itChunkLayerMeshes != terrainChunksInLayer.end()) {
                LayerMeshes& meshes = const_cast<LayerMeshes&>(itChunkLayerMeshes->second);
                if (meshes) {
                    meshes.render(layerId, m_currentLayer);
                }
            }

            // Render connections for this specific chunk if extra detail is on
            if (wantExtraDetail) {
                auto itLayerConnectionMeshes = batches.connectionMeshes.find(layerId);
                if (itLayerConnectionMeshes != batches.connectionMeshes.end()) {
                    const auto& chunkedConnectionMeshesForLayer = itLayerConnectionMeshes->second;
                    auto itChunkConnectionMeshes = chunkedConnectionMeshesForLayer.find(roomAreaHash);
                    if (itChunkConnectionMeshes != chunkedConnectionMeshesForLayer.end()) {
                        const ConnectionMeshes& connMeshes = itChunkConnectionMeshes->second;
                        connMeshes.render(layerId, m_currentLayer);
                    }
                }
            }

            // Render room names for this specific chunk if conditions met
            if (fontShaderWasBound && itLayerRoomNames != batches.roomNameBatches.end()) {
                 // (wantDoorNames and layerId == m_currentLayer already checked for shader binding)
                const auto& chunkedRoomNamesForLayer = itLayerRoomNames->second;
                auto itChunkRoomNames = chunkedRoomNamesForLayer.find(roomAreaHash);
                if (itChunkRoomNames != chunkedRoomNamesForLayer.end()) {
                    const UniqueMesh& nameMesh = itChunkRoomNames->second;
                    if (nameMesh.isValid()) { // Check UniqueMesh validity
                        nameMesh.render(nameRenderState);
                    }
                }
            }
        } // End of chunk loop

        if (fontShaderWasBound) {
            // Font shader is managed internally by GLFont render methods
            // m_glFont.getFontShader().release(); // Removed
        }
    } // End of layer loop

    // If the current layer had no terrain meshes at all, ensure depth is cleared and background faded.
    if (!currentLayerHasBeenSetup && std::abs(m_currentLayer - m_currentLayer) <= getConfig().canvas.mapRadius[2]) {
        // Check if current layer should have been processed (e.g. it's in m_visibleChunks)
        // This ensures that even if a visible layer has no terrain, its connections/names might still show over a clean bg.
        auto itVisibleCurrentLayer = m_visibleChunks.find(m_currentLayer);
        if (itVisibleCurrentLayer != m_visibleChunks.end() && !itVisibleCurrentLayer->second.empty()){
             gl.clearDepth();
             fadeBackground();
        } else if (allChunkedLayerMeshes.find(m_currentLayer) == allChunkedLayerMeshes.end() &&
                   batches.connectionMeshes.find(m_currentLayer) == batches.connectionMeshes.end() &&
                   batches.roomNameBatches.find(m_currentLayer) == batches.roomNameBatches.end()) {
            // If current layer has no data at all (no terrain, no connections, no names), still clear.
            gl.clearDepth();
            fadeBackground();
        }
    }
}
