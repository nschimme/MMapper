#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../../global/EnumIndexedArray.h"
#include "Legacy.h"

#include <memory>

namespace Legacy {

class NODISCARD TF final
{
private:
    static inline constexpr GLuint INVALID_TFID = 0;
    WeakFunctions m_weakFunctions;
    GLuint m_tf = INVALID_TFID;

public:
    TF() = default;
    ~TF() { reset(); }

    DELETE_CTORS_AND_ASSIGN_OPS(TF);

public:
    void emplace(const SharedFunctions &sharedFunctions);
    void reset();
    NODISCARD GLuint get() const;

public:
    NODISCARD explicit operator bool() const { return m_tf != INVALID_TFID; }
};

using SharedTf = std::shared_ptr<TF>;
using WeakTf = std::weak_ptr<TF>;

class NODISCARD SharedTransformFeedbacks final
    : private EnumIndexedArray<SharedTf, SharedTfEnum, NUM_SHARED_TFS>
{
private:
    using base = EnumIndexedArray<SharedTf, SharedTfEnum, NUM_SHARED_TFS>;

public:
    SharedTransformFeedbacks() = default;

public:
    NODISCARD SharedTf get(const SharedTfEnum tf)
    {
        SharedTf &shared = base::operator[](tf);
        if (shared == nullptr) {
            shared = std::make_shared<TF>();
        }
        return shared;
    }

    void reset(const SharedTfEnum tf) { base::operator[](tf).reset(); }

    void resetAll()
    {
        base::for_each([](auto &shared) { shared.reset(); });
    }
};

} // namespace Legacy
