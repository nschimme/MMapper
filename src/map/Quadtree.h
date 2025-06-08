#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../global/macros.h"
#include "roomid.h" // For RoomId
#include <vector>
#include <memory> // For std::unique_ptr

// Forward declaration
class Quadtree;

struct RoomBounds {
    float x, y, width, height;

    RoomBounds(float _x = 0, float _y = 0, float _w = 0, float _h = 0)
        : x(_x), y(_y), width(_w), height(_h) {}

    bool contains(const RoomBounds& other) const {
        return other.x >= x && other.y >= y &&
               other.x + other.width <= x + width &&
               other.y + other.height <= y + height;
    }

    bool intersects(const RoomBounds& other) const {
        return !(other.x > x + width ||
                 other.x + other.width < x ||
                 other.y > y + height ||
                 other.y + other.height < y);
    }
};

struct RoomEntry {
    RoomId id;
    RoomBounds bounds;
};

class QuadtreeNode {
public:
    QuadtreeNode(RoomBounds bounds, int level, Quadtree* tree);
    ~QuadtreeNode();

    void clear();
    void split();
    void insert(const RoomEntry& item);
    void queryRange(const RoomBounds& range, std::vector<RoomId>& foundItems) const;
    bool remove(RoomId id, const RoomBounds& itemBounds);

    const RoomBounds& getBounds() const { return m_bounds; }

private:
    int getIndex(const RoomBounds& pRect) const;
    void redistribute(); // Helper to move items to children after split

    RoomBounds m_bounds;
    std::vector<RoomEntry> m_items;
    std::unique_ptr<QuadtreeNode> m_children[4];
    bool m_isLeaf = true;
    int m_level;
    Quadtree* m_tree; // Parent tree for config
};

class Quadtree {
public:
    Quadtree(RoomBounds bounds, int maxItemsPerNode = 4, int maxLevels = 8);
    ~Quadtree();

    void insert(RoomId id, const RoomBounds& bounds);
    std::vector<RoomId> queryRange(const RoomBounds& range) const;
    bool remove(RoomId id, const RoomBounds& bounds);
    void clear();

    // Make these accessible to QuadtreeNode
    int getMaxItemsPerNode() const { return m_maxItemsPerNode; }
    int getMaxLevels() const { return m_maxLevels; }

private:
    friend class QuadtreeNode;
    std::unique_ptr<QuadtreeNode> m_root;
    int m_maxItemsPerNode;
    int m_maxLevels;
};
