#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../../global/EnumIndexedArray.h"
#include "Legacy.h"

#include <memory>

namespace Legacy {

class NODISCARD TFO final
{
private:
    static inline constexpr GLuint INVALID_TFO = 0;
    WeakFunctions m_weakFunctions;
    GLuint m_tfo = INVALID_TFO;

public:
    TFO() = default;
    ~TFO() { reset(); }

    DELETE_CTORS_AND_ASSIGN_OPS(TFO);

public:
    void emplace(const SharedFunctions &sharedFunctions);
    void reset();
    NODISCARD GLuint get() const;

public:
    NODISCARD explicit operator bool() const { return m_tfo != INVALID_TFO; }
};

using SharedTfo = std::shared_ptr<TFO>;
using WeakTfo = std::weak_ptr<TFO>;

class NODISCARD SharedTfos final : private EnumIndexedArray<SharedTfo, SharedTfEnum, NUM_SHARED_TFS>
{
private:
    using base = EnumIndexedArray<SharedTfo, SharedTfEnum, NUM_SHARED_TFS>;

public:
    SharedTfos() = default;

public:
    NODISCARD SharedTfo get(const SharedTfEnum tf)
    {
        SharedTfo &shared = base::operator[](tf);
        if (shared == nullptr) {
            shared = std::make_shared<TFO>();
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
