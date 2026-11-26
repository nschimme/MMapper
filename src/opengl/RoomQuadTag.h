#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <cstdint>

// Explore mode meshes every secret exit twice, as the wall it looks like until it is found and as
// the door it is afterwards, and the room shader keeps the form that matches. The two forms are
// told apart by a tag in the free high bits of the instance word RoomQuadTexVert::vertTexCol.w:
//
//   bits  0-7   tex_z    (already in use)
//   bits  8-15  colorId  (already in use)
//   bits 16-17  form     0 = untagged, 1 = door form, 2 = wall form
//   bits 18-20  dir      ExitDirEnum NORTH..DOWN (0-5), only meaningful when tagged
//
// A word without a tag is exactly what it was before, so every other quad is unchanged.
namespace room_quad_tag {

enum class NODISCARD FormEnum : uint8_t { NONE = 0, DOOR = 1, WALL = 2 };

inline constexpr int FORM_SHIFT = 16;
inline constexpr int FORM_MASK = 0x3;
inline constexpr int DIR_SHIFT = 18;
inline constexpr int DIR_MASK = 0x7;
inline constexpr int TAG_BITS = (FORM_MASK << FORM_SHIFT) | (DIR_MASK << DIR_SHIFT);

// `dir` is the numeric value of an ExitDirEnum from NORTH to DOWN.
NODISCARD constexpr int pack(const FormEnum form, const unsigned dir)
{
    return form == FormEnum::NONE
               ? 0
               : ((static_cast<int>(form) << FORM_SHIFT) | (static_cast<int>(dir) << DIR_SHIFT));
}

NODISCARD constexpr FormEnum formOf(const int word)
{
    return static_cast<FormEnum>((word >> FORM_SHIFT) & FORM_MASK);
}

NODISCARD constexpr unsigned dirOf(const int word)
{
    return static_cast<unsigned>((word >> DIR_SHIFT) & DIR_MASK);
}

// What the shader reads for the other two fields: the tag bits must not leak into them.
NODISCARD constexpr int texZOf(const int word)
{
    return word & 0xFF;
}

NODISCARD constexpr int colorIdOf(const int word)
{
    return (word >> 8) & 0xFF;
}

} // namespace room_quad_tag
