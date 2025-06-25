// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021-2024 The MMapper Authors

#include "RoomIdSet.h" // This now defines BasicSetWrapper and aliases RoomIdSet
#include "../global/BppOrderedSet.h" // For BppOrderedSet constructor/assignment test with BasicSetWrapper
#include "../global/tests.h"   // For TEST_ASSERT
#include <stdexcept>           // For std::out_of_range checks for first/last

namespace test {
void testRoomIdSet()
{
    RoomIdSet a; // Actually BasicSetWrapper<RoomId>
    RoomIdSet b;

    TEST_ASSERT(a == b);
    TEST_ASSERT(!a.contains(RoomId(1)));
    TEST_ASSERT(a.empty());
    TEST_ASSERT(a.size() == 0);

    // Test insert
    a.insert(RoomId(1));
    TEST_ASSERT(a.contains(RoomId(1)));
    TEST_ASSERT(!a.empty());
    TEST_ASSERT(a.size() == 1);
    TEST_ASSERT(a != b);

    // Test first() and last() on single element set
    TEST_ASSERT(a.first() == RoomId(1));
    TEST_ASSERT(a.last() == RoomId(1));

    b.insert(RoomId(1));
    TEST_ASSERT(!a.containsElementNotIn(b)); // a={1}, b={1}
    TEST_ASSERT(a == b);

    a.insert(RoomId(7)); // a={1,7}, b={1}
    TEST_ASSERT(a.contains(RoomId(7)));
    TEST_ASSERT(a.size() == 2);
    TEST_ASSERT(a.first() == RoomId(1));
    TEST_ASSERT(a.last() == RoomId(7));
    TEST_ASSERT(a.containsElementNotIn(b));
    TEST_ASSERT(a != b);

    b.insert(RoomId(7)); // a={1,7}, b={1,7}
    TEST_ASSERT(!a.containsElementNotIn(b));
    TEST_ASSERT(a == b);

    // Test erase
    b.erase(RoomId(7)); // a={1,7}, b={1}
    TEST_ASSERT(!b.contains(RoomId(7)));
    TEST_ASSERT(b.size() == 1);
    TEST_ASSERT(a.containsElementNotIn(b));
    TEST_ASSERT(a != b);

    // Test initializer list constructor for BasicSetWrapper
    RoomIdSet c = {RoomId(10), RoomId(20)};
    TEST_ASSERT(c.contains(RoomId(10)));
    TEST_ASSERT(c.contains(RoomId(20)));
    TEST_ASSERT(c.size() == 2);
    TEST_ASSERT(c.first() == RoomId(10));
    TEST_ASSERT(c.last() == RoomId(20));

    // Test initializer list assignment
    c = {RoomId(30), RoomId(40), RoomId(50)};
    TEST_ASSERT(c.contains(RoomId(30)));
    TEST_ASSERT(c.contains(RoomId(40)));
    TEST_ASSERT(c.contains(RoomId(50)));
    TEST_ASSERT(c.size() == 3);
    TEST_ASSERT(!c.contains(RoomId(10)));
    TEST_ASSERT(c.first() == RoomId(30));
    TEST_ASSERT(c.last() == RoomId(50));

    // Reset b to {1}
    b = {RoomId(1)}; // Re-assign b using initializer list
    TEST_ASSERT(b.size() == 1);
    TEST_ASSERT(b.contains(RoomId(1)));

    b.insert(RoomId(8)); // b={1,8}, a={1,7}
    TEST_ASSERT(a.containsElementNotIn(b));
    TEST_ASSERT(b.containsElementNotIn(a));
    TEST_ASSERT(a != b);

    b.insert(RoomId(7)); // b={1,7,8}, a={1,7}
    TEST_ASSERT(!a.containsElementNotIn(b));
    TEST_ASSERT(b.containsElementNotIn(a));
    TEST_ASSERT(a != b);

    // Test clear()
    RoomIdSet to_clear = {RoomId(100), RoomId(200)};
    TEST_ASSERT(!to_clear.empty());
    to_clear.clear();
    TEST_ASSERT(to_clear.empty());
    TEST_ASSERT(to_clear.size() == 0);
    bool thrown_first = false;
    try { to_clear.first(); } catch (const std::out_of_range&) { thrown_first = true; }
    TEST_ASSERT(thrown_first);

    bool thrown_last = false;
    try { to_clear.last(); } catch (const std::out_of_range&) { thrown_last = true; }
    TEST_ASSERT(thrown_last);

    // Test insertAll()
    RoomIdSet set1 = {RoomId(1), RoomId(2)};
    RoomIdSet set2 = {RoomId(2), RoomId(3)};
    set1.insertAll(set2);
    TEST_ASSERT(set1.size() == 3);
    TEST_ASSERT(set1.contains(RoomId(1)));
    TEST_ASSERT(set1.contains(RoomId(2)));
    TEST_ASSERT(set1.contains(RoomId(3)));
    TEST_ASSERT(set1.first() == RoomId(1));
    TEST_ASSERT(set1.last() == RoomId(3));

    RoomIdSet empty_set_for_insertall;
    set1.insertAll(empty_set_for_insertall);
    TEST_ASSERT(set1.size() == 3);

    empty_set_for_insertall.insertAll(set1);
    TEST_ASSERT(empty_set_for_insertall.size() == 3);
    TEST_ASSERT(empty_set_for_insertall.contains(RoomId(1)));

    RoomIdSet set3 = {RoomId(4), RoomId(5)};
    RoomIdSet set4 = {RoomId(1), RoomId(6)};
    set3.insertAll(set4);
    TEST_ASSERT(set3.size() == 4);
    TEST_ASSERT(set3.first() == RoomId(1));
    TEST_ASSERT(set3.last() == RoomId(6));

    RoomIdSet self_insert_set = {RoomId(77), RoomId(88)};
    self_insert_set.insertAll(self_insert_set);
    TEST_ASSERT(self_insert_set.size() == 2);

    // Test containsElementNotIn further
    RoomIdSet sA = {RoomId(1), RoomId(2), RoomId(3)};
    RoomIdSet sB = {RoomId(2), RoomId(3), RoomId(4)};
    TEST_ASSERT(sA.containsElementNotIn(sB));
    TEST_ASSERT(sB.containsElementNotIn(sA));
    RoomIdSet sC = {RoomId(2), RoomId(3)};
    TEST_ASSERT(sA.containsElementNotIn(sC));
    TEST_ASSERT(!sC.containsElementNotIn(sA));
    RoomIdSet sD = {RoomId(1), RoomId(2), RoomId(3)};
    TEST_ASSERT(!sA.containsElementNotIn(sD));
    TEST_ASSERT(!sD.containsElementNotIn(sA));

    // Test construction of BasicSetWrapper from BppOrderedSet
    BppOrderedSet<RoomId> base_bpp_set_payload;
    base_bpp_set_payload.insert(RoomId(500));
    base_bpp_set_payload.insert(RoomId(600));
    RoomIdSet from_bpp_wrapper(base_bpp_set_payload);
    TEST_ASSERT(from_bpp_wrapper.size() == 2);
    TEST_ASSERT(from_bpp_wrapper.contains(RoomId(500)));
    TEST_ASSERT(from_bpp_wrapper.first() == RoomId(500));
    TEST_ASSERT(from_bpp_wrapper.last() == RoomId(600));

    // Test assignment of BasicSetWrapper from BppOrderedSet
    BppOrderedSet<RoomId> another_bpp_payload;
    another_bpp_payload.insert(RoomId(700));
    from_bpp_wrapper = another_bpp_payload;
    TEST_ASSERT(from_bpp_wrapper.size() == 1);
    TEST_ASSERT(from_bpp_wrapper.contains(RoomId(700)));
    TEST_ASSERT(from_bpp_wrapper.first() == RoomId(700));
}
} // namespace test
