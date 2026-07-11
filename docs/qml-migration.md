# QML Migration

## Goal

MMapper's UI is historically built on Qt Widgets. We are incrementally migrating
individual panels to Qt Quick / QML, with two end goals:

- A mobile- and touch-friendly UI (Quick's input model and styling are far better
  suited to touch than Widgets).
- A mobile-capable Web client: the WebAssembly build currently ships the desktop
  Widgets UI, which does not work well on phones/tablets in a browser. QML gets us
  there without a second UI implementation.

The migration is deliberately incremental: each panel is ported independently, the
Widgets equivalent keeps working the whole time via a build-time switch, and the map
canvas (the most complex part of the UI) is intentionally left untouched until last.

## Build: `WITH_QML`

`WITH_QML` is a CMake option (`CMakeLists.txt`), default `ON` on every platform,
including `wasm`. It is an escape hatch, not a long-term configuration users are
expected to pick: turn it `OFF` only if your Qt installation lacks the Quick modules,
or temporarily while debugging a QML-related build issue.

When `ON`:

- `find_package(Qt6 COMPONENTS ... Qml Quick QuickWidgets QuickControls2)` is added
  to the component list, and the build fails fast with a clear message if those
  modules aren't installed.
- The `MMAPPER_WITH_QML` compile definition is added globally (`add_compile_definitions`
  in the top-level `CMakeLists.txt`), so it's visible to every target, including tests.
- `src/CMakeLists.txt` builds the `mm_qml` static Qt Quick module and links it into
  `mmapper`.

When `OFF`, the legacy Widgets panels (`TimerWidget`, `AdventureWidget`, `RoomWidget`)
are compiled instead; see "Ported panels" below.

Debian/Ubuntu runtime packages needed for `WITH_QML=ON`:

```
qt6-declarative-dev qml6-module-qtquick qml6-module-qtquick-controls \
qml6-module-qtquick-templates qml6-module-qtquick-window qml6-module-qtqml \
qml6-module-qtqml-workerscript
```

(see `.github/workflows/build-test.yml` for the exact list used by CI).

## Architecture

### `mm_qml` module

QML panels live in `src/qml/` and are compiled into a static Qt Quick module named
`MMapper` via `qt_add_qml_module(mm_qml ...)` in `src/CMakeLists.txt`. Each `.qml`
file is loaded at runtime from `qrc:/qt/qml/MMapper/<File>.qml`.

Important: every `QML_FILES` entry needs an explicit
`set_source_files_properties(qml/Foo.qml PROPERTIES QT_RESOURCE_ALIAS "Foo.qml")`
line. Without it, `qt_add_qml_module` derives the resource alias from the file's path
relative to `src/CMakeLists.txt` (e.g. `qml/Foo.qml`), so the file would only be
loadable as `qrc:/qt/qml/MMapper/qml/Foo.qml` — not the flat `.../MMapper/Foo.qml`
URL the rest of the code expects. **When adding a new `.qml` file, add both the
`QML_FILES` entry and the matching `set_source_files_properties` alias line.**

### `QmlDockWidget`

`src/qml/QmlDockWidget.{h,cpp}` wraps a `QQuickWidget` inside a `QDockWidget` and is
the standard way to embed a QML panel in the main window. Usage pattern
(see `src/mainwindow/mainwindow.cpp`):

```cpp
auto *const dock = new QmlDockWidget(tr("Timers Panel"), "DockWidgetTimers", this);
dock->setContextProperty("timerModel", model);       // context properties FIRST
dock->setContextProperty("timerController", controller);
dock->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/TimerPanel.qml"))); // source LAST
```

Context properties must be set **before** `setQmlSource()`: the QML root context has
to have every property populated before the root object is instantiated, or bindings
that reference those properties at construction time silently fail.

`QmlDockWidget` also has a built-in error fallback: if the QML file fails to load
(syntax error, missing type, etc.), it logs the QML errors and replaces the
`QQuickWidget` with a plain `QLabel("QML load failed")` instead of crashing or
showing a blank dock. This is deliberate defense against a bad `.qml` file taking
down the whole application.

### Startup GL pinning (`main.cpp`)

When `MMAPPER_WITH_QML` is defined, `main()` pins Qt Quick to the OpenGL RHI backend
before `QApplication` is constructed:

```cpp
QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
QQuickStyle::setStyle("Fusion");
```

This matters for two reasons:

1. `QQuickWidget` needs a GL-based RHI backend to embed correctly inside a Widgets
   window on all our target platforms; letting Qt auto-select (which can pick
   Vulkan/Metal/Direct3D depending on platform) is not reliable here.
2. MMapper's map canvas uses a custom GL 3.3 / GLES 3.0 renderer
   (`OpenGLProber`/`OpenGLConfig`). Pinning Quick to OpenGL keeps both renderers on
   the same GL context family, which is required for `AA_ShareOpenGLContexts` to do
   anything useful and keeps the door open for sharing a context between the map
   canvas and a future Quick-based overlay (see "Map canvas" below).

### Qt 6.4 compatibility

CI's Ubuntu job builds against the distro-packaged Qt6, which is old enough to act as
our de facto minimum-version guard (Qt 6.4, per `CMakeLists.txt`'s
`Qt6Core_VERSION VERSION_LESS 6.4.0` check). QML code must stay compatible:

- Use unversioned imports (`import QtQuick`, `import QtQuick.Controls`), never
  `import QtQuick 2.15` or similar — versioned imports pin to a specific module
  version that may not exist on 6.4.
- Don't use `pragma` directives or QML features introduced in Qt 6.5+.
- If you're unsure whether something is 6.4-safe, the Ubuntu CI leg is the ground
  truth — if it builds and runs there, it's fine.

## Theming

QML panels use `PanelFrame.qml` (`src/qml/PanelFrame.qml`) as their root item. It
wraps content in a `Rectangle` backed by a `SystemPalette` bound to
`SystemPalette.Active`, which tracks the live `QApplication` palette — including
`ThemeManager`'s dark/light switches — with no manual replumbing needed. Panels
expose their content via the `content` default property.

Prefer plain `QtQuick` primitives (`Rectangle`, `Text`, `Item`, `ListView`, ...) over
`QtQuick.Controls` styled widgets where practical. Primitives are simpler, cheaper,
and don't drag in a styling layer we'd otherwise have to theme separately; use
Controls types only where there's no reasonable primitive substitute (e.g. `Button`,
`ScrollBar`).

## Ported panels

| Panel | QML | Model/controller | Widget (WITH_QML=OFF only) |
|---|---|---|---|
| Timers | `src/qml/TimerPanel.qml` | `timers/TimerModel.{h,cpp}` (roles) + `timers/TimerController.{h,cpp}` | `timers/TimerWidget.{h,cpp}`, `timers/TimerDelegate.{h,cpp}` |
| Adventure | `src/qml/AdventurePanel.qml` | `adventure/AdventureLogModel.{h,cpp}` | `adventure/adventurewidget.{h,cpp}` |
| Room | `src/qml/RoomPanel.qml` | `roompanel/RoomModel.{h,cpp}` (roles) | `roompanel/RoomWidget.{h,cpp}` |

The Widget classes are excluded from the build entirely when `WITH_QML=ON`
(`src/CMakeLists.txt` only appends them to `mmapper_SRCS` under `if(NOT WITH_QML)`).
They exist solely as the `WITH_QML=OFF` escape hatch and are planned for deletion
about one release cycle after the WebAssembly build ships with QML, once we're
confident nobody needs to fall back to Widgets.

## Map canvas (future work, not started)

`MapCanvas`/`MapWindow` are untouched by this migration and are the largest, riskiest
piece of remaining work. The current end-state plan:

- Render the map as a `QQuickWindow` underlay: draw the existing GL scene from
  `beforeRenderPassRecording` (or equivalent RHI hook) so the custom renderer keeps
  full control of the GL state, with Quick items composited on top/around it.
- Move mouse/touch input handling to Quick pointer handlers (`PointerHandler`,
  `TapHandler`, etc.), driven by the existing `CanvasMouseModeEnum` state machine
  rather than duplicating it.
- Interim fallback if the underlay approach proves too invasive: render into a
  `QQuickFramebufferObject` instead. Simpler to integrate, at the cost of an extra
  offscreen composite pass.
- `QSGRenderNode` (drawing custom GL directly inside the Quick scene graph) was
  considered and rejected: restoring the custom renderer's GL state correctly after
  a `QSGRenderNode::render()` call is fragile and error-prone across GL driver
  quirks, and a bug there would corrupt map rendering silently.

This work has not started; MapCanvas/MapWindow require no changes for the panels
ported so far.

## Config binding (future work, not started)

Panels ported so far read `getConfig()` directly and rely on Qt's `ChangeMonitor`
callbacks for live updates, same as their Widgets predecessors. If/when a panel needs
to read or write configuration reactively from QML, the plan is a thin `Q_PROPERTY`
façade object wrapping `getConfig()`/`ChangeMonitor` (`NOTIFY` signal fired from the
existing change callback), exposed as a context property — not a rewrite of the
configuration system itself.

## How to add a new QML panel

Using the Timers panel (`src/qml/TimerPanel.qml`,
`src/timers/TimerModel.{h,cpp}`, `src/timers/TimerController.{h,cpp}`) as the
reference example:

1. Write (or reuse) a `QAbstractListModel`/`QObject` exposing the data as roles and
   `Q_INVOKABLE` methods/`Q_PROPERTY`s as needed (`TimerModel`, `TimerController`).
2. Add the new `.qml` file under `src/qml/`, rooted in `PanelFrame` for consistent
   theming, using only Qt 6.4-safe, unversioned imports.
3. In `src/CMakeLists.txt`, add the new file to `mm_qml`'s `QML_FILES` list **and**
   add the matching `set_source_files_properties(... QT_RESOURCE_ALIAS "Foo.qml")`
   line — both are required, see "`mm_qml` module" above.
4. In `src/mainwindow/mainwindow.cpp`, wire it up inside an
   `#ifdef MMAPPER_WITH_QML` / `#else` pair alongside the existing Widgets dock: build
   the model(s), create a `QmlDockWidget`, set context properties, then
   `setQmlSource("qrc:/qt/qml/MMapper/Foo.qml")`. Keep the `#else` branch building the
   Widgets equivalent so `WITH_QML=OFF` keeps working.
5. Guard any forward declarations/includes/members for the retired Widgets class
   with `#ifndef MMAPPER_WITH_QML` in `mainwindow.h`/`mainwindow.cpp`, and add the
   Widgets `.cpp`/`.h` to the `if(NOT WITH_QML)` block in `src/CMakeLists.txt` rather
   than the unconditional source list.
