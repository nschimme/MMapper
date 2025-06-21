#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../global/RuleOf5.h"
#include "../map/ExitDirection.h"
#include "../map/coordinate.h"
#include "path.h"
#include "pathprocessor.h"
#include "patheventcontext.h" // Include PathEventContext

#include <memory>

#include <QtGlobal>

// class PathMachine; // No longer needed directly if context provides map etc.
struct PathParameters;
// Forward declare mmapper::PathEventContext if not fully included via patheventcontext.h
// namespace mmapper { struct PathEventContext; } // Already handled by include

/*!
 * @brief Abstract base for PathProcessor strategies in the "Experimenting" state.
 *
 * Provides common functionality for forking new paths (`augmentPath`) and
 * evaluating path probabilities (`evaluate`) when PathMachine is uncertain and
 * exploring multiple hypotheses. Concrete strategies (Crossover, OneByOne)
 * implement `virt_receiveRoom` and use these inherited capabilities.
 *
 * @note Base class for Crossover and OneByOne
 */
class NODISCARD Experimenting : public PathProcessor
{
protected:
    mmapper::PathEventContext &m_context; // Added
    const Coordinate m_direction;
    const ExitDirEnum m_dirCode;
    const std::shared_ptr<PathList> m_paths;
    PathParameters &m_params;
    std::shared_ptr<PathList> m_shortPaths;
    std::shared_ptr<Path> m_best;
    std::shared_ptr<Path> m_second;
    double m_numPaths = 0.0;

protected:
    void augmentPath(const std::shared_ptr<Path> &path, const RoomHandle &room);

public:
    Experimenting() = delete;
    DELETE_CTORS_AND_ASSIGN_OPS(Experimenting);

protected:
    // explicit Experimenting(std::shared_ptr<PathList> paths, ExitDirEnum dirCode, PathParameters &params);
    explicit Experimenting(mmapper::PathEventContext &context, // Added context
                           std::shared_ptr<PathList> paths,
                           ExitDirEnum dirCode,
                           PathParameters &params);

public:
    ~Experimenting() override;

public:
    std::shared_ptr<PathList> evaluate();
};
