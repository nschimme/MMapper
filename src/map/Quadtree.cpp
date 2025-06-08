// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "Quadtree.h"
#include <algorithm> // For std::remove_if

// --- QuadtreeNode Implementation ---
QuadtreeNode::QuadtreeNode(RoomBounds bounds, int level, Quadtree* tree)
    : m_bounds(bounds), m_level(level), m_tree(tree) {
    for (int i = 0; i < 4; ++i) {
        m_children[i] = nullptr;
    }
}

QuadtreeNode::~QuadtreeNode() = default; // unique_ptr handles children

void QuadtreeNode::clear() {
    m_items.clear();
    if (!m_isLeaf) {
        for (int i = 0; i < 4; ++i) {
            if (m_children[i]) {
                m_children[i]->clear();
                m_children[i].reset(); // Release memory
            }
        }
        m_isLeaf = true;
    }
}

void QuadtreeNode::split() {
    if (!m_isLeaf || m_level + 1 >= m_tree->getMaxLevels()) {
        // Cannot split if not a leaf or if max depth would be exceeded
        return;
    }

    float subWidth = m_bounds.width / 2.0f;
    float subHeight = m_bounds.height / 2.0f;
    float x = m_bounds.x;
    float y = m_bounds.y;
    int nextLevel = m_level + 1;

    m_children[0] = std::make_unique<QuadtreeNode>(RoomBounds(x + subWidth, y, subWidth, subHeight), nextLevel, m_tree);             // Top right
    m_children[1] = std::make_unique<QuadtreeNode>(RoomBounds(x, y, subWidth, subHeight), nextLevel, m_tree);                         // Top left
    m_children[2] = std::make_unique<QuadtreeNode>(RoomBounds(x, y + subHeight, subWidth, subHeight), nextLevel, m_tree);             // Bottom left
    m_children[3] = std::make_unique<QuadtreeNode>(RoomBounds(x + subWidth, y + subHeight, subWidth, subHeight), nextLevel, m_tree); // Bottom right

    m_isLeaf = false;
    redistribute();
}

void QuadtreeNode::redistribute() {
    std::vector<RoomEntry> tempItems = std::move(m_items);
    m_items.clear(); // Ensure m_items is empty before re-inserting
    for (const auto& item : tempItems) {
        insert(item); // Re-insert items, they will go to children if node is split
    }
}

int QuadtreeNode::getIndex(const RoomBounds& pRect) const {
    int index = -1;
    float verticalMidpoint = m_bounds.x + (m_bounds.width / 2.0f);
    float horizontalMidpoint = m_bounds.y + (m_bounds.height / 2.0f);

    // Object can completely fit within the top quadrants
    bool topQuadrant = (pRect.y < horizontalMidpoint && pRect.y + pRect.height < horizontalMidpoint);
    // Object can completely fit within the bottom quadrants
    bool bottomQuadrant = (pRect.y > horizontalMidpoint);

    // Object can completely fit within the left quadrants
    if (pRect.x < verticalMidpoint && pRect.x + pRect.width < verticalMidpoint) {
        if (topQuadrant) {
            index = 1; // Top left child
        } else if (bottomQuadrant) {
            index = 2; // Bottom left child
        }
    }
    // Object can completely fit within the right quadrants
    else if (pRect.x > verticalMidpoint) {
        if (topQuadrant) {
            index = 0; // Top right child
        } else if (bottomQuadrant) {
            index = 3; // Bottom right child
        }
    }
    return index;
}

void QuadtreeNode::insert(const RoomEntry& item) {
    if (!m_bounds.contains(item.bounds) && !m_bounds.intersects(item.bounds)) {
        // If the item is completely outside this node's bounds, do not insert.
        // This check is more of a safeguard, primary bounds check should happen at Quadtree::insert
        return;
    }

    if (!m_isLeaf) {
        int index = getIndex(item.bounds);
        if (index != -1) {
            m_children[index]->insert(item);
            return;
        }
    }

    m_items.push_back(item);

    if (m_isLeaf && m_items.size() > static_cast<size_t>(m_tree->getMaxItemsPerNode()) && m_level < m_tree->getMaxLevels()) {
        split();
    }
}

void QuadtreeNode::queryRange(const RoomBounds& range, std::vector<RoomId>& foundItems) const {
    if (!m_bounds.intersects(range)) {
        return;
    }

    for (const auto& item : m_items) {
        if (range.intersects(item.bounds)) {
            foundItems.push_back(item.id);
        }
    }

    if (!m_isLeaf) {
        for (int i = 0; i < 4; ++i) {
            if (m_children[i]) {
                 m_children[i]->queryRange(range, foundItems);
            }
        }
    }
}

bool QuadtreeNode::remove(RoomId id, const RoomBounds& itemBounds) {
    if (!m_bounds.intersects(itemBounds)) {
        return false;
    }

    bool removed = false;
    m_items.erase(std::remove_if(m_items.begin(), m_items.end(),
                                 [&](const RoomEntry& item) {
                                     if (item.id == id) {
                                         removed = true;
                                         return true;
                                     }
                                     return false;
                                 }),
                  m_items.end());

    if (removed) return true;

    if (!m_isLeaf) {
        int index = getIndex(itemBounds);
        if (index != -1) {
            if (m_children[index] && m_children[index]->remove(id, itemBounds)) {
                return true;
            }
        } else { // Item could overlap multiple children or be in this node (if it couldn't fit in a child)
            for (int i = 0; i < 4; ++i) {
                if (m_children[i] && m_children[i]->remove(id, itemBounds)) {
                    return true;
                }
            }
        }
    }
    return false;
}

// --- Quadtree Implementation ---
Quadtree::Quadtree(RoomBounds bounds, int maxItemsPerNode, int maxLevels)
    : m_maxItemsPerNode(maxItemsPerNode), m_maxLevels(maxLevels) {
    m_root = std::make_unique<QuadtreeNode>(bounds, 0, this);
}

Quadtree::~Quadtree() = default; // unique_ptr handles root

void Quadtree::clear() {
    if (m_root) {
        m_root->clear();
    }
}

void Quadtree::insert(RoomId id, const RoomBounds& bounds) {
    if (m_root && m_root->getBounds().contains(bounds)) { // Ensure item is within root bounds
         m_root->insert(RoomEntry{id, bounds});
    } else if (m_root && m_root->getBounds().intersects(bounds)) {
        // Item is partially in bounds, also insert. Or handle as an error/special case.
        // For simplicity, we'll insert if it intersects. More robust handling might be needed.
        m_root->insert(RoomEntry{id, bounds});
    }
    // Else: item is completely outside the quadtree's defined bounds, do nothing or log error
}

std::vector<RoomId> Quadtree::queryRange(const RoomBounds& range) const {
    std::vector<RoomId> foundItems;
    if (m_root) {
        m_root->queryRange(range, foundItems);
    }
    return foundItems;
}

bool Quadtree::remove(RoomId id, const RoomBounds& bounds) {
    if (m_root) {
        return m_root->remove(id, bounds);
    }
    return false;
}
