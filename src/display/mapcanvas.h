#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../global/RuleOf5.h"
#include "../map/coordinate.h"
#include "../map/roomid.h"
#include "../mapdata/roomselection.h"
#include "../opengl/OpenGL.h"
#include "../opengl/Font.h"
#include "CanvasMouseModeEnum.h"
#include "MapCanvasData.h"
#include "MapCanvasRoomDrawer.h"
#include "Textures.h"
#include "MapCanvasViewModel.h"

#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <variant>
#include <QOpenGLDebugMessage>
#include <vector>

#include <QOpenGLWidget>
#include <QTimer>

class CGroupChar;
class Mmapper2Group;
class MapData;
class PrespammedPath;
class ConnectionSelection;
class InfomarkSelection;
class CharacterBatch;
class RoomSelFakeGL;
class InfomarksBatch;
class GLFont;
class RawInfomark;
class InfomarkHandle;
class QOpenGLDebugLogger;

class NODISCARD_QOBJECT MapCanvas : public QOpenGLWidget
{
    Q_OBJECT

public:
    static constexpr const int BASESIZE = 528;
    static constexpr const int SCROLL_SCALE = 64;

private:
    struct NODISCARD FrameRateController final
    {
        std::chrono::steady_clock::time_point lastFrameTime;
        bool animating = false;
    };

    struct NODISCARD Diff final
    {
        using DiffQuadVector = std::vector<RoomQuadTexVert>;

        struct NODISCARD MaybeDataOrMesh final
            : public std::variant<std::monostate, DiffQuadVector, UniqueMesh>
        {
        public:
            using base = std::variant<std::monostate, DiffQuadVector, UniqueMesh>;
            using base::base;

        public:
            NODISCARD bool empty() const { return std::holds_alternative<std::monostate>(*this); }
            NODISCARD bool hasData() const { return std::holds_alternative<DiffQuadVector>(*this); }
            NODISCARD bool hasMesh() const { return std::holds_alternative<UniqueMesh>(*this); }

        public:
            NODISCARD const DiffQuadVector &getData() const
            {
                return std::get<DiffQuadVector>(*this);
            }
            NODISCARD const UniqueMesh &getMesh() const { return std::get<UniqueMesh>(*this); }

            void render(OpenGL &gl, const MMTextureId texId)
            {
                if (empty()) {
                    assert(false);
                    return;
                }

                if (hasData()) {
                    *this = gl.createRoomQuadTexBatch(getData(), texId);
                    assert(hasMesh());
                }

                if (!hasMesh()) {
                    assert(false);
                    return;
                }
                auto &mesh = getMesh();
                mesh.render(gl.getDefaultRenderState().withBlend(BlendModeEnum::TRANSPARENCY));
            }
        };

        struct NODISCARD HighlightDiff final
        {
            Map saved;
            Map current;
            MaybeDataOrMesh highlights;
        };

        std::optional<std::future<HighlightDiff>> futureHighlight;
        std::optional<HighlightDiff> highlight;

        NODISCARD bool isUpToDate(const Map &saved, const Map &current) const;
        NODISCARD bool hasRelatedDiff(const Map &save) const;
        void cancelUpdates(const Map &saved);
        void maybeAsyncUpdate(const Map &saved, const Map &current);

        void resetExistingMeshesAndIgnorePendingRemesh()
        {
            futureHighlight.reset();
            highlight.reset();
        }
    };

public:
    explicit MapCanvas(MapData &mapData,
                       PrespammedPath &prespammedPath,
                       Mmapper2Group &groupManager,
                       QWidget *parent = nullptr);
    ~MapCanvas() override;

    DELETE_CTORS_AND_ASSIGN_OPS(MapCanvas);

public:
    static MapCanvas *getPrimary();

public:
    void setMapMode(MapModeEnum mode);
    void center(const glm::vec2 &worldCoord);
    void requestUpdate();

    MapCanvasViewModel* viewModel() const { return m_viewModel.get(); }

    NODISCARD QSize minimumSizeHint() const override;
    NODISCARD QSize sizeHint() const override;

    void userPressedEscape(bool pressed);
    void screenChanged();
    void graphicsSettingsChanged();

public:
    void setZoom(float zoom);
    NODISCARD float getRawZoom() const;
    NODISCARD float getTotalScaleFactor() const;

public slots:
    void slot_layerUp();
    void slot_layerDown();
    void slot_layerReset();
    void slot_setCanvasMouseMode(const CanvasMouseModeEnum mode);
    void slot_setRoomSelection(const SigRoomSelection &selection);
    void slot_setConnectionSelection(const std::shared_ptr<ConnectionSelection> &selection);
    void slot_setInfomarkSelection(const std::shared_ptr<InfomarkSelection> &selection);
    void slot_createRoom();
    void slot_setScroll(const glm::vec2 &worldPos);
    void slot_setHorizontalScroll(const float worldX);
    void slot_setVerticalScroll(const float worldY);
    void slot_zoomIn();
    void slot_zoomOut();
    void slot_zoomReset();
    void slot_dataLoaded();
    void slot_moveMarker(const RoomId id);
    void slot_mapChanged();
    void slot_requestUpdate();
    void slot_clearAllSelections();
    void slot_clearRoomSelection();
    void slot_clearConnectionSelection();
    void slot_clearInfomarkSelection();
    void slot_rebuildMeshes();
    void slot_onMessageLoggedDirect(const QOpenGLDebugMessage &message);
    void slot_onForcedPositionChange();

signals:
    void sig_zoomChanged(float zoom);
    void sig_newRoomSelection(const SigRoomSelection &selection);
    void sig_newConnectionSelection(ConnectionSelection *selection);
    void sig_newInfomarkSelection(InfomarkSelection *selection);
    void sig_onCenter(const glm::vec2 &pos);
    void sig_setScrollBars(const Coordinate &min, const Coordinate &max);
    void sig_continuousScroll(int dx, int dy);
    void sig_mapMove(int dx, int dy);
    void sig_selectionChanged();
    void sig_log(const QString &category, const QString &message);
    void sig_setCurrentRoom(RoomId id, bool update);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    bool event(QEvent *e) override;

private:
    void actuallyPaintGL();
    void paintCharacters();
    void drawGroupCharacters(CharacterBatch &batch);
    void paintSelectedRoom(RoomSelFakeGL &gl, const RawRoom &room);
    void paintSelectedRooms();
    void paintSelectedConnection();
    void paintSelectedInfomarks();
    void paintNewInfomarkSelection();
    void paintBatchedInfomarks();
    void updateInfomarkBatches();
    void paintNearbyConnectionPoints();
    void drawInfomark(InfomarksBatch &batch,
                     const InfomarkHandle &marker,
                     int layer,
                     const glm::vec2 &offset = {},
                     const std::optional<Color> &overrideColor = std::nullopt);
    NODISCARD BatchedInfomarksMeshes getInfomarksMeshes();
    void initTextures();
    void updateTextures();
    void onMovement();
    void infomarksChanged();
    void layerChanged();
    void forceUpdateMeshes();
    void selectionChanged();
    void cleanupOpenGL();
    void mapLog(const QString &message) { emit sig_log("Map", message); }
    void reportGLVersion();
    NODISCARD bool isBlacklistedDriver();
    void setAnimating(bool value);
    void renderLoop();
    void initLogger();
    void updateMultisampling();
    void setMvp(const glm::mat4 &viewProj);
    void setViewportAndMvp(int width, int height);
    void updateBatches();
    void finishPendingMapBatches();
    void updateMapBatches();
    void paintMap();
    void renderMapBatches();
    void paintSelections();
    void paintSelectionArea();
    void paintDifferences();
    void log(const QString &msg) { emit sig_log("MapCanvas", msg); }

    std::shared_ptr<InfomarkSelection> getInfomarkSelection(const MouseSel &sel);
    NODISCARD static glm::mat4 getViewProj_old(const glm::vec2 &scrollPos, const glm::ivec2 &size, float zoomScale, int currentLayer);
    NODISCARD static glm::mat4 getViewProj(const glm::vec2 &scrollPos, const glm::ivec2 &size, float zoomScale, int currentLayer);
    NODISCARD Viewport getViewport() const { return m_opengl.getViewport(); }

private:
    NODISCARD OpenGL &getOpenGL() { return m_opengl; }
    NODISCARD const OpenGL &getOpenGL() const { return m_opengl; }
    NODISCARD GLFont &getGLFont() { return m_glFont; }

private:
    MapScreen m_mapScreen;
    OpenGL m_opengl;
    GLFont m_glFont;
    Batches m_batches;
    MapCanvasTextures m_textures;
    MapData &m_data;
    Mmapper2Group &m_groupManager;
    Diff m_diff;
    FrameRateController m_frameRateController;
    std::unique_ptr<QOpenGLDebugLogger> m_logger;
    Signal2Lifetime m_lifetime;

    std::unique_ptr<MapCanvasViewModel> m_viewModel;
    QTimer m_continuousScrollTimer;
};
