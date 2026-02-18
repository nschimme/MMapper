// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "TF.h"

#include "Legacy.h"

namespace Legacy {

void TF::emplace(const SharedFunctions &sharedFunctions)
{
    reset();
    m_weakFunctions = sharedFunctions;
    sharedFunctions->glGenTransformFeedbacks(1, &m_tf);
}

void TF::reset()
{
    if (m_tf != INVALID_TFID) {
        if (auto shared = m_weakFunctions.lock()) {
            shared->glDeleteTransformFeedbacks(1, &m_tf);
        }
        m_tf = INVALID_TFID;
    }
}

GLuint TF::get() const
{
    return m_tf;
}

} // namespace Legacy
