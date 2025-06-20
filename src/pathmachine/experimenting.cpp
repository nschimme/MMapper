// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "experimenting.h"

#include "../global/utils.h"
#include "../map/room.h"
#include "path.h"
#include "pathparameters.h"

#include <memory>

Experimenting::Experimenting(std::shared_ptr<PathList> pat,
                             const ExitDirEnum in_dirCode,
                             PathParameters &in_params)
    : m_direction(::exitDir(in_dirCode)) // Prefixed
    , m_dirCode(in_dirCode) // Prefixed
    , m_paths(PathList::alloc()) // Prefixed
    , m_params(in_params) // Prefixed
    , m_shortPaths(std::move(pat)) // Prefixed
{}

Experimenting::~Experimenting() = default;

void Experimenting::augmentPath(const std::shared_ptr<Path> &path, const RoomHandle &room)
{
    auto &p = deref(path);
    const Coordinate c = p.getRoom().getPosition() + m_direction; // Prefixed
    // Use getSharedPtrFromThis() from derived class (Crossover/OneByOne)
    const auto working = p.fork(room, c, m_params, this->getSharedPtrFromThis(), m_dirCode); // Prefixed params, dirCode
    if (m_best == nullptr) { // Prefixed
        m_best = working; // Prefixed
    } else if (working->getProb() > m_best->getProb()) { // Prefixed
        m_paths->push_back(m_best); // Prefixed
        m_second = m_best; // Prefixed
        m_best = working; // Prefixed
    } else {
        if (m_second == nullptr || working->getProb() > m_second->getProb()) { // Prefixed
            m_second = working; // Prefixed
        }
        m_paths->push_back(working); // Prefixed
    }
    ++m_numPaths; // Prefixed
}

std::shared_ptr<PathList> Experimenting::evaluate(ChangeList &changes) // Added ChangeList
{
    for (PathList &sp = deref(m_shortPaths); !sp.empty();) { // Prefixed
        std::shared_ptr<Path> ppath = utils::pop_front(sp);
        Path &path = deref(ppath);
        if (!path.hasChildren()) {
            path.deny(changes); // Pass changes
        }
    }

    if (m_best != nullptr) { // Prefixed
        if (m_second == nullptr || m_best->getProb() > m_second->getProb() * m_params.acceptBestRelative // Prefixed all
            || m_best->getProb() > m_second->getProb() + m_params.acceptBestAbsolute) { // Prefixed all
            for (auto &path_ptr : *m_paths) { // Prefixed paths
                path_ptr->deny(changes); // Pass changes
            }
            m_paths->clear(); // Prefixed
            m_paths->push_front(m_best); // Prefixed
        } else {
            m_paths->push_back(m_best); // Prefixed

            for (std::shared_ptr<Path> working = m_paths->front(); working != m_best;) { // Prefixed paths, best
                m_paths->pop_front(); // Prefixed
                // throw away if the probability is very low or not
                // distinguishable from best. Don't keep paths with equal
                // probability at the front, for we need to find a unique
                // best path eventually.
                if (m_best->getProb() > working->getProb() * m_params.maxPaths / m_numPaths // Prefixed best, params, numPaths
                    || (m_best->getProb() <= working->getProb() // Prefixed best
                        && m_best->getRoom() == working->getRoom())) { // Prefixed best
                    working->deny(changes); // Pass changes
                } else {
                    m_paths->push_back(working); // Prefixed
                }
                working = m_paths->front(); // Prefixed
            }
        }
    }
    m_second = nullptr; // Prefixed
    m_shortPaths = nullptr; // Prefixed
    m_best = nullptr; // Prefixed
    return m_paths; // Prefixed
}
