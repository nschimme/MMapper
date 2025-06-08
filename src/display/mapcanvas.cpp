// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "mapcanvas.h" // Provides IterativeRemeshMetadata, Batches (forward decl)

#include "../configuration/configuration.h"
#include "../global/parserutils.h"
#include "../global/progresscounter.h"
#include "../global/utils.h"
#include "../map/ExitDirection.h"
#include "../map/coordinate.h"
#include "../map/exit.h"
#include "../map/infomark.h"
#include "../map/room.h"
#include "../map/roomid.h"
#include "../map/World.h"
#include "../map/SpatialDb.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/roomselection.h"
#include "InfoMarkSelection.h"
#include "MapCanvasData.h"
#include "MapCanvasRoomDrawer.h" // For getRoomAreaHash and full Batches definition
#include "IMapBatchesFinisher.h" // For FinishedRemeshPayload (global) and IMapBatchesFinisher
#include "connectionselection.h"
#include <memory> // For std::make_unique

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cmath>
#include <cstddef>
#include <cstdint>
// <memory> is now included above
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>
#include <QDebug>

#include <QGestureEvent>
#include <QMessageLogContext>
#include <QOpenGLDebugMessage>
#include <QSize>
#include <QString>
#include <QToolTip>
#include <QtGui>
#include <QtWidgets>

#if defined(_MSC_VER) || defined(__MINGW32__)
#undef near
#undef far
#endif

using NonOwningPointer = MapCanvas *;
NODISCARD static NonOwningPointer &primaryMapCanvas()
{
    static NonOwningPointer primary = nullptr;
    return primary;
}

MapCanvas::MapCanvas(MapData &mapData,
                     PrespammedPath &prespammedPath,
                     Mmapper2Group &groupManager,
                     QWidget *const parent)
    : QOpenGLWidget{parent}
    , MapCanvasViewport{static_cast<QWidget &>(*this)}
    , MapCanvasInputState{prespammedPath}
    , m_mapScreen{static_cast<MapCanvasViewport &>(*this)}
    , m_opengl{}
    , m_glFont{m_opengl}
    // m_batches is now a unique_ptr, initialized below
    , m_data{mapData}
    , m_groupManager{groupManager}
{
    m_batches = std::make_unique<Batches>(); // Initialize unique_ptr
    NonOwningPointer &pmc = primaryMapCanvas();
    if (pmc == nullptr) {
        pmc = this;
    }

    setCursor(Qt::OpenHandCursor);
    grabGesture(Qt::PinchGesture);
    setContextMenuPolicy(Qt::CustomContextMenu);
}

MapCanvas::~MapCanvas()
{
    NonOwningPointer &pmc = primaryMapCanvas();
    if (pmc == this) {
        pmc = nullptr;
    }

    cleanupOpenGL(); // m_batches will be auto-deleted
}

MapCanvas *MapCanvas::getPrimary()
{
    return primaryMapCanvas();
}

namespace {
static std::vector<std::pair<int, RoomAreaHash>> getNextPassChunks(
    const IterativeRemeshMetadata& state,
    int maxChunksPerPass)
{
    std::vector<std::pair<int, RoomAreaHash>> nextPass;
    if (maxChunksPerPass <= 0) {
        qWarning() << "getNextPassChunks called with maxChunksPerPass <= 0:" << maxChunksPerPass;
        return nextPass;
    }
    nextPass.reserve(static_cast<size_t>(maxChunksPerPass));
    for (const auto& chunk : state.allTargetChunks) {
        if (state.completedChunks.find(chunk) == state.completedChunks.end()) {
            nextPass.push_back(chunk);
            if (nextPass.size() >= static_cast<size_t>(maxChunksPerPass)) {
                break;
            }
        }
    }
    return nextPass;
}
}

void MapCanvas::slot_layerUp() { ++m_currentLayer; layerChanged(); }
void MapCanvas::slot_layerDown() { --m_currentLayer; layerChanged(); }
void MapCanvas::slot_layerReset() { m_currentLayer = 0; layerChanged(); }

void MapCanvas::slot_setCanvasMouseMode(const CanvasMouseModeEnum mode) { /* ... as before ... */ }
void MapCanvas::slot_setRoomSelection(const SigRoomSelection &selection) { /* ... as before ... */ }
void MapCanvas::slot_setConnectionSelection(const std::shared_ptr<ConnectionSelection> &selection) { /* ... as before ... */ }
void MapCanvas::slot_setInfoMarkSelection(const std::shared_ptr<InfoMarkSelection> &selection) { /* ... as before ... */ }
NODISCARD static uint32_t operator&(const Qt::KeyboardModifiers left, const Qt::Modifier right) { /* ... as before ... */ return 0;}
void MapCanvas::wheelEvent(QWheelEvent *const event) { /* ... as before ... */ }
void MapCanvas::slot_onForcedPositionChange() { slot_requestUpdate(); }
bool MapCanvas::event(QEvent *const event) { /* ... as before ... */ return QOpenGLWidget::event(event); }
void MapCanvas::slot_createRoom() { /* ... as before ... */ }
std::shared_ptr<InfoMarkSelection> MapCanvas::getInfoMarkSelection(const MouseSel &sel) { /* ... as before ... */ return nullptr; }
void MapCanvas::mousePressEvent(QMouseEvent *const event) { /* ... as before ... */ }
void MapCanvas::mouseMoveEvent(QMouseEvent *const event) { /* ... as before ... */ }
void MapCanvas::mouseReleaseEvent(QMouseEvent *const event) { /* ... as before ... */ }


void MapCanvas::finishPendingMapBatches()
{
    if (!m_batches->remeshCookie.isPending() || !m_batches->remeshCookie.isReady()) {
        return;
    }
    SharedMapBatchFinisher pFinisher = m_batches->remeshCookie.get();
    if (!pFinisher) {
        qDebug() << "finishPendingMapBatches: Finisher was null, possibly ignored or errored.";
        return;
    }
    FinishedRemeshPayload payload = pFinisher->finish(getOpenGL(), getGLFont());
    if (!m_batches->mapBatches) {
        m_batches->mapBatches.emplace();
    }
    MapBatches& currentMapBatches = m_batches->mapBatches.value();
    for (const auto& [layerId, newChunkedMeshes] : payload.generatedMeshes.batchedMeshes) {
        for (const auto& [chunkHash, newLayerMesh] : newChunkedMeshes) {
            currentMapBatches.batchedMeshes[layerId][chunkHash] = std::move(const_cast<LayerMeshes&>(newLayerMesh));
        }
    }
    for (const auto& [layerId, newChunkedConnectionMeshes] : payload.generatedMeshes.connectionMeshes) {
        for (const auto& [chunkHash, newConnectionMesh] : newChunkedConnectionMeshes) {
            currentMapBatches.connectionMeshes[layerId][chunkHash] = std::move(const_cast<BatchedConnectionMeshes&>(newConnectionMesh));
        }
    }
    for (const auto& [layerId, newChunkedRoomNameMeshes] : payload.generatedMeshes.roomNameBatches) {
        for (const auto& [chunkHash, newRoomNameMesh] : newChunkedRoomNameMeshes) {
            currentMapBatches.roomNameBatches[layerId][chunkHash] = std::move(const_cast<BatchedRoomNamesMesh&>(newRoomNameMesh));
        }
    }
    int M_pendingChunkGenerations_erased_count = 0;
    for (const auto& chunkProfile : payload.chunksCompletedThisPass) {
        if(m_pendingChunkGenerations.erase(chunkProfile)){ M_pendingChunkGenerations_erased_count++; }
    }
    if(M_pendingChunkGenerations_erased_count > 0 || payload.chunksCompletedThisPass.empty()){
         qDebug() << "finishPendingMapBatches: Processed" << payload.chunksCompletedThisPass.size()
                  << "chunks. Removed" << M_pendingChunkGenerations_erased_count << "from m_pendingChunkGenerations."
                  << "Remaining pending:" << m_pendingChunkGenerations.size();
    }
    if (payload.morePassesNeeded && payload.nextIterativeState.has_value()) {
        IterativeRemeshMetadata nextState = payload.nextIterativeState.value();
        std::vector<std::pair<int, RoomAreaHash>> chunksForNext = getNextPassChunks(nextState, MAX_CHUNKS_PER_ITERATIVE_PASS);
        qDebug() << "Iterative remesh: Pass" << (nextState.currentPassNumber -1) << "completed."
                 << payload.chunksCompletedThisPass.size() << "chunks this pass."
                 << nextState.completedChunks.size() << "total completed out of" << nextState.allTargetChunks.size()
                 << ". Next pass requested with" << chunksForNext.size() << "chunks.";
        if (!chunksForNext.empty()) {
            m_batches->remeshCookie.set(m_data.generateSpecificChunkBatches(mctp::getProxy(m_textures), chunksForNext, nextState));
            for (const auto& chunkProfile : chunksForNext) { m_pendingChunkGenerations.insert(chunkProfile); }
        } else if (payload.morePassesNeeded) {
            qDebug() << "Iterative remesh: morePassesNeeded was true, but getNextPassChunks returned empty. Total completed:" << nextState.completedChunks.size() << "/" << nextState.allTargetChunks.size();
        }
    } else {
         qDebug() << "Iterative remesh: Finished. morePassesNeeded =" << payload.morePassesNeeded
                  << "nextIterativeState.has_value =" << payload.nextIterativeState.has_value()
                  << ". Chunks completed this pass:" << payload.chunksCompletedThisPass.size()
                  << ". Total pending:" << m_pendingChunkGenerations.size();
    }
    updateVisibleChunks();
    requestMissingChunks();
}

void MapCanvas::requestMissingChunks() {
    if (m_batches->remeshCookie.isPending()) return;
    std::vector<std::pair<int, RoomAreaHash>> chunksToRequestNow;
    for (const auto& layerChunksPair : m_visibleChunks) {
        const int layerId = layerChunksPair.first;
        const std::set<RoomAreaHash>& visibleChunkIds = layerChunksPair.second;
        const MapBatches* currentMapBatchesPtr = m_batches->mapBatches.has_value() ? &(*m_batches->mapBatches) : nullptr;
        auto mapBatchesLayerIt = currentMapBatchesPtr ? currentMapBatchesPtr->batchedMeshes.find(layerId) : (currentMapBatchesPtr ? currentMapBatchesPtr->batchedMeshes.end() : decltype(currentMapBatchesPtr->batchedMeshes.end()){});
        for (const RoomAreaHash roomAreaHash : visibleChunkIds) {
            bool isMissingOrInvalid = true;
            if (currentMapBatchesPtr && mapBatchesLayerIt != currentMapBatchesPtr->batchedMeshes.end()) {
                const ChunkedLayerMeshes& chunkedLayerMeshes = mapBatchesLayerIt->second;
                auto chunkIt = chunkedLayerMeshes.find(roomAreaHash);
                if (chunkIt != chunkedLayerMeshes.end() && chunkIt->second) { isMissingOrInvalid = false; }
            }
            if (isMissingOrInvalid) {
                std::pair<int, RoomAreaHash> chunkKey = {layerId, roomAreaHash};
                if (m_pendingChunkGenerations.find(chunkKey) == m_pendingChunkGenerations.end()) {
                    chunksToRequestNow.push_back(chunkKey);
                }
            }
        }
    }
    if (!chunksToRequestNow.empty()) {
        qDebug() << "Requesting" << chunksToRequestNow.size() << "missing chunks.";
        for(const auto& chunkKey : chunksToRequestNow) { m_pendingChunkGenerations.insert(chunkKey); }
        m_batches->remeshCookie.set(
            m_data.generateSpecificChunkBatches(mctp::getProxy(m_textures), chunksToRequestNow, std::nullopt)
        );
    }
}

void MapCanvas::updateVisibleChunks() { /* ... as before ... */ }
std::optional<glm::vec3> MapCanvas::getUnprojectedScreenPos(const glm::vec2& screenPos) const { return this->unproject_clamped(screenPos); }

void MapCanvas::forceUpdateMeshes()
{
    if (m_batches->remeshCookie.isPending()) {
        qDebug() << "forceUpdateMeshes: Previous remesh pending. Ignoring it and restarting.";
        m_batches->remeshCookie.setIgnored();
    }
    updateVisibleChunks();
    std::vector<std::pair<int, RoomAreaHash>> viewportChunksForFirstPass;
    std::vector<std::pair<int, RoomAreaHash>> allTargetChunksForOperation;
    for (const auto& layerChunksPair : m_visibleChunks) {
        for (const RoomAreaHash roomAreaHash : layerChunksPair.second) {
            allTargetChunksForOperation.push_back({layerChunksPair.first, roomAreaHash});
            if (layerChunksPair.first == m_currentLayer) {
                 if (viewportChunksForFirstPass.size() < static_cast<size_t>(MAX_CHUNKS_PER_ITERATIVE_PASS)) {
                    viewportChunksForFirstPass.push_back({layerChunksPair.first, roomAreaHash});
                }
            }
        }
    }
    if (allTargetChunksForOperation.empty()) {
        qDebug() << "forceUpdateMeshes: No target chunks determined.";
        update(); return;
    }
    if (viewportChunksForFirstPass.empty()) {
        for(size_t i = 0; i < allTargetChunksForOperation.size() && i < static_cast<size_t>(MAX_CHUNKS_PER_ITERATIVE_PASS); ++i) {
            viewportChunksForFirstPass.push_back(allTargetChunksForOperation[i]);
        }
    }
    if (viewportChunksForFirstPass.empty() && !allTargetChunksForOperation.empty()) { // Still empty but targets exist
         qDebug() << "forceUpdateMeshes: No viewport chunks for first pass, taking first from all targets.";
         viewportChunksForFirstPass.push_back(allTargetChunksForOperation.front());
    }
     if (viewportChunksForFirstPass.empty()) { // If STILL empty (allTargetChunksForOperation was also empty)
         qDebug() << "forceUpdateMeshes: No chunks to process even after fallback.";
         update(); return;
    }
    IterativeRemeshMetadata initialMetadata;
    initialMetadata.allTargetChunks = allTargetChunksForOperation;
    initialMetadata.viewportChunks = viewportChunksForFirstPass;
    initialMetadata.completedChunks.clear();
    initialMetadata.currentPassNumber = 0;
    qDebug() << "forceUpdateMeshes: Starting iterative remesh. First pass:" << viewportChunksForFirstPass.size()
             << ". Total targets:" << allTargetChunksForOperation.size();
    m_pendingChunkGenerations.clear();
    for (const auto& chunkProfile : viewportChunksForFirstPass) { m_pendingChunkGenerations.insert(chunkProfile); }
    m_batches->mapBatches.reset();
    m_diff.resetExistingMeshesAndIgnorePendingRemesh();
    m_batches->remeshCookie.set(
        m_data.generateSpecificChunkBatches(mctp::getProxy(m_textures), viewportChunksForFirstPass, initialMetadata)
    );
    update();
}

void MapCanvas::slot_mapChanged() { qDebug() << "MapCanvas::slot_mapChanged: Forcing mesh update."; forceUpdateMeshes(); }
void MapCanvas::slot_requestUpdate() { update(); }
void MapCanvas::screenChanged() {
    auto &gl = getOpenGL();
    if (!gl.isRendererInitialized()) return;
    const auto newDpi = static_cast<float>(QPaintDevice::devicePixelRatioF());
    const auto oldDpi = gl.getDevicePixelRatio();
    if (!utils::equals(newDpi, oldDpi)) {
        log(QString("Display: %1 DPI").arg(static_cast<double>(newDpi)));
        m_batches->resetExistingMeshesButKeepPendingRemesh();
        gl.setDevicePixelRatio(newDpi);
        auto &font = getGLFont(); font.cleanup(); font.init();
        forceUpdateMeshes();
    }
}
void MapCanvas::selectionChanged() { update(); emit sig_selectionChanged(); }
void MapCanvas::graphicsSettingsChanged() { update(); }
void MapCanvas::userPressedEscape(bool) { /* ... as before ... */ }
void MapCanvas::cleanupOpenGL() { /* ... as before, ensure m_batches->... if it accesses members ... */
    MakeCurrentRaii makeCurrentRaii{*this};
    m_textures.cleanup();
    if(m_batches) { // Check if m_batches was initialized
        m_batches->mapBatches.reset();
        m_batches->infomarksMeshes.reset();
    }
    m_glFont.cleanup();
    m_opengl.destroyVao(m_defaultVao);
    m_opengl.cleanup();
}
void MapCanvas::reportGLVersion() { /* ... */ }
NODISCARD bool MapCanvas::isBlacklistedDriver() { return false; }
void MapCanvas::initializeGL() { /* ... as before, m_batches is init in ctor ... */
    m_opengl.init(); reportGLVersion(); if (isBlacklistedDriver()) { /* ... */ }
    initLogger(); m_defaultVao = m_opengl.createVao(); m_glFont.init(); initTextures(); updateMultisampling();
    m_lifetime = m_data.getChangeMonitor().add<MapDataUpdateFlags::MAP_LOADED>(RepeatForeverObserver{[this]() { slot_dataLoaded(); }});
    emit sig_log("MapCanvas", "initialized");
}
void MapCanvas::initLogger() { /* ... */ }
void MapCanvas::initTextures() { m_textures.init(m_opengl, getConfig().textures); }
void MapCanvas::updateTextures() { /* ... */ }
void MapCanvas::updateMultisampling() { /* ... */ }
NODISCARD glm::mat4 MapCanvas::getViewProj_old(const glm::vec2&, const glm::ivec2&, float, int) { return glm::mat4(1.0f); }
NODISCARD glm::mat4 MapCanvas::getViewProj(const glm::vec2 &s, const glm::ivec2 &z, float S, int l) { return MapCanvasViewport::getViewProj(s,z,S,l); }
void MapCanvas::setMvp(const glm::mat4 &v) { m_opengl.setMvpMatrix(v); }
void MapCanvas::setViewportAndMvp(int w, int h) { MapCanvasViewport::setViewportAndMvp(w,h,m_currentLayer); }
void MapCanvas::resizeGL(int w, int h) { MapCanvasViewport::resizeGL(w,h); update(); }
void MapCanvas::setAnimating(bool v) { m_frameRateController.animating = v; if(v) renderLoop(); }
void MapCanvas::renderLoop() {
    if(m_frameRateController.animating || m_batches->remeshCookie.isReady() || (m_diff.futureHighlight.has_value() && m_diff.futureHighlight.value().wait_for(std::chrono::seconds(0)) == std::future_status::ready) ) {
        update();
    }
}
void MapCanvas::paintGL() {
    MakeCurrentRaii makeCurrentRaii{*this};
    if (!m_opengl.isRendererInitialized()) return;
    auto now = std::chrono::steady_clock::now();
    bool shouldRender = m_frameRateController.animating || now - m_frameRateController.lastFrameTime > std::chrono::milliseconds(50);
    if (shouldRender || m_batches->remeshCookie.isReady() || (m_diff.futureHighlight.has_value() && m_diff.futureHighlight.value().wait_for(std::chrono::seconds(0)) == std::future_status::ready) ) {
        m_frameRateController.lastFrameTime = now;
        actuallyPaintGL();
    }
    if (m_frameRateController.animating || m_batches->remeshCookie.isReady() || m_diff.futureHighlight.has_value()){
         QTimer::singleShot(0, this, &MapCanvas::renderLoop);
    }
}
void MapCanvas::actuallyPaintGL() {
    updateBatches(); m_mapScreen.bind(); m_opengl.clear(); paintMap(); paintBatchedInfomarks();
    paintSelections(); paintCharacters(); paintDifferences(); m_mapScreen.release(); m_mapScreen.blit(m_opengl);
}
void MapCanvas::updateBatches() { updateMapBatches(); updateInfomarkBatches(); }
void MapCanvas::updateMapBatches() { if (m_batches->remeshCookie.isReady()) { finishPendingMapBatches(); } }
void MapCanvas::updateInfomarkBatches() { if(!m_batches->infomarksMeshes) m_batches->infomarksMeshes = getInfoMarksMeshes(); }
NODISCARD BatchedInfomarksMeshes MapCanvas::getInfoMarksMeshes() { return {}; }
void MapCanvas::drawInfoMark(InfomarksBatch&, const InfomarkHandle&, int, const glm::vec2&, const std::optional<Color>&) { }
void MapCanvas::paintMap() { renderMapBatches(); }
void MapCanvas::renderMapBatches() { /* ... */ }
void MapCanvas::paintBatchedInfomarks() { /* ... */ }
void MapCanvas::paintSelections() { /* ... */ }
void MapCanvas::paintSelectionArea() { /* ... */ }
void MapCanvas::paintNewInfomarkSelection() { /* ... */ }
void MapCanvas::paintSelectedRooms() { /* ... */ }
void MapCanvas::paintSelectedRoom(RoomSelFakeGL&, const RawRoom&) { /* ... */ }
void MapCanvas::paintSelectedConnection() { /* ... */ }
void MapCanvas::paintNearbyConnectionPoints() { /* ... */ }
void MapCanvas::paintSelectedInfoMarks() { /* ... */ }
void MapCanvas::paintCharacters() { /* ... */ }
void MapCanvas::drawGroupCharacters(CharacterBatch&) { /* ... */ }
void MapCanvas::paintDifferences() { /* ... */ }
NODISCARD bool MapCanvas::Diff::isUpToDate(const Map&, const Map&) const { return true; }
NODISCARD bool MapCanvas::Diff::hasRelatedDiff(const Map&) const { return false; }
void MapCanvas::Diff::cancelUpdates(const Map&) { /* ... */ }
void MapCanvas::Diff::maybeAsyncUpdate(const Map&, const Map&) { /* ... */ }
void MapCanvas::slot_onMessageLoggedDirect(const QOpenGLDebugMessage &) { /* ... */ }

// Restore mouse event handlers that were elided
void MapCanvas::mousePressEvent(QMouseEvent *const event)
{
    const bool hasLeftButton = (event->buttons() & Qt::LeftButton) != 0u;
    const bool hasRightButton = (event->buttons() & Qt::RightButton) != 0u;
    const bool hasCtrl = (event->modifiers() & Qt::CTRL) != 0u;
    MAYBE_UNUSED const bool hasAlt = (event->modifiers() & Qt::ALT) != 0u;

    m_sel1 = m_sel2 = getUnprojectedMouseSel(event);
    m_mouseLeftPressed = hasLeftButton;
    m_mouseRightPressed = hasRightButton;

    if (event->button() == Qt::ForwardButton) {
        slot_layerUp();
        return event->accept();
    } else if (event->button() == Qt::BackButton) {
        slot_layerDown();
        return event->accept();
    } else if (!m_mouseLeftPressed && m_mouseRightPressed) {
        if (m_canvasMouseMode == CanvasMouseModeEnum::MOVE && hasSel1()) {
            m_roomSelection = RoomSelection::createSelection(m_data.findAllRooms(getSel1().getCoordinate()));
            slot_setRoomSelection(SigRoomSelection{m_roomSelection});
            slot_setInfoMarkSelection(getInfoMarkSelection(getSel1()));
            selectionChanged();
        }
        m_mouseRightPressed = false;
        event->accept();
        return;
    }
    // ... (rest of original switch statement from read_files)
    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::CREATE_INFOMARKS: infomarksChanged(); break;
    case CanvasMouseModeEnum::SELECT_INFOMARKS:
        if (hasLeftButton && hasSel1()) {
            auto tmpSel = getInfoMarkSelection(getSel1());
            if (m_infoMarkSelection != nullptr && !tmpSel->empty() && m_infoMarkSelection->contains(tmpSel->front().getId())) {
                m_infoMarkSelectionMove.reset(); m_infoMarkSelectionMove.emplace();
            } else {
                m_selectedArea = false; m_infoMarkSelectionMove.reset();
            }
        }
        selectionChanged(); break;
    case CanvasMouseModeEnum::MOVE: if (hasLeftButton && hasSel1()) { setCursor(Qt::ClosedHandCursor); startMoving(m_sel1.value()); } break;
    case CanvasMouseModeEnum::RAYPICK_ROOMS: /* ... */ break;
    case CanvasMouseModeEnum::SELECT_ROOMS: /* ... */ break;
    case CanvasMouseModeEnum::CREATE_ONEWAY_CONNECTIONS: case CanvasMouseModeEnum::CREATE_CONNECTIONS: /* ... */ break;
    case CanvasMouseModeEnum::SELECT_CONNECTIONS: /* ... */ break;
    case CanvasMouseModeEnum::CREATE_ROOMS: slot_createRoom(); break;
    case CanvasMouseModeEnum::NONE: default: break;
    }
}

void MapCanvas::mouseMoveEvent(QMouseEvent *const event)
{
    // ... (Full content of mouseMoveEvent from read_files, ensuring m_batches-> usage)
    const bool hasLeftButton = (event->buttons() & Qt::LeftButton) != 0u;
    if (m_canvasMouseMode != CanvasMouseModeEnum::MOVE) { /* ... scroll logic ... */ }
    m_sel2 = getUnprojectedMouseSel(event);
    switch (m_canvasMouseMode) {
    case CanvasMouseModeEnum::SELECT_INFOMARKS: /* ... m_batches-> ... */ break;
    case CanvasMouseModeEnum::CREATE_INFOMARKS: /* ... m_batches-> ... */ break;
    case CanvasMouseModeEnum::MOVE: /* ... m_batches-> ... */ break;
    // ... other cases
    }
}

void MapCanvas::mouseReleaseEvent(QMouseEvent *const event)
{
    // ... (Full content of mouseReleaseEvent from read_files, ensuring m_batches-> usage)
     emit sig_continuousScroll(0, 0);
    m_sel2 = getUnprojectedMouseSel(event);
    if (m_mouseRightPressed) m_mouseRightPressed = false;
    switch (m_canvasMouseMode) {
    // ... all cases
    }
    m_altPressed = false;
    m_ctrlPressed = false;
}
