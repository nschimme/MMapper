#ifndef MAP_WORLD_COMPARE_DETAIL_H
#define MAP_WORLD_COMPARE_DETAIL_H

#include "RawExit.h"
#include "RawRoom.h"
#include "RoomFields.h"

namespace map_compare_detail {

inline bool hasMeshDifference(const RawExit &a, const RawExit &b)
{
    // Compare relevant members of RawExit that affect mesh generation
    if (a.fields.exitFlags != b.fields.exitFlags) {
        return true;
    }
    if (a.fields.doorFlags != b.fields.doorFlags) {
        return true;
    }
    // Door name change is not a mesh difference (a.fields.doorName != b.fields.doorName)

    // If it's a flow exit, and the set of connected rooms changes, it's a visual difference
    // as the stream line will change.
    // We check a.fields.exitFlags.isFlow() because if b gained/lost the isFlow property,
    // the exitFlags check would already have caught it.
    if (a.fields.exitFlags.isFlow()) {
        if (a.outgoing != b.outgoing) {
            return true;
        }
    }
    // Note: a.incoming != b.incoming is not checked here as 'incoming' changes
    // on their own don't change the visual representation of *this* exit directly.
    // Changes to the other side's 'outgoing' (which becomes this side's 'incoming')
    // will be caught when that other room's exit is processed.

    return false;
}

inline bool hasMeshDifference(const RawRoom::Exits &a, const RawRoom::Exits &b)
{
    if (a.size() != b.size()) {
        return true;
    }
    for (const auto dir : ALL_EXITS7) {
        if (hasMeshDifference(a[dir], b[dir])) {
            return true;
        }
    }
    return false;
}

inline bool hasMeshDifference(const RoomFields &a, const RoomFields &b)
{
    // Compare relevant members of RoomFields that affect mesh generation
    return a.TerrainType != b.TerrainType || a.LoadFlags != b.LoadFlags;
}

inline bool hasMeshDifference(const RawRoom &a, const RawRoom &b)
{
    if (hasMeshDifference(static_cast<const RoomFields &>(a.fields),
                          static_cast<const RoomFields &>(b.fields))) {
        return true;
    }
    if (hasMeshDifference(a.exits, b.exits)) {
        return true;
    }
    // Compare other RawRoom specific members if they affect mesh
    // For example, if m_objects changes could affect mesh (e.g. static meshes)
    // if (a.m_objects.size() != b.m_objects.size()) return true;
    // for (size_t i = 0; i < a.m_objects.size(); ++i) {
    //    if (/* relevant comparison for objects */) return true;
    // }
    return false;
}

} // namespace map_compare_detail

#endif // MAP_WORLD_COMPARE_DETAIL_H
