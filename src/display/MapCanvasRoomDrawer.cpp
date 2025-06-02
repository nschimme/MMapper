// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "MapCanvasRoomDrawer.h"

// Most, if not all, content of this file was related to the old
// asynchronous map drawing system (InternalData, generateMapDataFinisher, etc.)
// or the direct synchronous creation of MapBatches from MapCanvas.
// This logic has been moved to AsyncMapAreaManager.cpp (for data preparation)
// and MapCanvas.cpp/mapcanvas_gl.cpp (for initiating and rendering).

// LayerMeshes::render is implemented in MapBatches.cpp.
// The Batches struct defined in MapCanvasRoomDrawer.h has been simplified
// to only contain non-area-specific elements like infomark meshes.

// If any utility functions from this file were truly generic and used by
// other parts of the display code AND NOT related to the old remeshing,
// they would be preserved here. However, the majority of this file's previous
// content was specific to the now-replaced map area drawing pipeline.

// It is expected that this file is now very small or empty after the refactor.
// Keeping necessary includes for MapCanvasRoomDrawer.h if it still declares anything.
#include "../configuration/configuration.h" // For LOOKUP_COLOR if any minor helpers remained that use it.
#include "../global/utils.h" // For DECL_TIMER if any minor helpers remained.
#include "../mapdata/mapdata.h" // If any helpers directly interact with MapData.
#include "../opengl/OpenGL.h" // If any helpers directly interact with OpenGL.
#include "MapCanvasData.h" // For basic types if still needed.

// For example, if LOOKUP_COLOR macro was used by some remaining small static helper here:
// #define LOOKUP_COLOR(X) (getNamedColorOptions().X)
// However, getNamedColorOptions itself was likely part of the removed code.

// If Batches struct methods were defined here, they would remain if Batches struct still has logic.
// Batches methods like resetExistingMeshesButKeepPendingRemesh were modified in the .h directly.
// No remaining method implementations from Batches struct are expected here.
