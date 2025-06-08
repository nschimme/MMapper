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
    MakeCurrentRaii makeCurrentRaii{*this};

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
        const QSurfaceFormat &format = context()->format();
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
               .arg(context()->isValid() ? "valid" : "invalid")
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
        hide();
        doneCurrent();
        QMessageBox::critical(this,
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
        this->update();
    });

    setConfig().canvas.showMissingMapId.registerChangeCallback(m_lifetime, [this]() {
        this->update();
    });

    setConfig().canvas.showUnmappedExits.registerChangeCallback(m_lifetime, [this]() {
        this->forceUpdateMeshes();
    });

    QOpenGLExtraFunctions *f = QOpenGLContext::currentContext()->extraFunctions();
    f->glGenVertexArrays(1, &m_defaultVao);
    f->glBindVertexArray(m_defaultVao);
}

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

    const auto rotateHorizontal = glm::mat3(
        glm::rotate(glm::mat4(1), -yawRadians, glm::vec3(0, 0, 1)));

    const auto forward = rotateHorizontal
                         * glm::vec3(0.f, std::sin(pitchRadians), -std::cos(pitchRadians));
    const auto right = rotateHorizontal * glm::vec3(1, 0, 0);
    const auto up = glm::cross(right, glm::normalize(forward));

    const auto eye = center - camDistance * forward;

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
    const auto size = getViewport().size;
    assert(size.x == width);
    assert(size.y == height);

    const float zoomScale = getTotalScaleFactor();
    const auto viewProj = (!want3D) ? getViewProj_old(m_scroll, size, zoomScale, m_currentLayer)
                                    : getViewProj(m_scroll, size, zoomScale, m_currentLayer);
    setMvp(viewProj);
    updateFrustumPlanes();
}

void MapCanvas::resizeGL(int width, int height)
{
    if (m_textures.room_modified == nullptr) {
        return;
    }

    setViewportAndMvp(width, height);
    // updateVisibleChunks(); // Called by forceUpdateMeshes when a new iterative remesh starts.
                           // Not strictly needed on every resize if a remesh is ongoing or complete,
                           // as the iterative process itself handles progressive loading.
                           // The primary role of updateVisibleChunks is now to seed forceUpdateMeshes.
    requestMissingChunks();

    update();
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

    static constexpr int TARGET_FRAMES_PER_SECOND = 20;
    auto targetFrameTime = std::chrono::milliseconds(1000 / TARGET_FRAMES_PER_SECOND);

    auto now = std::chrono::steady_clock::now();
    update();
    auto afterPaint = std::chrono::steady_clock::now();

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

    if (remeshCookie.isIterativeRemeshInProgress() && remeshCookie.isPending()) {
        return;
    }

    if (remeshCookie.isIterativeRemeshInProgress() && !remeshCookie.isPending()) {
        return;
    }

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

    MMLOG() << "[updateMapBatches] Conditions met to start a new full mesh generation via forceUpdateMeshes.";
    forceUpdateMeshes();
    return;
}

void MapCanvas::finishPendingMapBatches()
{
    RemeshCookie& cookie = m_batches.remeshCookie;
    if (cookie.isPending()) {
        qDebug() << "[R_FPB] finishPendingMapBatches: Cookie is PENDING. Ready:" << cookie.isReady()
                 << "Iterative:" << cookie.isIterativeRemeshInProgress();
    }

    std::string_view prefix = "[finishPendingMapBatches] ";

#define LOG() MMLOG() << prefix

    if (!cookie.isPending() || !cookie.isReady()) {
        return;
    }

    bool wasAnimatingForThisPass = m_frameRateController.animating;

    if (cookie.isIterativeRemeshInProgress()) {
        qDebug() << "[R_FPB] Iterative: About to call cookie.getPassData() for pass" << cookie.getCurrentPassNumber();
        SharedMapBatchFinisher passFinisher = cookie.getPassData();
        qDebug() << "[R_FPB] Iterative: passFinisher is valid:" << (passFinisher != nullptr);
        std::vector<std::pair<int, RoomAreaHash>> processedChunksThisPass = cookie.getCurrentPassChunks();

        if (passFinisher) {
            LOG() << "Finishing " << processedChunksThisPass.size() << " chunks from pass " << cookie.getCurrentPassNumber();

            std::optional<MapBatches> temporaryPassBatches_opt;
            ::finish(*passFinisher, temporaryPassBatches_opt, getOpenGL(), getGLFont());
            qDebug() << "[R_FPB] Iterative: Global ::finish called. temporaryPassBatches_opt.has_value():" << temporaryPassBatches_opt.has_value();
            if (temporaryPassBatches_opt.has_value()) {
                qDebug() << "[R_FPB] Iterative: temporaryPassBatches_opt.batchedMeshes size (layers):" << temporaryPassBatches_opt->batchedMeshes.size();
                if (!temporaryPassBatches_opt->batchedMeshes.empty()) {
                    qDebug() << "[R_FPB] Iterative: Chunks in first layer of temporaryPassBatches_opt:" << temporaryPassBatches_opt->batchedMeshes.begin()->second.size();
                }
            }

            if (temporaryPassBatches_opt.has_value()) {
                MapBatches& passOutputBatches = *temporaryPassBatches_opt;
                if (!m_batches.mapBatches.has_value()) {
                    m_batches.mapBatches.emplace(std::move(passOutputBatches));
                } else {
                    MapBatches& mainMapBatches = *m_batches.mapBatches;
                    for (auto& layer_pair : passOutputBatches.batchedMeshes) {
                        for (auto& chunk_pair : layer_pair.second) {
                            mainMapBatches.batchedMeshes[layer_pair.first].insert_or_assign(chunk_pair.first, std::move(chunk_pair.second));
                        }
                    }
                    for (auto& layer_pair : passOutputBatches.connectionMeshes) {
                       for (auto& chunk_pair : layer_pair.second) {
                            mainMapBatches.connectionMeshes[layer_pair.first].insert_or_assign(chunk_pair.first, std::move(chunk_pair.second));
                        }
                    }
                    for (auto& layer_pair : passOutputBatches.roomNameBatches) {
                        for (auto& chunk_pair : layer_pair.second) {
                            mainMapBatches.roomNameBatches[layer_pair.first].insert_or_assign(chunk_pair.first, std::move(chunk_pair.second));
                        }
                    }
                }
                qDebug() << "[R_FPB] Iterative: m_batches.mapBatches updated. Total layers in mapBatches:" << (m_batches.mapBatches ? m_batches.mapBatches->batchedMeshes.size() : 0);
                if (m_batches.mapBatches && !m_batches.mapBatches->batchedMeshes.empty()) {
                    size_t total_chunks_in_mapbatches = 0;
                    for(const auto& layer_entry : m_batches.mapBatches->batchedMeshes) {
                        total_chunks_in_mapbatches += layer_entry.second.size();
                    }
                    qDebug() << "[R_FPB] Iterative: Total chunks in m_batches.mapBatches:" << total_chunks_in_mapbatches;
                }
            } else {
                 LOG() << "Pass finisher did not produce batches.";
            }

            cookie.recordChunksAsCompleted(processedChunksThisPass);
            for(const auto& chunkKey : processedChunksThisPass) {
                m_pendingChunkGenerations.erase(chunkKey);
            }

            Coordinate vc = cookie.getViewportCenter().value_or(Coordinate(0,0,m_currentLayer));
            std::vector<std::pair<int, RoomAreaHash>> nextPassChunks =
                calculateNextPassChunks(cookie.getCurrentPassNumber() + 1,
                                        vc,
                                        cookie.getCompletedChunks(),
                                        m_data.getCurrentMap());
            qDebug() << "[R_FPB] Iterative: Calculated nextPassChunks. Count:" << nextPassChunks.size();
            if (!nextPassChunks.empty()) {
                qDebug() << "[R_FPB] Iterative: First few nextPassChunks (max 3):";
                for (size_t i = 0; i < std::min<size_t>(nextPassChunks.size(), 3u); ++i) {
                    qDebug() << "[R_FPB] Iterative:   Layer:" << nextPassChunks[i].first << "Hash:" << nextPassChunks[i].second;
                }
            }

            if (!nextPassChunks.empty()) {
                LOG() << "Requesting next pass with " << nextPassChunks.size() << " chunks.";
                for(const auto& chunkKey : nextPassChunks) { m_pendingChunkGenerations.insert(chunkKey); }
                auto nextFuture = m_data.generateSpecificChunkBatches(mctp::getProxy(m_textures), nextPassChunks);
                cookie.continueIterativePass(std::move(nextFuture), cookie.getCurrentPassNumber() + 1, nextPassChunks);
                setAnimating(true);
            } else {
                LOG() << "Iterative remesh complete.";
                (void)cookie.finalizeIterativeRemesh();
                m_pendingChunkGenerations.clear();
                setAnimating(false);
            }
        } else {
            qDebug() << "[R_FPB] Iterative: Pass " << cookie.getCurrentPassNumber() << " failed or was ignored.";
            LOG() << "Iterative pass future was null or ignored.";
            (void)cookie.finalizeIterativeRemesh();
            m_pendingChunkGenerations.clear();
            setAnimating(false);
        }
    }
    // Non-iterative path - to be phased out or removed.
    /*
    else { // Old non-iterative path (to be phased out or kept for specific full loads)
        LOG() << "Non-iterative: Waiting for the cookie.";
        SharedMapBatchFinisher pFuture = cookie.get(); // Original get()
        qDebug() << "[R_FPB] Non-Iterative: pFuture is valid:" << (pFuture != nullptr);
        assert(!cookie.isPending()); // get() should have reset this
        setAnimating(false); // Stop animation after a full (non-iterative) load

        if (pFuture == nullptr) {
            LOG() << "Got NULL (means the update was flagged to be ignored)";
            m_pendingChunkGenerations.clear(); // Clear if ignored
        } else {
            LOG() << "Clearing the map batches and call the finisher to create new ones";
            DECL_TIMER(t, __FUNCTION__);
            const IMapBatchesFinisher &finisher_obj = *pFuture;

            m_batches.mapBatches.reset();
            ::finish(finisher_obj, m_batches.mapBatches, getOpenGL(), getGLFont());
            qDebug() << "[R_FPB] Non-Iterative: Global ::finish called. m_batches.mapBatches.has_value():" << m_batches.mapBatches.has_value();
            if (m_batches.mapBatches.has_value()) {
                qDebug() << "[R_FPB] Non-Iterative: m_batches.mapBatches.batchedMeshes size (layers):" << m_batches.mapBatches->batchedMeshes.size();
            }

            if (m_batches.mapBatches.has_value()) {
                for (const auto& layer_pair : m_batches.mapBatches->batchedMeshes) {
                    for (const auto& chunk_pair : layer_pair.second) {
                        m_pendingChunkGenerations.erase({layer_pair.first, chunk_pair.first});
                    }
                }
            }
        }
    }
    */
    if (wasAnimatingForThisPass && !cookie.isPending() && !cookie.isIterativeRemeshInProgress()){
         setAnimating(false);
    }
    update();
#undef LOG
}

void MapCanvas::actuallyPaintGL()
{
    setViewportAndMvp(width(), height());

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

    if (isUpToDate(saved, current)) {
        return;
    }

    const bool showNeedsServerId = getConfig().canvas.showMissingMapId.get();
    const bool showChanged = getConfig().canvas.showUnsavedChanges.get();

    diff.futureHighlight = std::async(
        std::launch::async,
        [saved, current, showNeedsServerId, showChanged]() -> Diff::HighlightDiff {
            DECL_TIMER(t2, "[async] actuallyPaintGL: highlight differences and needs update");

            static constexpr std::array<glm::vec3, 4> corners{
                glm::vec3{0, 0, 0},
                glm::vec3{1, 0, 0},
                glm::vec3{1, 1, 0},
                glm::vec3{0, 1, 0},
            };

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
            update();
        }
        return;
    }

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

        updateBatches();
        finishPendingMapBatches();

        if (showPerfStats) {
            optAfterBatches = Clock::now();
        }

        actuallyPaintGL();
    }

    if (!showPerfStats) {
        return;
    }

    const auto &start = optStart.value();
    const auto &afterTextures = optAfterTextures.value();
    const auto &afterBatches = optAfterBatches.value();
    const auto afterPaint = Clock::now();
    const bool calledFinish = [this]() -> bool {
        if (auto *const ctxt = QOpenGLWidget::context()) {
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

    const auto w = width();
    const auto h = height();
    const auto dpr = getOpenGL().getDevicePixelRatio();

    auto &font = getGLFont();
    std::vector<GLText> text;

    const auto lineHeight = font.getFontHeight();
    const float rightMargin = float(w) * dpr
                              - static_cast<float>(font.getGlyphAdvance('e').value_or(5));

    auto y = lineHeight;
    const auto print = [lineHeight, rightMargin, &text, &y](const QString &msg) {
        text.emplace_back(glm::vec3(rightMargin, y, 0),
                          mmqt::toStdStringLatin1(msg),
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

    auto &gl = getOpenGL();
    const auto layer = static_cast<float>(m_currentLayer);

    if (m_selectedArea) {
        const glm::vec3 A{pos1, layer};
        const glm::vec3 B{pos2.x, pos1.y, layer};
        const glm::vec3 C{pos2, layer};
        const glm::vec3 D{pos1.x, pos2.y, layer};

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

    MAYBE_UNUSED const bool enabled = getOpenGL().tryEnableMultisampling(wantMultisampling);
    activeStatus = wantMultisampling;
}

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
    const auto wantDoorNames = settings.drawDoorNames
                               && (totalScaleFactor >= settings.doorNameScaleCutoff);

    auto &gl = getOpenGL();
    const auto& allChunkedLayerMeshes = batches.batchedMeshes;

    const auto fadeBackground = [&gl, &settings]() {
        auto bgColor = Color{settings.backgroundColor.getColor(), 0.5f};
        const auto blendedWithBackground = GLRenderState().withBlend(BlendModeEnum::TRANSPARENCY).withColor(bgColor);
        gl.renderPlainFullScreenQuad(blendedWithBackground);
    };

    bool currentLayerHasBeenSetup = false;

    for (const auto& layerMeshesPair : allChunkedLayerMeshes) {
        const int layerId = layerMeshesPair.first;
        const ChunkedLayerMeshes& terrainChunksInLayer = layerMeshesPair.second;

        if (std::abs(layerId - m_currentLayer) > getConfig().canvas.mapRadius[2]) {
            continue;
        }

        if (layerId == m_currentLayer && !currentLayerHasBeenSetup) {
            gl.clearDepth();
            fadeBackground();
            currentLayerHasBeenSetup = true;
        }

        auto itVisibleLayer = m_visibleChunks.find(layerId);
        if (itVisibleLayer == m_visibleChunks.end() || itVisibleLayer->second.empty()) {
            continue;
        }
        const std::set<RoomAreaHash>& visibleChunkIdsForLayer = itVisibleLayer->second;

        bool fontShaderWasBound = false;
        GLRenderState nameRenderState;
        nameRenderState = GLRenderState()
                              .withBlend(BlendModeEnum::TRANSPARENCY)
                              .withDepthFunction(DepthFunctionEnum::LEQUAL);

        auto itLayerRoomNames = batches.roomNameBatches.find(layerId);
        if (wantDoorNames && layerId == m_currentLayer && itLayerRoomNames != batches.roomNameBatches.end()) {
            const auto& chunkedRoomNamesForLayer = itLayerRoomNames->second;
            for (const RoomAreaHash roomAreaHash : visibleChunkIdsForLayer) {
                if (chunkedRoomNamesForLayer.count(roomAreaHash)) {
                    const UniqueMesh& nameMesh = chunkedRoomNamesForLayer.at(roomAreaHash);
                    if (nameMesh.isValid()) {
                        fontShaderWasBound = true;
                    }
                }
            }
        }

        for (const RoomAreaHash roomAreaHash : visibleChunkIdsForLayer) {
            auto itChunkLayerMeshes = terrainChunksInLayer.find(roomAreaHash);
            if (itChunkLayerMeshes != terrainChunksInLayer.end()) {
                LayerMeshes& meshes = const_cast<LayerMeshes&>(itChunkLayerMeshes->second);
                if (meshes) {
                    meshes.render(layerId, m_currentLayer);
                }
            }

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

            if (fontShaderWasBound && itLayerRoomNames != batches.roomNameBatches.end()) {
                const auto& chunkedRoomNamesForLayer = itLayerRoomNames->second;
                auto itChunkRoomNames = chunkedRoomNamesForLayer.find(roomAreaHash);
                if (itChunkRoomNames != chunkedRoomNamesForLayer.end()) {
                    const UniqueMesh& nameMesh = itChunkRoomNames->second;
                    if (nameMesh.isValid()) {
                        nameMesh.render(nameRenderState);
                    }
                }
            }
        }

        if (fontShaderWasBound) {
            // Font shader is managed internally by GLFont render methods
        }
    }

    if (!currentLayerHasBeenSetup && std::abs(m_currentLayer - m_currentLayer) <= getConfig().canvas.mapRadius[2]) {
        auto itVisibleCurrentLayer = m_visibleChunks.find(m_currentLayer);
        if (itVisibleCurrentLayer != m_visibleChunks.end() && !itVisibleCurrentLayer->second.empty()){
             gl.clearDepth();
             fadeBackground();
        } else if (allChunkedLayerMeshes.find(m_currentLayer) == allChunkedLayerMeshes.end() &&
                   batches.connectionMeshes.find(m_currentLayer) == batches.connectionMeshes.end() &&
                   batches.roomNameBatches.find(m_currentLayer) == batches.roomNameBatches.end()) {
            gl.clearDepth();
            fadeBackground();
        }
    }
}

[end of src/display/mapcanvas_gl.cpp]
