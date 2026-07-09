// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "ExitDirection.h"

#include "../global/Array.h"
#include "../global/Charset.h"
#include "../global/Consts.h"
#include "../global/EnumIndexedArray.h"
#include "../global/enums.h"
#include "coordinate.h"

namespace enums {
View<ExitDirEnum> getAllExitsNESW()
{
    static constexpr auto g_all_exits = genEnumValues<ExitDirEnum, NUM_EXITS_NESW>();
    static const auto view = View{g_all_exits};
    return view;
}
View<ExitDirEnum> getAllExitsNESWUD()
{
    static constexpr auto g_all_exits = genEnumValues<ExitDirEnum, NUM_EXITS_NESWUD>();
    static const auto view = View{g_all_exits};
    return view;
}
View<ExitDirEnum> getAllExits7()
{
    static constexpr auto g_all_exits = genEnumValues<ExitDirEnum, NUM_EXITS>();
    static const auto view = View{g_all_exits};
    return view;
}

} // namespace enums

bool isNESW(const ExitDirEnum dir)
{
    switch (dir) {
    case ExitDirEnum::NORTH:
    case ExitDirEnum::SOUTH:
    case ExitDirEnum::EAST:
    case ExitDirEnum::WEST:
        return true;

    case ExitDirEnum::UP:
    case ExitDirEnum::DOWN:
    case ExitDirEnum::UNKNOWN:
    case ExitDirEnum::NONE:
    default:
        return false;
    }
}
bool isUpDown(const ExitDirEnum dir)
{
    switch (dir) {
    case ExitDirEnum::UP:
    case ExitDirEnum::DOWN:
        return true;

    case ExitDirEnum::NORTH:
    case ExitDirEnum::SOUTH:
    case ExitDirEnum::EAST:
    case ExitDirEnum::WEST:
    case ExitDirEnum::UNKNOWN:
    case ExitDirEnum::NONE:
    default:
        return false;
    }
}

bool isNESWUD(const ExitDirEnum dir)
{
    switch (dir) {
    case ExitDirEnum::NORTH:
    case ExitDirEnum::SOUTH:
    case ExitDirEnum::EAST:
    case ExitDirEnum::WEST:
    case ExitDirEnum::UP:
    case ExitDirEnum::DOWN:
        return true;

    case ExitDirEnum::UNKNOWN:
    case ExitDirEnum::NONE:
        break;
    }
    return false;
}

// TODO: merge this with other implementations
ExitDirEnum opposite(const ExitDirEnum in)
{
#define PAIR(A, B) \
    do { \
    case ExitDirEnum::A: \
        return ExitDirEnum::B; \
    case ExitDirEnum::B: \
        return ExitDirEnum::A; \
    } while (false)
    switch (in) {
        PAIR(NORTH, SOUTH);
        PAIR(WEST, EAST);
        PAIR(UP, DOWN);
    case ExitDirEnum::UNKNOWN:
    case ExitDirEnum::NONE:
        break;
    }
    return ExitDirEnum::UNKNOWN;
#undef PAIR
}

const char *lowercaseDirection(const ExitDirEnum dir)
{
#define X_CASE(UPPER, lower) \
    case ExitDirEnum::UPPER: \
        return #lower
    switch (dir) {
        X_CASE(NORTH, north);
        X_CASE(SOUTH, south);
        X_CASE(EAST, east);
        X_CASE(WEST, west);
        X_CASE(UP, up);
        X_CASE(DOWN, down);
        X_CASE(UNKNOWN, unknown);
    default:
        X_CASE(NONE, none);
    }
#undef X_CASE
}

namespace Mmapper2Exit {

ExitDirEnum dirForChar(const char dir)
{
    switch (dir) {
    case 'n':
        return ExitDirEnum::NORTH;
    case 's':
        return ExitDirEnum::SOUTH;
    case 'e':
        return ExitDirEnum::EAST;
    case 'w':
        return ExitDirEnum::WEST;
    case 'u':
        return ExitDirEnum::UP;
    case 'd':
        return ExitDirEnum::DOWN;
    default:
        return ExitDirEnum::UNKNOWN;
    }
}

ExitDirEnum dirForChar(const QChar dir)
{
    return dirForChar(mmqt::toLatin1(dir));
}

char charForDir(const ExitDirEnum dir)
{
    switch (dir) {
    case ExitDirEnum::NORTH:
        return 'n';
    case ExitDirEnum::SOUTH:
        return 's';
    case ExitDirEnum::EAST:
        return 'e';
    case ExitDirEnum::WEST:
        return 'w';
    case ExitDirEnum::UP:
        return 'u';
    case ExitDirEnum::DOWN:
        return 'd';
    case ExitDirEnum::UNKNOWN:
    case ExitDirEnum::NONE:
        break;
    }
    return char_consts::C_QUESTION_MARK;
}
} // namespace Mmapper2Exit

using ExitCoordinates = EnumIndexedArray<Coordinate, ExitDirEnum, NUM_EXITS_INCLUDING_NONE>;
NODISCARD static ExitCoordinates initExitCoordinates()
{
    ExitCoordinates exitDirs;
    exitDirs[ExitDirEnum::NORTH] = Coordinate(0, 1, 0);
    exitDirs[ExitDirEnum::SOUTH] = Coordinate(0, -1, 0);
    exitDirs[ExitDirEnum::EAST] = Coordinate(1, 0, 0);
    exitDirs[ExitDirEnum::WEST] = Coordinate(-1, 0, 0);
    exitDirs[ExitDirEnum::UP] = Coordinate(0, 0, 1);
    exitDirs[ExitDirEnum::DOWN] = Coordinate(0, 0, -1);
    return exitDirs;
}

Coordinate exitDir(const ExitDirEnum dir)
{
    static const auto exitDirs = initExitCoordinates();
    return exitDirs[dir];
}

/*
 * Snap zones within a room coordinate (fract(x), fract(y)) in [0..1, 0..1]:
 *
 * (0,1)             (1,1)
 *    +---------------+
 *    | \     N     / |
 *    |   \   U   /   |  U = UP (Center 0.75, 0.75, radius 0.15)
 *    |     \   /     |
 *    |  W    X    E  |  X = UNKNOWN (Center 0.5, 0.5, radius 0.15)
 *    |     /   \     |
 *    |   /   D   \   |  D = DOWN (Center 0.25, 0.25, radius 0.15)
 *    | /     S     \ |
 *    +---------------+
 * (0,0)             (1,0)
 *
 * Partitioned by diagonals:
 * ne = x >= 1-y (North-East half)
 * nw = x <= y   (North-West half)
 *
 * (ne && nw)   => NORTH
 * (ne && !nw)  => EAST
 * (!ne && nw)  => WEST
 * (!ne && !nw) => SOUTH
 *
 * Circle-based zones (U, D, X) take precedence over the quadrant diagonals.
 * This logic is moved here from ConnectionSelection to allow unit testing and
 * potential reuse by other UI components needing room-relative direction snapping.
 */
ExitDirEnum getExitDirFromCoordinate(const Coordinate2f &c)
{
    const glm::vec2 pos = glm::fract(c.to_vec2());
    const glm::vec2 upCenter{0.75f, 0.75f};
    const glm::vec2 downCenter{0.25f, 0.25f};
    const glm::vec2 actualCenter{0.5f, 0.5f};
    const float upDownRadius = 0.15f;
    const float centerRadius = 0.15f;

    if (glm::distance(pos, upCenter) <= upDownRadius) {
        return ExitDirEnum::UP;
    } else if (glm::distance(pos, downCenter) <= upDownRadius) {
        return ExitDirEnum::DOWN;
    } else if (glm::distance(pos, actualCenter) <= centerRadius) {
        return ExitDirEnum::UNKNOWN;
    }

    const bool ne = pos.x >= 1.f - pos.y;
    const bool nw = pos.x <= pos.y;
    return ne ? (nw ? ExitDirEnum::NORTH : ExitDirEnum::EAST)
              : (nw ? ExitDirEnum::WEST : ExitDirEnum::SOUTH);
}

std::string_view to_string_view(const ExitDirEnum dir)
{
    return lowercaseDirection(dir);
}

void test::testExitDirection()
{
    // Test getExitDirFromCoordinate
    // North: fractal pos (0.5, 0.9)
    assert(getExitDirFromCoordinate(Coordinate2f{0.5f, 0.9f}) == ExitDirEnum::NORTH);
    // South: fractal pos (0.5, 0.1)
    assert(getExitDirFromCoordinate(Coordinate2f{0.5f, 0.1f}) == ExitDirEnum::SOUTH);
    // East: fractal pos (0.9, 0.5)
    assert(getExitDirFromCoordinate(Coordinate2f{0.9f, 0.5f}) == ExitDirEnum::EAST);
    // West: fractal pos (0.1, 0.5)
    assert(getExitDirFromCoordinate(Coordinate2f{0.1f, 0.5f}) == ExitDirEnum::WEST);
    // Up: near (0.75, 0.75)
    assert(getExitDirFromCoordinate(Coordinate2f{0.75f, 0.75f}) == ExitDirEnum::UP);
    // Down: near (0.25, 0.25)
    assert(getExitDirFromCoordinate(Coordinate2f{0.25f, 0.25f}) == ExitDirEnum::DOWN);
    // Unknown: near (0.5, 0.5)
    assert(getExitDirFromCoordinate(Coordinate2f{0.5f, 0.5f}) == ExitDirEnum::UNKNOWN);
}
