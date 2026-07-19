#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "MapSource.h"
#include "RawMapData.h"
#include "abstractmapstorage.h"

#include <memory>
#include <optional>

#include <QObject>

// Widget-free map-storage helpers shared by MainWindow's async load path
// (see mainwindow/mainwindow-async.cpp's getLoadOrMergeMapStorage()/
// background::load_map_data(), which now delegate here) and QmlShellWindow's
// Shell B load path (see mainwindow/QmlShellWindow.cpp's wireFileCommands()).
//
// Neither function here touches QtWidgets, QProgressDialog, or MainWindow --
// they only need a QObject* to parent the storage object to (for lifetime/
// sig_log wiring, which callers still do themselves) and an
// AbstractMapStorage& with its ProgressCounter already attached. That's the
// narrowest seam that lets a widget-free shell reuse exactly the same format
// detection and background load logic MainWindow uses, without pulling in
// MainWindow's QProgressDialog/CanvasDisabler/ExtraBlockers machinery.
namespace maploadhelper {

// Sniffs pSource's IODevice against every known map format (MMapper2 binary,
// MMapper2/Pandora XML) and constructs the matching AbstractMapStorage
// subclass, parented to `parent`. Throws std::runtime_error("Unrecognized
// file format") if none match. Callers are still responsible for connecting
// AbstractMapStorage::sig_log to their own log sink, same as before this was
// extracted from mainwindow-async.cpp.
NODISCARD std::unique_ptr<AbstractMapStorage> detectAndCreateStorage(
    const std::shared_ptr<MapSource> &pSource, QObject *parent);

// Loads raw map data from storage and constructs a Map from it. Safe to call
// on a background thread (mirrors mainwindow-async.cpp's
// AsyncLoader::background_load()); storage's ProgressCounter must already be
// set (via AbstractMapStorage::setProgressCounter()) before calling this.
NODISCARD std::optional<MapLoadData> loadMapData(AbstractMapStorage &storage);

} // namespace maploadhelper
