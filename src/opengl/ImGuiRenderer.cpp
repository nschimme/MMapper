#include "ImGuiRenderer.h"

#include "../display/MapCanvasData.h"

#include <cmath>
#include <imgui.h>

#include <backends/imgui_impl_opengl3.h>

#include <QDateTime>
#include <QFile>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLWindow>
#include <QWheelEvent>

ImGuiRenderer::ImGuiRenderer(QOpenGLWindow *window)
    : m_window(window)
{}

ImGuiRenderer::~ImGuiRenderer()
{
    if (m_initialized) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext();
    }
}

void ImGuiRenderer::initialize()
{
    if (m_initialized) {
        return;
    }

    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Use the GLSL version based on the context
    const char *glsl_version = "#version 130";
    if (m_window->context()->format().renderableType() == QSurfaceFormat::OpenGLES) {
        glsl_version = "#version 300 es";
    }

    ImGui_ImplOpenGL3_Init(glsl_version);

    updateDPI();

    m_initialized = true;
    io.BackendPlatformName = "imgui_impl_qt";
}

void ImGuiRenderer::updateDPI()
{
    ImGuiIO &io = ImGui::GetIO();
    float dpr = static_cast<float>(m_window->devicePixelRatio());

    io.Fonts->Clear();

    auto loadFont = [&](const QString &path, float size) -> ImFont * {
        QFile fontFile(path);
        if (fontFile.open(QIODevice::ReadOnly)) {
            QByteArray fontData = fontFile.readAll();
            void *data = ImGui::MemAlloc(static_cast<size_t>(fontData.size()));
            std::memcpy(data, fontData.constData(), static_cast<size_t>(fontData.size()));
            return io.Fonts->AddFontFromMemoryTTF(data, static_cast<int>(fontData.size()), size);
        }
        return nullptr;
    };

    m_fontRegular = loadFont(":/fonts/Cantarell-Regular.ttf", 18.0f * dpr);
    m_fontItalic = loadFont(":/fonts/Cantarell-Italic.ttf", 18.0f * dpr);

    if (!m_fontRegular) {
        m_fontRegular = io.Fonts->AddFontDefault();
    }
    if (!m_fontItalic) {
        m_fontItalic = m_fontRegular;
    }

    // Set global scale to inverse of DPR so we can work in logical pixels
    io.FontGlobalScale = 1.0f / dpr;

    // We must re-create the font texture
    ImGui_ImplOpenGL3_CreateFontsTexture();
}

void ImGuiRenderer::newFrame()
{
    if (!m_initialized) {
        return;
    }

    if (m_dpiDirty) {
        updateDPI();
        m_dpiDirty = false;
    }

    ImGuiIO &io = ImGui::GetIO();

    // Update display size
    io.DisplaySize = ImVec2(static_cast<float>(m_window->width()),
                            static_cast<float>(m_window->height()));
    io.DisplayFramebufferScale = ImVec2(static_cast<float>(m_window->devicePixelRatio()),
                                        static_cast<float>(m_window->devicePixelRatio()));

    // Update time
    static qint64 lastTime = QDateTime::currentMSecsSinceEpoch();
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    io.DeltaTime = static_cast<float>(currentTime - lastTime) / 1000.0f;
    if (io.DeltaTime <= 0.0f) {
        io.DeltaTime = 0.00001f;
    }
    lastTime = currentTime;

    updateModifiers();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiRenderer::render()
{
    if (!m_initialized) {
        return;
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

static void AddTextRotated(ImDrawList *drawList,
                           ImFont *font,
                           float fontSize,
                           ImVec2 pos,
                           ImU32 col,
                           const char *text,
                           float angleDegrees,
                           bool center)
{
    float angle = angleDegrees * (static_cast<float>(M_PI) / 180.0f);
    ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
    ImVec2 offset(0, 0);
    if (center) {
        offset.x = -textSize.x * 0.5f;
        offset.y = -textSize.y * 0.5f;
    }

    if (angle == 0.0f) {
        drawList->AddText(font, fontSize, ImVec2(pos.x + offset.x, pos.y + offset.y), col, text);
        return;
    }

    size_t vtxBufSizeBefore = static_cast<size_t>(drawList->VtxBuffer.Size);
    drawList->AddText(font, fontSize, ImVec2(pos.x + offset.x, pos.y + offset.y), col, text);
    size_t vtxBufSizeAfter = static_cast<size_t>(drawList->VtxBuffer.Size);

    if (vtxBufSizeAfter > vtxBufSizeBefore) {
        float s = std::sin(angle);
        float c = std::cos(angle);
        for (size_t i = vtxBufSizeBefore; i < vtxBufSizeAfter; ++i) {
            ImDrawVert &v = drawList->VtxBuffer[static_cast<int>(i)];
            float x = v.pos.x - pos.x;
            float y = v.pos.y - pos.y;
            v.pos.x = pos.x + (x * c - y * s);
            v.pos.y = pos.y + (x * s + y * c);
        }
    }
}

void ImGuiRenderer::draw2dText(const std::vector<GLText> &text)
{
    if (text.empty() || !m_initialized) {
        return;
    }

    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##2DText",
                 nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                     | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    float dpr = static_cast<float>(m_window->devicePixelRatio());

    for (const auto &t : text) {
        ImVec2 pos(t.pos.x / dpr, t.pos.y / dpr);
        ImFont *font = t.fontFormatFlag.contains(FontFormatFlagEnum::ITALICS) ? m_fontItalic
                                                                              : m_fontRegular;
        float fontSize = font->FontSize;

        const std::string &label = t.text;
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label.c_str());

        if (t.fontFormatFlag.contains(FontFormatFlagEnum::HALIGN_CENTER)) {
            pos.x -= textSize.x * 0.5f;
        } else if (t.fontFormatFlag.contains(FontFormatFlagEnum::HALIGN_RIGHT)) {
            pos.x -= textSize.x;
        }

        if (t.bgcolor) {
            const glm::vec4 bg = t.bgcolor->getVec4();
            drawList->AddRectFilled(ImVec2(pos.x - 2, pos.y - 2),
                                    ImVec2(pos.x + textSize.x + 2, pos.y + textSize.y + 2),
                                    ImGui::ColorConvertFloat4ToU32(ImVec4(bg.r, bg.g, bg.b, bg.a)));
        }

        const glm::vec4 fg = t.color.getVec4();
        drawList->AddText(font,
                          fontSize,
                          pos,
                          ImGui::ColorConvertFloat4ToU32(ImVec4(fg.r, fg.g, fg.b, fg.a)),
                          label.c_str());

        if (t.fontFormatFlag.contains(FontFormatFlagEnum::UNDERLINE)) {
            drawList->AddLine(ImVec2(pos.x, pos.y + textSize.y),
                              ImVec2(pos.x + textSize.x, pos.y + textSize.y),
                              ImGui::ColorConvertFloat4ToU32(ImVec4(fg.r, fg.g, fg.b, fg.a)));
        }
    }

    ImGui::End();
}

void ImGuiRenderer::draw3dText(const MapCanvasViewport &viewport, const std::vector<GLText> &text)
{
    if (text.empty() || !m_initialized) {
        return;
    }

    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##3DText",
                 nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground
                     | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    float height = static_cast<float>(m_window->height());

    for (const auto &t : text) {
        auto optProjected = viewport.project(t.pos);
        if (optProjected) {
            ImVec2 pos(optProjected->x, height - optProjected->y);
            ImFont *font = t.fontFormatFlag.contains(FontFormatFlagEnum::ITALICS) ? m_fontItalic
                                                                                  : m_fontRegular;
            float fontSize = font->FontSize;

            const std::string &label = t.text;
            ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label.c_str());

            bool center = t.fontFormatFlag.contains(FontFormatFlagEnum::HALIGN_CENTER);
            if (!center && t.fontFormatFlag.contains(FontFormatFlagEnum::HALIGN_RIGHT)) {
                pos.x -= textSize.x;
            }

            if (t.bgcolor) {
                const glm::vec4 bg = t.bgcolor->getVec4();
                float ox = center ? -textSize.x * 0.5f : 0.0f;
                drawList->AddRectFilled(ImVec2(pos.x + ox - 2, pos.y - 2),
                                        ImVec2(pos.x + ox + textSize.x + 2, pos.y + textSize.y + 2),
                                        ImGui::ColorConvertFloat4ToU32(
                                            ImVec4(bg.r, bg.g, bg.b, bg.a)));
            }

            const glm::vec4 fg = t.color.getVec4();
            ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(fg.r, fg.g, fg.b, fg.a));

            AddTextRotated(drawList,
                           font,
                           fontSize,
                           pos,
                           col,
                           label.c_str(),
                           static_cast<float>(t.rotationAngle),
                           center);

            if (t.fontFormatFlag.contains(FontFormatFlagEnum::UNDERLINE)) {
                float ox = center ? -textSize.x * 0.5f : 0.0f;
                drawList->AddLine(ImVec2(pos.x + ox, pos.y + textSize.y),
                                  ImVec2(pos.x + ox + textSize.x, pos.y + textSize.y),
                                  col);
            }
        }
    }

    ImGui::End();
}

void ImGuiRenderer::updateModifiers()
{
    ImGuiIO &io = ImGui::GetIO();
    Qt::KeyboardModifiers modifiers = QGuiApplication::queryKeyboardModifiers();
    io.AddKeyEvent(ImGuiMod_Ctrl, modifiers.testFlag(Qt::ControlModifier));
    io.AddKeyEvent(ImGuiMod_Shift, modifiers.testFlag(Qt::ShiftModifier));
    io.AddKeyEvent(ImGuiMod_Alt, modifiers.testFlag(Qt::AltModifier));
    io.AddKeyEvent(ImGuiMod_Super, modifiers.testFlag(Qt::MetaModifier));
}

bool ImGuiRenderer::handleMouseEvent(QMouseEvent *event)
{
    if (!m_initialized) {
        return false;
    }
    ImGuiIO &io = ImGui::GetIO();

    io.AddMousePosEvent(static_cast<float>(event->position().x()),
                        static_cast<float>(event->position().y()));

    int button = -1;
    if (event->button() == Qt::LeftButton) {
        button = 0;
    } else if (event->button() == Qt::RightButton) {
        button = 1;
    } else if (event->button() == Qt::MiddleButton) {
        button = 2;
    }

    if (button != -1) {
        if (event->type() == QEvent::MouseButtonPress) {
            io.AddMouseButtonEvent(button, true);
        } else if (event->type() == QEvent::MouseButtonRelease) {
            io.AddMouseButtonEvent(button, false);
        }
    }

    return io.WantCaptureMouse;
}

bool ImGuiRenderer::handleWheelEvent(QWheelEvent *event)
{
    if (!m_initialized) {
        return false;
    }
    ImGuiIO &io = ImGui::GetIO();

    QPoint delta = event->angleDelta();
    io.AddMouseWheelEvent(static_cast<float>(delta.x()) / 120.0f,
                          static_cast<float>(delta.y()) / 120.0f);

    return io.WantCaptureMouse;
}

bool ImGuiRenderer::handleKeyEvent(QKeyEvent *event)
{
    if (!m_initialized) {
        return false;
    }
    ImGuiIO &io = ImGui::GetIO();

    if (event->type() == QEvent::KeyPress) {
        QString text = event->text();
        if (!text.isEmpty()) {
            io.AddInputCharactersUTF8(text.toUtf8().constData());
        }
    }

    // Map some common keys
    ImGuiKey key = ImGuiKey_None;
    switch (event->key()) {
    case Qt::Key_Tab:
        key = ImGuiKey_Tab;
        break;
    case Qt::Key_Left:
        key = ImGuiKey_LeftArrow;
        break;
    case Qt::Key_Right:
        key = ImGuiKey_RightArrow;
        break;
    case Qt::Key_Up:
        key = ImGuiKey_UpArrow;
        break;
    case Qt::Key_Down:
        key = ImGuiKey_DownArrow;
        break;
    case Qt::Key_PageUp:
        key = ImGuiKey_PageUp;
        break;
    case Qt::Key_PageDown:
        key = ImGuiKey_PageDown;
        break;
    case Qt::Key_Home:
        key = ImGuiKey_Home;
        break;
    case Qt::Key_End:
        key = ImGuiKey_End;
        break;
    case Qt::Key_Insert:
        key = ImGuiKey_Insert;
        break;
    case Qt::Key_Delete:
        key = ImGuiKey_Delete;
        break;
    case Qt::Key_Backspace:
        key = ImGuiKey_Backspace;
        break;
    case Qt::Key_Space:
        key = ImGuiKey_Space;
        break;
    case Qt::Key_Enter:
        key = ImGuiKey_Enter;
        break;
    case Qt::Key_Return:
        key = ImGuiKey_Enter;
        break;
    case Qt::Key_Escape:
        key = ImGuiKey_Escape;
        break;
    default:
        break;
    }

    if (key != ImGuiKey_None) {
        io.AddKeyEvent(key, event->type() == QEvent::KeyPress);
    }

    return io.WantCaptureKeyboard;
}
