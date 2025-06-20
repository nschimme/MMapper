#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../global/RuleOf5.h"
#include "../map/DoorFlags.h"
#include "../map/ExitDirection.h"
#include "../map/ExitFieldVariant.h"
#include "../map/ExitFlags.h"
#include "PathProcessor.h" // Changed path
#include "../map/coordinate.h"
#include "path.h"
#include "../map/ChangeList.h"   // Added for ChangeList in evaluate()

#include <memory>

#include <QtGlobal>

class PathMachine;
struct PathParameters;

/**
 * @brief Abstract base for PathProcessor strategies in the "Experimenting" state.
 *
 * Provides common functionality for forking new paths (`augmentPath`) and
 * evaluating path probabilities (`evaluate`) when PathMachine is uncertain and
 * exploring multiple hypotheses. Concrete strategies (Crossover, OneByOne)
 * implement `virt_receiveRoom` and use these inherited capabilities.
 */
// Base class for Crossover and OneByOne
class NODISCARD Experimenting : public PathProcessor // Removed EnableGetWeakHandleFromThis<Experimenting>
{
protected:
    void augmentPath(const std::shared_ptr<Path> &path, const RoomHandle &room);
    const Coordinate m_direction; // Prefixed
    const ExitDirEnum m_dirCode; // Prefixed
    const std::shared_ptr<PathList> m_paths; // Prefixed
    PathParameters &m_params; // Prefixed
    std::shared_ptr<PathList> m_shortPaths; // Prefixed
    std::shared_ptr<Path> m_best; // Prefixed
    std::shared_ptr<Path> m_second; // Prefixed
    double m_numPaths = 0.0; // Prefixed

public:
    Experimenting() = delete;
    DELETE_CTORS_AND_ASSIGN_OPS(Experimenting);

protected:
    explicit Experimenting(std::shared_ptr<PathList> paths,
                           ExitDirEnum dirCode,
                           PathParameters &params);

public:
    ~Experimenting() override;

public:
    std::shared_ptr<PathList> evaluate(ChangeList &changes); // Added ChangeList
};
