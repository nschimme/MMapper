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

### Startup graphics API pinning (`main.cpp`)

When `MMAPPER_WITH_QML` is defined, `main()` pins Qt Quick to the software scene-graph
backend before `QApplication` is constructed:

```cpp
QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
QQuickStyle::setStyle("Fusion");
```

This was not the original design. Phase 1/2 pinned Quick to the OpenGL RHI backend
instead (plus `Qt::AA_ShareOpenGLContexts`), on the theory that `QQuickWidget` needs a
GL-based RHI backend to embed cleanly, and that keeping Quick and the map canvas's
custom GL 3.3 / GLES 3.0 renderer (`OpenGLProber`/`OpenGLConfig`) on the same context
family would let them eventually share a context.

That combination turned out to be broken on macOS: Qt 6.4 widget backing stores
composite through Metal/CoreAnimation, not GL. Forcing the QQuickWidget dock panels
onto the GL RHI backend gave each panel its own QOpenGLContext, and once the same
top-level window also hosted the map's native `QOpenGLWindow` (via
`createWindowContainer`), the two GL-vs-Metal compositing paths fought over the
window's backing store. The visible symptom was severe artifacting: the native GL map
rendered fine, but the surrounding widget backing store was left as uninitialized
garbage, and the QML dock panels only partially composited on top of it.

The software backend avoids the conflict entirely: each `QQuickWidget` renders its
scene into a plain `QImage` and blits it into the widget backing store, with no GL
context involved at all. `Qt::AA_ShareOpenGLContexts` is no longer needed either — it
only mattered for sharing a GL-backed Quick context with the map canvas — so it has
been removed. The panels are simple 2D UI (lists, text, buttons), so software
rendering is not a meaningful performance cost.

This is specific to the current `QQuickWidget`-in-a-Widgets-window architecture. Once
the map canvas becomes a `QQuickWindow` scene-graph underlay (the phase described
under "Map canvas" below), there is no more `QQuickWidget` and no more raw
`QOpenGLWindow` sibling for it to conflict with — the whole top-level becomes a single
Quick scene graph, and pinning back to a hardware-accelerated graphics API (OpenGL,
Metal, or Vulkan/Direct3D via RHI auto-selection) will be revisited at that point.

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
| Log | `src/qml/LogPanel.qml` | `mainwindow/LogModel.{h,cpp}` (roles, 10k-line cap) | *(none — inline `QTextBrowser`)* |
| Group | `src/qml/GroupPanel.qml` | `group/GroupModel.{h,cpp}` (roles) + `group/GroupController.{h,cpp}` + `qml/GroupIconProvider.{h,cpp}` | `group/groupwidget.{h,cpp}` |
| Description | `src/qml/DescriptionPanel.qml` | `media/DescriptionAdapter.{h,cpp}` + `media/DescriptionImageStore.h` + `qml/DescriptionImageProvider.{h,cpp}` | `media/DescriptionWidget.{h,cpp}` |
| Tasks | `src/qml/TasksPanel.qml` | `mainwindow/TasksModel.{h,cpp}` (roles, poll/diff) | `mainwindow/TasksPanel.{h,cpp}` |
| Status bar: clock | `src/qml/ClockStrip.qml` | `clock/ClockAdapter.{h,cpp}` + `clock/ClockStrings.{h,cpp}` (shared) | `clock/mumeclockwidget.{h,cpp,ui}` |
| Status bar: XP status | `src/qml/XpStatusItem.qml` | `adventure/XpStatusAdapter.{h,cpp}` | `adventure/xpstatuswidget.{h,cpp}` |
| Client | `src/qml/ClientPanel.qml` + `src/qml/ClientDisplay.qml` | `client/ClientController.{h,cpp}` + `client/ClientLineModel.{h,cpp}` + `client/ClientTelnetBackend.{h,cpp}` | `client/ClientWidget.{h,cpp,ui}`, `client/inputwidget.{h,cpp}`, `client/stackedinputwidget.{h,cpp}`, `client/PasswordDialog.{h,cpp}`, `client/PreviewWidget.{h,cpp}`, `client/PaletteManager.{h,cpp}` |
| About dialog | `src/qml/AboutDialog.qml` | `mainwindow/AboutInfo.{h,cpp}` (incl. `LicenseModel`) | `mainwindow/aboutdialog.{h,cpp,ui}` |
| Updater dialog | `src/qml/UpdateDialog.qml` | `mainwindow/UpdateChecker.{h,cpp}` | `mainwindow/UpdateDialog.{h,cpp}` |
| Find Rooms dialog | `src/qml/FindRoomsDialog.qml` | `mainwindow/FindRoomsController.{h,cpp}` + `mainwindow/FindRoomsModel.{h,cpp}` | `mainwindow/findroomsdlg.{h,cpp,ui}` |
| Preferences dialog | `src/qml/PreferencesDialog.qml` + 9 `src/qml/Prefs*Page.qml` | `preferences/PreferencesController.{h,cpp}` + 9 `preferences/*PageAdapter.{h,cpp}` | `preferences/configdialog.{h,cpp,ui}` + 9 widget pages (`audiopage`, `autologpage`, `clientpage`, `generalpage`, `graphicspage` (+`AdvancedGraphics.{h,cpp}`), `grouppage`, `mumeprotocolpage`, `parserpage`, `pathmachinepage`, each `.{h,cpp,ui}`) |

The Widget classes are excluded from the build entirely when `WITH_QML=ON`
(`src/CMakeLists.txt` only appends them to `mmapper_SRCS` under `if(NOT WITH_QML)`).
As of phase 5 this list covers `TimerWidget`/`TimerDelegate`, `adventurewidget`,
`RoomWidget`, `groupwidget`, `DescriptionWidget`, `TasksPanel`, `MumeClockWidget`
(+ its `.ui`), `XPStatusWidget`, and the Client panel's six Widget files —
`ClientWidget` (+ its `.ui`), `inputwidget`, `stackedinputwidget`,
`PasswordDialog`, `PreviewWidget`, and `PaletteManager` (used only by
`inputwidget`). The Log panel has no legacy widget class to
retire — it replaces an inline `QTextBrowser` that was created directly in
`mainwindow.cpp`, not a separate widget file. The status bar's two widgets are
always-on, non-dockable status bar contents rather than `QDockWidget` panels (see
"Status bar" under "Phase 3 notes" below), but otherwise follow the same
`WITH_QML`-gated Widget-vs-adapter split as every other row in this table. As of
phase 6 this also covers the four top-level dialogs: `aboutdialog` (+ its
`.ui`), `UpdateDialog`, `findroomsdlg` (+ its `.ui`), and `configdialog`
together with all nine preferences page widgets — `audiopage`, `autologpage`,
`clientpage`, `generalpage`, `graphicspage` (+ its `AdvancedGraphics` helper),
`grouppage`, `mumeprotocolpage`, `parserpage`, and
`pathmachinepage` — each `.cpp`/`.h`/`.ui` (`AdvancedGraphics` has no `.ui` of
its own). Two preferences-adjacent widgets, `AnsiColorDialog` and
`ManagePasswordDialog`, stayed compiled unconditionally in both builds
through phase 6 (see "Preferences" under "Phase 6 notes" below for the
original reasoning); both were ported to QML in phase 9 / Stage 1 (see
"Phase 9 / Stage 1" below) and are now `if(NOT WITH_QML)`-gated like every
other row in this table. All of
the Widget classes exist solely as the `WITH_QML=OFF` escape hatch and are planned
for deletion about one release cycle after the WebAssembly build ships with QML,
once we're confident nobody needs to fall back to Widgets.

`client/displaywidget.{h,cpp}` stayed compiled unconditionally (never moved
into the `if(NOT WITH_QML)` block with the rest of the client) through
phase 8 because `AnsiTextHelper`, defined there, is a general-purpose ANSI
text renderer consumed outside the client entirely — `viewers/AnsiViewWindow.cpp`
and `mpi/remoteeditwidget.cpp` used it regardless of `WITH_QML`. Phase 8
moved its cursor-driven core into the widget-free `global/AnsiHtml.{h,cpp}`
as `mmqt::AnsiTextToDocument`/`mmqt::ansiToHtml()`, with `AnsiTextHelper`
reduced to a thin `QTextEdit` wrapper over it; phase 9 / Stage 1 then ported
both of `displaywidget`'s remaining outside consumers off it (`AnsiViewWindow`
via `makeAnsiViewWindow()`'s QmlDialog path, `remoteeditwidget` via the new
QML remote editor), so `client/displaywidget.{h,cpp}` -- along with
`mpi/remoteeditwidget.{h,cpp}`, `mpi/findreplacewidget.{h,cpp}`, and
`mpi/gotowidget.{h,cpp}` -- is now `if(NOT WITH_QML)`-gated too (see "Phase 9
/ Stage 1" below). `client/ClientTelnet.{h,cpp}` still stays
unconditional: it's the real telnet socket implementation that
`ClientTelnetBackend` wraps, so both builds need it.

## Phase 2 notes

Phase 2 ported the Log, Group, and Description panels, fixed a column-sizing bug in
the Room panel, and introduced two new pieces of shared infrastructure: a config
façade (`QmlConfig`) and per-engine image provider registration. No new QML runtime
module dependencies were introduced (see "Packaging" below).

### RoomPanel column width fix

The Room panel's `TableView` originally let columns size themselves, which on Qt 6.4
collapses to a small fixed default width regardless of content — Qt 6.5 added
`TableView.explicitColumnWidth`/`resizableColumns` to address this properly, but
those aren't available on the Qt 6.4 floor this project builds against (see "Qt 6.4
compatibility" above). The fix (`src/qml/RoomPanel.qml`) is a manual
`columnWidthProvider` function that measures the pixel width of both the column's
header text and its widest cell text (via a `TextMetrics` item and a new
`RoomModel::longestTextInColumn(col)` helper in `roompanel/RoomModel.{h,cpp}`), and
returns `max(headerWidth, contentWidth) + margin`, floored at a minimum width. This
is the pattern to reach for any time a `TableView`/`HorizontalHeaderView` pairing
needs content-driven column widths on Qt 6.4.

### `QmlConfig` façade

`src/qml/QmlConfig.h` is a `Q_PROPERTY` façade wrapping the `groupManager` subgroup
of `Configuration` (`npcHide`, `npcSortBottom`, `groupColor`), exposed to QML as the
`config` context property consumed by `GroupPanel.qml`. Unlike most `Configuration`
subgroups, `GroupManagerSettings` is a plain struct with no `ChangeMonitor` — it's a
bag of public fields written directly by callers (most notably the preferences
dialog), so there is no callback `QmlConfig` can hook to observe writes it didn't
make itself. The contract:

- Each setter (`setNpcHide`, `setNpcSortBottom`, `setGroupColor`) writes
  `getConfig()` and emits its `*Changed` signal itself; QML sees its own writes
  immediately.
- `reload()` re-syncs the cached values against the live `Configuration` and emits
  whichever `*Changed` signals differ. `MainWindow::slot_onPreferences()` wires this
  to `ConfigDialog::sig_groupSettingsChanged` and to the dialog's `finished` signal,
  so anything the preferences dialog wrote lands in QML as soon as the dialog
  signals a change or closes.
- Any other code path that writes `setConfig().groupManager.*` directly, outside of
  `QmlConfig`'s own setters and without going through a hook that calls `reload()`,
  leaves QML holding stale values until the next `reload()`. This is a deliberate,
  documented limitation (see the class comment in `QmlConfig.h`), not a bug — there
  is no config-system-wide change notification to hang a fix off yet (see "Config
  binding" above, which is still future work for subgroups that already have
  `ChangeMonitor`).

The Description panel's colors/font (`parser.roomName`/`DescColor`,
`integratedClient.font`/colors) have the same no-`ChangeMonitor` shape and are
handled the same way: `DescriptionAdapter::reloadConfig()` is called from the same
`ConfigDialog::finished` handler in `mainwindow.cpp`.

### Per-engine image providers

Every `QmlDockWidget` wraps its own `QQuickWidget`, and every `QQuickWidget` owns its
own `QQmlEngine` — image providers are registered per-engine, not globally, so each
panel that needs one must register it on its own dock. `QmlDockWidget::addImageProvider`
(`src/qml/QmlDockWidget.{h,cpp}`) does this:

```cpp
dock->addImageProvider("groupicons", new GroupIconProvider());
...
dock->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/GroupPanel.qml")));
```

Like `setContextProperty`, `addImageProvider` must be called **before**
`setQmlSource()` — the provider has to be registered with the engine before the QML
loads and issues `image://...` requests against it. The `QQmlEngine` takes ownership
of the provider pointer.

Two image providers were added in phase 2:

- `GroupIconProvider` (`src/qml/GroupIconProvider.{h,cpp}`), registered under the
  `"groupicons"` id. It resolves URLs of the form
  `image://groupicons/<std|inv>/<position|affect>/<lowercase-enum-name>` (documented
  above `GroupModel` in `src/group/GroupModel.h`) back to icon files via the existing
  `getIconFilename()` lookup. `<std|inv>` picks the standard or inverted (white-icon)
  variant, mirroring `GroupImageCache`'s invert condition
  (`mmqt::textColor(charColor) == Qt::white`); `<lowercase-enum-name>` is the same
  token `Filenames.cpp`'s `getFilenameSuffix()` uses (e.g. `standing`, `sanctuary`).
- `DescriptionImageProvider` (`src/qml/DescriptionImageProvider.{h,cpp}`), registered
  under the `"description"` id, which also performs the stack blur used for the
  Description panel's background (see below).

For panels that need to hand data from an always-compiled adapter object (which
can't depend on `Qt6::Quick`) to a `Quick`-only image provider, the pattern is a
small shared struct behind a `std::shared_ptr`: `DescriptionImageStore`
(`src/media/DescriptionImageStore.h`) holds the current sharp `QImage` plus a
revision counter, guarded by a `QMutex` because `requestImage()` runs on a QML
loader/render thread while `DescriptionAdapter::updateRoom()` runs on the GUI
thread. `DescriptionAdapter` (always compiled, owns no `QQuickImageProvider`) and
`DescriptionImageProvider` (`Quick`-only, owned by the `QQmlEngine`) both hold a
`shared_ptr` to the same store, which sidesteps their lifetime mismatch —
`DescriptionAdapter` is owned by `MainWindow`, `DescriptionImageProvider` by the
engine, and neither has to outlive the other. `rev` is embedded in the
`image://description/...` URL so a changed room forces QML to reload the image
instead of reusing a cached pixmap for a stale id. The actual stack blur for the
panel's blurred background happens lazily inside `DescriptionImageProvider::requestImage()`,
in C++, not in QML.

### Group panel design notes

`src/qml/GroupPanel.qml` is a `ListView`, not a `TableView`, over
`GroupProxyModel`/`GroupModel` (`src/group/GroupModel.{h,cpp}`), coordinated by
`GroupController` (`src/group/GroupController.{h,cpp}`). Notes on the choices made:

- **`ListView`, not `TableView`.** The group roster is a row-oriented UI (each row
  is one character, selected/dragged/right-clicked as a unit), and Qt 6.4's
  `TableView` has no row-level selection or built-in drag-and-drop model — both
  would have to be hand-rolled on top of `TableView` anyway, at which point a
  `ListView` of per-row delegates is simpler and gives per-row `TapHandler`/
  `DragHandler`/`DropArea` for free.
- **Fixed-width `Row` columns, not `QtQuick.Layouts`.** Each delegate lays out its
  cells with a plain `Row` of fixed/derived-width children (`nameW`, `statW`,
  `stateW`, `roomW` computed as properties on the root `PanelFrame`), rather than
  `RowLayout`/`ColumnLayout`. This avoids adding a `QtQuick.Layouts` import (and the
  corresponding `qml6-module-qtquick-layouts` package) for something fixed-width
  positioning already covers — see "Packaging" below.
  `statMetrics`/`TextMetrics` mirrors `GroupDelegate::sizeHint()`'s `"999 / 999"`
  monospace measurement from the legacy `groupwidget.cpp` delegate to size the
  HP/Mana/Moves columns.
- **Declarative pulse animation, not a repaint timer.** The legacy
  `GroupDelegate::paint()` drove the low-HP/low-Moves pulse by hand: a 50ms
  `QTimer` forced a repaint on every tick and the delegate recomputed a sine-wave
  alpha in `paint()`. `GroupPanel.qml`'s `StatBar` component replaces that with a
  `SequentialAnimation on opacity` (two 750ms `NumberAnimation`s with
  `Easing.InOutSine`, `loops: Animation.Infinite`) bound to the `low` property —
  the scene graph drives the animation, so there's no polling timer and no
  C++-side repaint scheduling at all.
- **Left-button-only drag to reorder.** Reordering is done via `DragHandler` +
  `DropArea`, calling `GroupController::moveCharacter(fromProxyRow, toProxyRow)` on
  drop. `DragHandler`'s default `acceptedButtons` (left button only) is deliberate,
  not incidental: right-click and long-press both open the row's context menu
  (`TapHandler` with `acceptedButtons: Qt.RightButton`, plus `onLongPressed`), so a
  drag gesture that also accepted the right button or triggered on long-press would
  race with the context menu. Recolor is still a C++ `QColorDialog` invoked from
  `GroupController::recolorCharacter()` (via the "Recolor" context menu item), not a
  QML dialog — see "Packaging" below.

### Log panel

`src/qml/LogPanel.qml` is backed by `mainwindow/LogModel.{h,cpp}`, a
`QAbstractListModel` capped at `LogModel::MAX_LINES = 10000` lines: once the cap is
reached, `append()` drops the oldest line before inserting the new one. This is new
behavior relative to the legacy inline `QTextBrowser` it replaces, which had no line
limit and could grow unbounded for a long-running session.

### Packaging: zero new QML module dependencies

Phase 2 added no new entries to the Debian `qml6-module-*`/`libqt6*` dependency list
in `src/CMakeLists.txt` (`CPACK_DEBIAN_PACKAGE_DEPENDS`, `WITH_QML` branch) or to
`.github/workflows/build-test.yml`'s package install list — both are byte-for-byte
unchanged from phase 1. This was a deliberate constraint, not an accident of what
happened to be needed:

- Recolor uses a plain C++ `QColorDialog::getColor()` call from
  `GroupController::recolorCharacter()`, not `QtQuick.Dialogs`' `ColorDialog` — so no
  `qml6-module-qtquick-dialogs` dependency.
- The Description panel's background blur is done in C++ inside
  `DescriptionImageProvider::requestImage()` (a manual stack blur over the `QImage`),
  not `Qt5Compat.GraphicalEffects`' `FastBlur` — so no `qml6-module-qt5compat-graphicaleffects`
  (or the `Qt5Compat` component in `find_package(Qt6 ...)`) dependency.
- The Group panel's columns are fixed-width `Row` children (see above), not
  `QtQuick.Layouts` — so no `qml6-module-qtquick-layouts` dependency.

If a future phase revisits any of these decisions — e.g. moving recolor to a QML
`ColorDialog`, or the blur to a QML effect — the corresponding package(s) above (and
the matching `find_package(Qt6 COMPONENTS ...)` addition) need to be added to both
`src/CMakeLists.txt`'s `CPACK_DEBIAN_PACKAGE_DEPENDS` and
`.github/workflows/build-test.yml`, alongside a `WITH_QML` doc update here.

## Phase 3 notes

Phase 3 fixed a layout bug in the Description panel's blur backdrop, standardized
every ListView-based panel's header on the same look `RoomPanel`'s `TableView`
already had, ported the Tasks panel, and ported the two always-on status bar
widgets (Mume clock, XP status). No new QML runtime module dependencies were
introduced (same "Packaging" constraint as phase 2).

### Description panel blur layout fix + `grab()`-based pixel regression test

The sharp foreground image in `DescriptionPanel.qml` was previously laid out
full-bleed over the blurred backdrop; phase 3 insets it instead (widget-parity
with the legacy `DescriptionWidget`, which never drew the sharp image edge to
edge). This left a border of blur-only background visible around the sharp
image, which is otherwise easy to regress silently since a screenshot diff is
the only way to notice a layout change like this.

`TestQml::descriptionPanelBlurLayout()` (`tests/TestQml.cpp`) guards it with a
pixel-level regression test: it loads `DescriptionPanel.qml` inside a real
`QmlDockWidget`, feeds `DescriptionAdapter::setImageForTesting()` a strongly
patterned red/blue source image, waits for the blur image provider's async
`Image.Ready` status, calls `QQuickWidget::grab()`, and asserts on the sampled
corner pixel's color (reddish, non-transparent) at a coordinate known to fall
in the blur-only border under the new layout. This works under CI because
`initTestCase()` forces `QT_QUICK_BACKEND=software` before `QApplication` is
constructed — the same software scene-graph backend production uses (see
"Startup graphics API pinning" above) — so `grab()` returns real rendered
pixels even on a GPU-less, headless offscreen-platform CI runner. **This is a
reusable pattern**: any future QML layout bug that's easier to see than to
assert on via property inspection can use `grab()` + `pixelColor()` the same
way, as long as the software backend is active.

### `PanelHeaderRow.qml`: shared table-header convention

`src/qml/PanelHeaderRow.qml` is a plain `Row` of styled cells that reproduces
`QtQuick.Controls.Fusion`'s `HorizontalHeaderView` delegate look (8px cell
padding, top-to-bottom gradient, 1px cell borders, centered text), but driven
by a `columns: [{text, width}, ...]` property instead of a `syncView`
`TableView`. It exists because `HorizontalHeaderView` requires a `syncView`
`TableView` to size against, which rules it out for the `ListView`-based
panels (Group, Tasks): those need the same visual header row without an
underlying `TableView`.

The convention going forward:

- Panels whose body is a `TableView` (`RoomPanel`) use
  `QtQuick.Controls.Fusion`'s `HorizontalHeaderView` directly, unchanged.
- Panels whose body is a `ListView` (`GroupPanel`, and the new `TasksPanel`)
  use `PanelHeaderRow` instead, with `columns` mirroring the `ListView`
  delegate's own column layout so the header cells line up with the rows
  beneath them.

`PanelHeaderRow` is `SystemPalette`-derived (`light`/`button`/`mid`/`text`
roles), not the upstream delegate's hardcoded light-theme literals, so it
tracks `ThemeManager`'s dark/light switches the same way `PanelFrame` does
(see "Theming" above) — the upstream Fusion delegate does not do this itself,
which is why phase 3 needed a local component rather than reusing it as-is
for the `ListView` panels too.

### Tasks panel: poll/diff `TasksModel` + `holdRemovals`

`src/mainwindow/TasksModel.{h,cpp}` is a `QAbstractListModel` that mirrors the
legacy `TasksPanel` (QWidget) dock: a `QTimer` polls the `async_tasks`
registry every 250ms (`TASKS_REFRESH_INTERVAL_MS`, matching the widget's own
cadence) and diffs the polled snapshot against the model's current rows, so
`TasksPanel.qml`'s `ListView` only sees `dataChanged`/insert/remove deltas
each tick rather than a full model reset.

`holdRemovals` (`Q_PROPERTY bool`) mirrors the widget's `underMouse()`-gated
removal suppression (`TasksPanel::refresh_data()`'s `allowRemoval`): while
true, `refresh()` still updates rows in place (so a task's final
Finished/Canceled status is visible) but leaves finished/vanished tasks in the
model instead of removing them, so a user hovering the list doesn't have rows
yanked out from under the pointer mid-interaction. `TasksPanel.qml` binds this
to a `HoverHandler` over the `ListView`
(`onHoveredChanged: tasksModel.holdRemovals = hovered`). Transitioning from
held to released calls `refresh()` immediately rather than waiting for the
next timer tick, so deferred removals happen as soon as the pointer leaves
instead of up to 250ms later.

`TasksPanel.qml` uses `PanelHeaderRow` for its header (see above) and a
`Q_INVOKABLE cancelTask(row)` on the model — unifying the widget's separate
"Cancel" row-button and context-menu code paths into one entry point — that
only calls `AsyncTaskHandle::requestCancel()` when `getCanCancel()` allows it.

### Status bar: bare `QQuickWidget` + `SizeViewToRootObject`

The Mume clock and XP status widgets are always-on status bar contents, not
dockable panels, so `MainWindow::setupStatusBar()` embeds them with a bare
`QQuickWidget` directly (`statusBar()->insertPermanentWidget(...)`) instead of
`QmlDockWidget`, which would add unneeded `QDockWidget` machinery. The pattern
(mirrored for both `ClockStrip.qml` and `XpStatusItem.qml`):

```cpp
auto *const quick = new QQuickWidget(statusBar());
quick->setResizeMode(QQuickWidget::SizeViewToRootObject);
quick->setClearColor(Qt::transparent);
quick->setAttribute(Qt::WA_TranslucentBackground);
quick->rootContext()->setContextProperty("adapter", adapter); // before setSource()
quick->setSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/XpStatusItem.qml")));
statusBar()->insertPermanentWidget(0, quick);
```

- `SizeViewToRootObject` makes the `QQuickWidget` size itself to the QML root
  item's `implicitWidth`/`implicitHeight` instead of the more common opposite
  (view sizes the root item) — required here because a status bar item's
  natural size is content-driven (e.g. the clock's text width changes as the
  in-game time changes) and `QStatusBar`'s layout needs an accurate
  `sizeHint()` from the widget to lay out its permanent widgets correctly.
- `setClearColor(Qt::transparent)` + `Qt::WA_TranslucentBackground` let the
  status bar's own background show through the item instead of the
  `QQuickWidget` painting an opaque backing rectangle behind it — without
  both, the item would appear as an opaque colored box floating in the status
  bar rather than blending in.

Both widgets are backed by an always-compiled adapter with no `QQuickWidget`/
`Qt6::Quick` dependency of its own (`XpStatusAdapter`, `ClockAdapter` — same
shape as `DescriptionAdapter`/`QmlConfig` from phase 2, see "Per-engine image
providers" above): `XpStatusAdapter` ports `XPStatusWidget`'s
`updateContent()`/`enterEvent()`/`leaveEvent()`/`clicked()` logic behind
`Q_INVOKABLE hoverEntered()`/`hoverExited()`/`clicked()` methods called from
QML `HoverHandler`/`TapHandler`s, and signals
(`sig_showStatusMessage`/`sig_clearStatusMessage`/`sig_toggleAdventurePanel`)
that `mainwindow.cpp` wires to the real `QStatusBar` and the Adventure dock.
`ClockAdapter` does the equivalent for `MumeClockWidget`.

`src/clock/ClockStrings.{h,cpp}` is a small enum→`QString` helper namespace
(`clockstrings::moonPhaseEmoji()`, `seasonText()`, `weatherEmoji()`/
`weatherTooltip()`, `fogEmoji()`/`fogTooltip()`) factored out of
`MumeClockWidget` and shared by both `ClockAdapter` and the legacy
`MumeClockWidget` (`WITH_QML=OFF`), so the two implementations can't drift on
what a given moon phase/season/weather/fog value displays as. It has no Qt
widget or QML dependency, only `QString`, so it links into both builds
unconditionally.

### Packaging: zero new QML module dependencies (again)

As in phase 2, phase 3 added no new `qml6-module-*` entries to
`CPACK_DEBIAN_PACKAGE_DEPENDS` in `src/CMakeLists.txt` or to
`.github/workflows/build-test.yml`'s package list — the new panels/widgets use
only `QtQuick` primitives, `SystemPalette`, `HoverHandler`/`TapHandler`, and
`QQuickWidget`, all already covered by the existing dependency set.

## Phase 5 notes

### Client architecture: `ClientController` + backend seam

The integrated MUD client (`ClientWidget`/`InputWidget`/`StackedInputWidget`/
`PreviewWidget`) is the biggest single panel ported so far, and the only one
that owns a live network connection. Rather than porting `ClientWidget`'s
logic straight into QML/JS, it was lifted into a plain `QObject`,
`ClientController` (`src/client/ClientController.h`), that owns the command
history, hotkey dispatch, tab-completion, and command-separator splitting
logic InputWidget/StackedInputWidget used to own, and exposes it to
`ClientPanel.qml`/`ClientDisplay.qml` via `Q_PROPERTY`/`Q_INVOKABLE`.

The one piece deliberately *not* folded into `ClientController` itself is the
telnet socket. `ClientController` talks to it only through the abstract
`ClientControllerBackend` seam (`connectToMud()`/`disconnectFromMud()`/
`isConnected()`/`sendToMud()`/`windowSizeChanged()`); `ClientTelnetBackend`
(`src/client/ClientTelnetBackend.{h,cpp}`) is the real implementation, owning
a `ClientTelnet` and forwarding its callbacks back into the controller. Tests
(`tests/TestClient.cpp`) install a `FakeBackend` instead, so all of
`ClientController`'s Q_INVOKABLE logic — history boundaries, hotkey lookup,
tab-completion cycling, command splitting — is exercised without a real
socket. This also keeps `TestClient`/`TestQml` free of any dependency on
actual network I/O: the backend seam is what makes `ClientController` a unit
suitable for `Q_INVOKABLE`-level testing in the first place, not just a
QML-friendly wrapper.

### `ClientPanel.qml` key handling

`ClientPanel.qml`'s input `TextArea` reimplements `InputWidget`'s
`keyPressEvent()`/`event()` key handling in QML's `Keys.onPressed`/
`Keys.onShortcutOverride`, in the same numbered-case order as the original
(tab-completion state machine, configured hotkeys, terminal Ctrl shortcuts,
Enter, history Up/Down, Tab, PageUp/PageDown — see the comments inline).
Two points are easy to get wrong when reading it:

- **`Keys.onShortcutOverride` guard**: any key that resolves to a configured
  hotkey (`clientController.isHotkey()`) must be claimed in
  `ShortcutOverride` before `Keys.onPressed` ever runs, mirroring
  `InputWidget::event()`'s `QEvent::ShortcutOverride` handling
  (`inputwidget.cpp`). Without this, Qt's shortcut system intercepts the key
  as a global `QAction` shortcut first and `Keys.onPressed` never sees it.
- **The Ctrl+numpad zoom collision**: this guard is not just defensive —
  `Hotkey.h`'s `NUMPAD_PLUS`/`NUMPAD_MINUS` bindings resolve to the same
  `Qt::Key_Plus`/`Qt::Key_Minus` codes as `MainWindow`'s map-zoom `QAction`s
  (`zoomInAct`'s `"Ctrl++"`, `zoomOutAct`'s `"Ctrl+-"`, see
  `mainwindow.cpp:createActions()`). Without the `ShortcutOverride` claim, a
  user with a numpad +/- hotkey configured while typing in the client would
  have Ctrl+Numpad+/- silently zoom the map canvas instead of firing their
  hotkey — the same collision `InputWidget`'s widget-level event filter
  existed to prevent.

### Password entry: inline swap, not a floating dialog

`ClientWidget`/`StackedInputWidget` show `PasswordDialog` as a small floating
top-level dialog when the telnet echo mode turns off. `ClientPanel.qml`
instead swaps the input `TextArea` for a `TextField` with
`echoMode: TextInput.Password` in place, toggled by
`clientController.echoVisible` (see the `inputHolder` `Item` in
`ClientPanel.qml`). A floating dialog is a mouse/keyboard-era pattern that
adds an extra window to manage for no benefit on touch — an inline swap
keeps focus in the same input rectangle the user was just typing in, which
matters more on the touch-friendly targets this migration is aimed at
(see "Goal" above) than pixel-parity with the old dialog chrome.

### Preview tail: `ClientLineModel` shared between two views

`PreviewWidget`'s job — showing a peek of the trailing scrollback while the
user has scrolled the main display up to read backlog — needed no new model
of its own: `ClientPanel.qml`'s preview `ListView` binds directly to the same
`ClientLineModel` instance (`clientLineModel` context property) that
`ClientDisplay.qml`'s main view uses, just filtered to `visible: !display.atEnd`
and pinned to the bottom (`positionViewAtEnd()`). One list model, two views,
rather than `PreviewWidget`'s separate `QTextDocument` mirroring the main
display's content.

### `displaywidget` stayed compiled unconditionally through phase 8

Unlike every other Widget class this phase retired, `client/displaywidget.{h,cpp}`
was *not* moved into the `if(NOT WITH_QML)` block at this point — see "Ported
panels" above for why (`AnsiTextHelper` had consumers outside the client:
`AnsiViewWindow.cpp` and `remoteeditwidget.cpp`; `roomeditattrdlg.cpp` was
one too until its phase-8 QML port switched that path to
`mmqt::ansiToHtml()`). Phase 9 / Stage 1 ported both of those remaining
outside consumers off it, so `displaywidget` moved into the `if(NOT WITH_QML)`
block after all — see "Phase 9 / Stage 1" below.

## Phase 6 notes

Phase 6 ported the remaining top-level dialogs — About, MMapper Updater, Find
Rooms, and the Preferences dialog (nine pages) — from `QDialog`s built around
generated `ui_*.h` forms to `QmlDialog`-hosted `.qml` files driven by plain
`QObject` controllers/adapters, following the same widget-vs-QML,
`WITH_QML`-gated split as every panel before it.

### `QmlDialog`: the dialog equivalent of `QmlDockWidget`

`src/qml/QmlDialog.{h,cpp}` is `QmlDockWidget`'s counterpart for top-level
dialogs: a `QDialog` subclass that fills its client area with a single
`QQuickWidget`, exposes the same `setContextProperty()`/`addImageProvider()`/
`setQmlSource()` surface (context properties and image providers still have to
be registered **before** `setQmlSource()`, for the same reason as
`QmlDockWidget` — see "`QmlDockWidget`" above), and has the same QML-load-error
fallback (`QLabel("QML load failed")` swapped in for the `QQuickWidget`).

Two differences from `QmlDockWidget`, both because a dialog is modal-shaped
where a dock is not:

- `QmlDialog` exposes itself as the `"dialog"` context property, so QML can
  call `dialog.accept()`/`dialog.reject()` directly (`QDialog::accept()`/
  `reject()` are slots) — e.g. a Close button binds `onClicked:
  dialog.reject()`. This is set up in the constructor, before any caller-supplied
  context properties, so it's always available.
- `QmlDialog` resizes itself to the QML root item's
  `implicitWidth`/`implicitHeight` (floored at a 320×200 minimum) the first
  time the QML finishes loading, mirroring a `QDialog`'s usual
  size-to-contents behavior on first show — a dock has no equivalent concept
  since `QDockWidget` sizing is driven by the surrounding `QMainWindow`
  layout instead.

Usage pattern (mirrors the `QmlDockWidget` snippet above; see
`src/mainwindow/mainwindow.cpp`):

```cpp
auto *const dialog = new QmlDialog(tr("Find Rooms"), "FindRoomsDlg", this);
dialog->setContextProperty("findRoomsController", m_findRoomsController);
dialog->setContextProperty("findRoomsModel", m_findRoomsController->getModel());
dialog->setQmlSource(QUrl(QStringLiteral("qrc:/qt/qml/MMapper/FindRoomsDialog.qml")));
```

About, Update, and Find Rooms are all created once, eagerly, alongside their
Widgets counterparts during `MainWindow`'s constructor (same
`#ifdef MMAPPER_WITH_QML` / `#else` pattern as every dock), and simply
`show()`/`raise()`/`activateWindow()`d on demand — there is no per-open
recreate/destroy cycle for these three. `objectName` is kept identical to the
Widgets dialog it replaces (`"FindRoomsDlg"`, `"UpdateDialog"`,
`"AboutDialog"`, `"ConfigDialog"`) so any saved window geometry/state keyed
on that name still applies.

### About dialog: `AboutInfo` + `LicenseModel`

`src/mainwindow/AboutInfo.{h,cpp}` is a widget-free content provider: it
builds the same about/authors HTML and license text `AboutDialog` (the
widget) builds in its constructor, once, and exposes it to `AboutDialog.qml`
via `CONSTANT` `Q_PROPERTY`s (`aboutHtml`, `authorsHtml`, `licenses`) — the
content never changes for the process lifetime, so there's no `NOTIFY`
plumbing needed.

The Licenses tab is backed by `LicenseModel`, a `QAbstractListModel`
(`TitleRole`/`IntroRole`/`TextRole`) rather than a `QVariantList` of
`QVariantMap`s — a plain `QVariantList`-of-`QVariantMap` model crashes the
QML engine's identifier interning when bound to a `Repeater`, so a real
role-based list model is required here even though the data is static.

### Find Rooms: `FindRoomsController` lifting `FindRoomsDlg`'s slots

`src/mainwindow/FindRoomsController.{h,cpp}` lifts `FindRoomsDlg`'s
search/select/edit/activate slots into a plain `QObject`, owning a
`FindRoomsModel` and re-emitting the same signal surface the widget dialog
emitted (`sig_newRoomSelection`, `sig_center`, `sig_log`,
`sig_editSelection`) — `MainWindow`'s existing downstream connections
(`MapWindow::slot_centerOnWorldPos`, `MapCanvas::slot_setRoomSelection`,
`MainWindow::slot_log`, `MainWindow::slot_onEditRoomSelection`) attach to the
controller exactly as they did to the widget, unchanged.

### Update checker: `UpdateChecker` extracted from `UpdateDialog`

`src/mainwindow/UpdateChecker.{h,cpp}` is the network-check/version-compare
logic (`CompareVersion`, the GitHub releases JSON fetch/parse) factored out
of `UpdateDialog` into a widget-free `QObject`, exposed to
`UpdateDialog.qml` as the `updateChecker` context property. `UpdateDialog`
(`WITH_QML=OFF`) keeps its own copy of the equivalent logic; the extraction
exists so the QML dialog has something to bind to, not to deduplicate the
two (a further follow-up could share `UpdateChecker` between both builds,
but that wasn't required to unblock this port).

### Preferences: `PreferencesController` + nine per-page adapters + `QmlDialog` host

The Preferences dialog is the largest single port in phase 6: nine pages
(Path Machine, Mume Protocol, Auto Logger, Group Manager, Audio, Graphics,
Parser, General, Integrated Client), each previously a `QWidget` subclass
generated from a `.ui` file, ported to a `.qml` page plus a small
`QObject` adapter (`src/preferences/*PageAdapter.{h,cpp}`) exposing that
page's slice of `Configuration` via `Q_PROPERTY`/`Q_INVOKABLE`.

**Apply-on-change, not apply-on-OK.** Every adapter setter writes
`getConfig()` immediately when QML calls it (e.g. toggling a checkbox),
mirroring the legacy widget pages' own `ChangeMonitor`-driven immediate-write
behavior — `ConfigDialog` never buffered edits and wrote them only on OK. The
same is true here: `ok()`/`cancel()` are not a transactional commit/rollback
of in-memory edits, they mirror `ConfigDialog::slot_ok()`/`slot_cancel()`
exactly:

- `PreferencesController::ok()` calls `getConfig().write()` (flush the
  already-applied live `Configuration` to disk) and emits `sig_accepted()`.
- `PreferencesController::cancel()` calls `setConfig().read()` (re-read
  `Configuration` from disk, discarding any apply-on-change writes made while
  the dialog was open) and `reloadAll()`, which re-syncs every adapter so the
  QML pages resync to the on-disk values.

`PreferencesController` (`src/preferences/PreferencesController.{h,cpp}`)
owns all nine adapters as children and exposes each as a `CONSTANT`
`Q_PROPERTY` (`pathMachine`, `mumeProtocol`, `autoLog`, `group`, `audio`,
`graphics`, `parser`, `general`, `client`); `PreferencesDialog.qml`
(`src/qml/PreferencesDialog.qml`) is a `ListView` nav + `Loader` shell —
`configdialog.ui`'s left-nav-plus-stacked-pages layout without a `.ui`
file — with each nav entry's `Loader.source` pointing at one `Prefs*Page.qml`
file (`PrefsPathMachinePage.qml`, ..., `PrefsClientPage.qml`), and Cancel/OK
buttons calling `preferencesController.cancel()`/`ok()` followed by
`dialog.reject()`/`accept()`.

`MainWindow::slot_onPreferences()` creates the controller and dialog once
(lazily, on first open — unlike About/Update/Find Rooms, `PreferencesController`
needs `m_mapWindow`/`m_groupManager`/`m_qmlConfig`/`m_descriptionAdapter`,
which aren't guaranteed constructed yet at `MainWindow` construction time) and
keeps both alive across opens, calling `reloadAll()` immediately before every
`show()` — this replaces `ConfigDialog::showEvent()`'s `sig_loadConfig()`
fan-out, which the legacy widget triggered every time it was shown because it
was destroyed and recreated per-open (`MainWindow::slot_onPreferences()`'s
`m_configDialog.reset()` on `finished`). The QML controller/dialog pair is
never destroyed once created, so an explicit `reloadAll()` on each open takes
its place. Signal wiring is otherwise identical to the widget:
`sig_graphicsSettingsChanged` → `MapWindow::slot_graphicsSettingsChanged`,
`sig_groupSettingsChanged` → `Mmapper2Group::slot_groupSettingsChanged` and
`QmlConfig::reload()`, and the dialog's `finished` signal → `QmlConfig::reload()`
+ `DescriptionAdapter::reloadConfig()` (covers both OK and Cancel, same as the
widget's `finished` handler did — `sig_accepted()` needs no separate
connection of its own because `dialog.accept()` always follows `ok()` in the
same QML click handler, which fires `finished`).

**Two pieces intentionally stayed native widgets, always compiled, through
phase 8.** Unlike every other widget page, `AnsiColorDialog`
(`src/preferences/AnsiColorDialog.{h,cpp,ui}`, with its `ansicombo.{h,cpp}`
combo-box helper) and `ManagePasswordDialog`
(`src/preferences/ManagePasswordDialog.{h,cpp,ui}`) were **not** ported to
QML in phase 6 and were **not** moved into the `if(NOT WITH_QML)` block at
that point — they stayed compiled unconditionally in both builds.
`ParserPageAdapter` launched `AnsiColorDialog` and `GeneralPageAdapter`
launched `ManagePasswordDialog` as plain native `Q_INVOKABLE`-triggered
`QDialog::exec()` calls even when `WITH_QML=ON`, the same way a
`Q_INVOKABLE` on any other adapter opens a native `QColorDialog`/`QFileDialog`
(see `GroupController::recolorCharacter()`'s `QColorDialog` in phase 2,
"Group panel design notes" above, for the same pattern). At the time,
porting either dialog's fairly intricate custom painting (`AnsiColorDialog`'s
live ANSI color grid) or secure-entry semantics (`ManagePasswordDialog`) to
QML was judged not worth it for a rarely-opened dialog reached from one
preferences page each. Phase 9 / Stage 1 revisited that call and ported both
(see "Phase 9 / Stage 1" below); they're now `if(NOT WITH_QML)`-gated like
every other preferences page.

### Deferred / not ported in phase 6

- ~~**`roomeditattrdlg`/`infomarkseditdlg`.**~~ Ported in phase 8 (see
  below); no longer deferred.
- ~~**Preferences search bar.**~~ Ported in phase 7 (see below); no longer
  deferred.
- ~~**`AnsiColorDialog`/`ManagePasswordDialog`.**~~ Ported in phase 9 /
  Stage 1 (see below); no longer deferred.

### Phase 7: user-reported regression fixes (macOS pass on phase 6)

- **Client line endings.** The telnet stream reaching `ClientLineModel`
  keeps `\r\n` endings; the phase-6 `white-space:pre-wrap` wrapper made the
  leftover `\r` render as a second break, doubling every line (the widget
  was immune because `QTextCursor::insertText()` treats `\r\n` as one block
  break). `appendRawWithAnsi()` now treats `\r\n` — including pairs split
  across `appendText()` chunks, via an `m_pendingCr` carry flag — and lone
  `\r` as single breaks.
- **Autoscroll pin.** `ClientDisplay.qml` derived `stick` from every
  `contentY` change, but ListView emits layout-driven `contentY`
  adjustments during appends (contentHeight estimate refinement,
  `dataChanged` on the trailing partial row); those transients unpinned
  the view so new output immediately dropped into preview mode. `stick`
  now only updates on user-driven movement (`listView.moving` /
  scrollbar `pressed`) using the widget's 4px at-bottom tolerance, with
  `pageUp()`/`pageDown()` updating it explicitly.
- **Dialog theming.** Dialog roots were bare `Item`s over `QQuickWidget`'s
  default white clear color with raw black `Text` — white dialogs on any
  theme. `QmlDialog`/`QmlDockWidget` now set the clear color from the host
  palette, dialog roots are `SystemPalette`-window Rectangles, and
  unstyled `Text` became palette-aware `Label` throughout the dialogs and
  preferences pages.
- **Preferences behavior parity.** The widget `ConfigDialog` is a single
  scrolling column of all nine pages (bold section headers with a rule),
  an icon nav that scrolls to a section on click and follows scrolling
  (scrollspy incl. bottom-clamp), a debounced search whose results list
  replaces the page column (non-matching nav entries disabled; result
  click scrolls to and focuses the control), and a back-to-top footer.
  `PreferencesDialog.qml` was a page-per-click `Loader` in a different
  order with different titles; it now mirrors the widget exactly, with
  the nine `Prefs*Page.qml` roots converted from `Flickable` to plain
  `Column`s so the dialog's single `Flickable` owns scrolling. Title-bar
  close persists settings (`PreferencesQmlDialog::closeEvent` in
  `mainwindow.cpp`, mirroring `ConfigDialog::closeEvent`), while the
  Cancel button remains the only revert path and every show re-syncs the
  adapters (`PreferencesController::reloadAll()`), both matching the
  widget.

### Phase 8: panel regression fixes + map-editing dialogs

Regression fixes from the user's macOS pass on phase 7:

- **Dock minimum sizes.** `QmlDockWidget::syncMinimumSize()` copies each
  panel root's `implicitWidth`/`implicitHeight` onto the `QQuickWidget`
  minimum (kept live via `implicit*Changed`), and every `*Panel.qml` root
  declares a minimum mirroring its widget-era hint — the client uses the
  configured columns/rows in font metrics (`DisplayWidget::sizeHint()`
  parity, via new `QmlConfig` `clientColumns`/`clientRows` properties),
  the group panel uses header + one row (`GroupWidget::sizeHint()`).
- **Welcome page images.** The Play button's 48px terminal icon and the
  `mellon.png` pixmap were restored to `ClientPanel.qml`'s welcome column.
- **Timer tick + colors.** `TimerModel`'s 50ms refresh now emits
  `dataChanged` with an empty roles vector (the QML delegate binds the
  custom time/name/expired roles, which the old
  `{DisplayRole, ProgressRole}` list never notified — count-up timers
  looked frozen), and `TimerPanel.qml` ports `TimerDelegate::paint()`'s
  green→yellow→red countdown progress coloring.

Map-editing dialog ports (the phase-6 deferral, now done):

- **Infomarks editor.** `InfomarkEditController` +
  `InfomarkEditDialog.qml` replace `infomarkseditdlg.{h,cpp,ui}` (now
  `NOT WITH_QML`-gated). Ported quirks: the Type combo snaps back to the
  stored value while modifying an existing marker (only "sticks" in
  Create-New mode), Create refreshes but Modify does not, and the
  widget's never-used `MapCanvas*` coupling was dropped (the canvas
  repaints via map-change notifications).
- **Room attribute editor.** `RoomEditController` (all field edits commit
  to `MapData` immediately; "All" mode shows every mob/load flag
  `PartiallyChecked` with no union computation and applies edits via
  `applyChangesToList`; per-direction exit/door models enforce the
  EXIT/DOOR/UNMAPPED checkability constraints; the note is the only
  deferred field) + `RoomEditDialog.qml` (five tabs; tri-state
  checkboxes are display-only overlays so the model stays authoritative)
  replace `roomeditattrdlg.{h,cpp,ui}` (now gated). The asymmetric close
  guard is ported in `RoomEditQmlDialog`: [X] is refused while the note
  is dirty, Escape closes after the "ignored note" warning. Supporting
  pieces: `mainwindow/CheckableFlagModel` (generic tri-state list model
  for QML) and `global/AnsiHtml` (`mmqt::ansiToHtml()`, the widget-free
  extraction of `AnsiTextHelper`'s core used for the description/stats/
  diff previews).

### `.ui` gating

As of phase 6, `preferences/AnsiColorDialog.ui` and
`preferences/ManagePasswordDialog.ui` stayed in the unconditional
`mmapper_UIS` list (see "Two pieces intentionally stayed native widgets"
above); phase 9 / Stage 1 moved both into the `if(NOT WITH_QML)`-gated block
too (see "Phase 9 / Stage 1" below), so `mmapper_UIS`'s unconditional list is
now empty. The other ten `.ui` files —
`configdialog.ui` and the nine page `.ui`s (`audiopage.ui`, `autologpage.ui`,
`clientpage.ui`, `generalpage.ui`, `graphicspage.ui`, `grouppage.ui`,
`mumeprotocolpage.ui`, `parserpage.ui`, `pathmachinepage.ui`) — moved into
the `if(NOT WITH_QML)`-gated `mmapper_UIS` block in `src/CMakeLists.txt`
back in phase 6,
alongside their `.cpp`/`.h` files, following the same pattern
`mumeclockwidget.ui`/`ClientWidget.ui`/`aboutdialog.ui`/`findroomsdlg.ui`
established in earlier phases.

## Phase 9 / Stage 1: ANSI viewer, ANSI color picker, remote editor, and password dialog ports

Phase 9's Stage 1 closed out the last widgets that had been deliberately left
native in earlier phases (see "Deferred / not ported in phase 6" and
`client/displaywidget.{h,cpp}`'s doc comment above), moving `WITH_QML=ON`
another step closer to a build with zero legacy Widget dialogs.

- **ANSI viewer.** `viewers/AnsiViewWindow.h`'s `makeAnsiViewWindow()` — the
  seam both `viewers/LaunchAsyncViewer.h` (the parser's map-show viewers) and
  `mpi/remoteeditwidget.cpp`'s ANSI-preview flow call — now returns a plain
  `std::unique_ptr<QDialog>` rather than a concrete `AnsiViewWindow`. Under
  `WITH_QML` it builds a `QmlDialog` hosting `qml/AnsiViewDialog.qml`, driven
  by the widget-free `viewers/AnsiViewContent` (renders program/title/message
  into a title string and an HTML document via `mmqt::ansiToHtml()`, the same
  default colors `AnsiTextHelper::init()` resolves from
  `Configuration::integratedClient`); under `NOT WITH_QML` it's the original
  `AnsiViewWindow` widget (now gated, along with `client/displaywidget.{h,cpp}`
  — see below). `AnsiViewWindow.h`'s doc comment explains why the return type
  is the widget-free `QDialog` base rather than `AnsiViewWindow` itself: under
  `MMAPPER_WITH_QML` the `AnsiViewWindow` class doesn't even exist, so callers
  can never assume a concrete type.
- **ANSI color picker.** `AnsiColorDialog`'s 17-color tables and SGR
  (Select Graphic Rendition) assembly moved out into a widget-free
  `preferences/AnsiColorTables.{h,cpp}`, shared by both the legacy widget
  (`ansicombo.{h,cpp}` now delegates to it) and the new QML port:
  `mainwindow/AnsiColorPickerController` (fg/bg color-index and
  bold/italic/underline `Q_PROPERTY`s, a `resultAnsiString`/preview-color
  pair recomputed on every setter) + `qml/AnsiColorPickerDialog.qml`. Because
  `preferences/ParserPageAdapter.cpp` compiles straight into
  `TestMainWindow`/`TestQml` (see `tests/CMakeLists.txt`'s
  `preferences_adapter_SRCS`), which link neither `Qt6::Quick` nor `mm_qml`,
  the adapter can't construct a `QmlDialog` itself. `ParserPageAdapter`
  exposes a `setColorPicker(ColorPicker)` injection point instead (default:
  the native `AnsiColorDialog::getColor()`, set under `NOT WITH_QML` inside
  `ParserPageAdapter.cpp`'s own constructor); `mainwindow.cpp`'s
  `slot_onPreferences()` overrides it with `qml/AnsiColorPickerLauncher.h`'s
  `ansi_color_picker::getColor()`, which builds the `QmlDialog` +
  `AnsiColorPickerController` and only invokes the callback on accept. This
  injected-launcher pattern — a widget-free adapter/controller compiled
  everywhere, plus a `WITH_QML`-only launcher `.cpp` in `qml/` that
  `mainwindow.cpp` wires in — is reused verbatim for the password dialog
  below, and was already established for `GroupController::recolorCharacter()`'s
  native `QColorDialog` back in phase 2.
- **Remote editor.** Landed in three steps:
  1. *Widget-free text core extraction.* `RemoteEditHighlighter` (the
     six-pass ANSI/encoding/overflow line highlighter) and
     `RemoteEditDocumentOps` (justify/tab/whitespace/ANSI operations,
     wrap-around find/replace, goto-line, and the status-bar computation)
     moved out of `remoteeditwidget.cpp` into always-compiled, `QtGui`-only
     files (`mpi/RemoteEditHighlighter.{h,cpp}`,
     `mpi/RemoteEditDocumentOps.{h,cpp}`) operating on
     `QTextDocument`/`QTextCursor`. The widget editor now delegates to them
     instead of implementing the logic inline, preserving quirks byte-for-byte
     (the 79-vs-80 length edge case, `joinLines`' extra-block selection
     quirk).
  2. *QML editor added behind the widget.* `mpi/RemoteEditController` drives
     a QML `TextArea`'s `QTextDocument` with the shared highlighter and
     document ops, tracks dirtiness by byte-comparing against the original
     body (mirrors the widget's `slot_contentsChanged()`, not
     `QTextDocument::isModified()`), and emits the same `sig_save`/`sig_cancel`
     signals `RemoteEditWidget` did. `qml/RemoteEditDialog.qml` carries the
     full menu and shortcut surface, inline find/replace and goto bars, Tab/
     Backtab handling, and the status footer. `qml/RemoteEditDialogHost`
     ports the submitted/dirty discard-confirmation close contract
     (`closeEvent`'s asymmetric guard). At this point the session layer
     still constructed the widget editor — the switch landed separately so
     the QML editor could be exercised in isolation first.
  3. *Session switch.* `mpi/remoteeditsession.cpp`
     (`RemoteEditInternalSession`) and the parser's config-edit path
     (`parser/AbstractParser-Config.cpp`) now construct
     `RemoteEditController` + `RemoteEditDialogHost` under `WITH_QML`, with
     the same save/cancel wiring and close-on-destruct lifecycle the widget
     path had. With every `WITH_QML`-build consumer moved off them,
     `mpi/remoteeditwidget.{h,cpp}`, `mpi/findreplacewidget.{h,cpp}`,
     `mpi/gotowidget.{h,cpp}`, and `client/displaywidget.{h,cpp}` all retired
     into the `if(NOT WITH_QML)` block in `src/CMakeLists.txt` (see the
     updated "Ported panels" and "`displaywidget` stayed compiled
     unconditionally through phase 8" notes above).
- **Password dialog.** `preferences/ManagePasswordDialog.{h,cpp,ui}` is
  replaced under `WITH_QML` by `preferences/PasswordDialogController`
  (widget-free: `accountName`/`password`/`hasStoredPassword`/
  `showPasswordControls`/`errorText` `Q_PROPERTY`s, a `requestDelete()`
  invokable mirroring the widget's delete-button handler) +
  `qml/PasswordDialog.qml` (account-name field, password field with a View
  echo-mode toggle, a Delete button, and a single OK button — the widget's
  `QDialogButtonBox` only ever had `QDialogButtonBox::Ok`, no Cancel).
  `GeneralPageAdapter` gets the same injected-launcher treatment as
  `ParserPageAdapter`'s color picker: it exposes
  `setPasswordDialogLauncher(PasswordDialogLauncher)`, defaults (under
  `NOT WITH_QML`) to constructing the native `ManagePasswordDialog` exactly
  as before, and `mainwindow.cpp`'s `slot_onPreferences()` overrides it with
  `qml/PasswordDialogLauncher.h`'s `password_dialog::manage()`. The WASM
  branch (`ManagePasswordDialog.cpp:19-24`'s `CURRENT_PLATFORM ==
  PlatformEnum::Wasm` check, which hides the password/view/delete controls
  and retitles the dialog to "Manage Account") is decided in
  `password_dialog::manage()` (C++), not in QML, and passed to the
  controller as the `showPasswordControls` `Q_PROPERTY`; the QML file only
  reads that flag to decide what to show. `GeneralPageAdapter`'s keychain
  round-trip (`PasswordConfig::setPassword()`/`getPassword()`/
  `deletePassword()`) and its two `ManagePasswordDialog` construction sites
  (the `sig_incomingPassword` prefill path and the no-stored-password path)
  are otherwise untouched — only which dialog implementation gets
  constructed changed.
- **Manager-level flows remain native.** `GeneralPageAdapter::factoryReset()`
  /`exportConfig()`/`importConfig()`'s native `QMessageBox` confirmations and
  `QFileDialog` content APIs, and the WASM keychain-error `QMessageBox` in
  `PasswordConfig`'s `handleError()` lambda, are unchanged by this port —
  porting confirmation/file-picker chrome to QML was out of scope for Stage 1
  and is deliberately deferred to whichever later stage tackles the
  remaining native application shell (the top-level `QMainWindow`, its menu
  bar, and the last few native-only dialog flows like these).

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
