// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2021 The MMapper Authors

#include "TinyRoomIdSet.h"

#include "../global/tests.h"
#include "RoomIdSet.h"

#include <array>
#include <set>

template<typename T,
         typename U,
         typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, std::decay_t<U>>>>
static T convertTo(const U &input)
{
    T copy;
    for (const auto &x : input) {
        copy.insert(x);
    }
    return copy;
}

RoomIdSet toRoomIdSet(const TinyRoomIdSet &set)
{
    return convertTo<RoomIdSet>(set);
}

TinyRoomIdSet toTinyRoomIdSet(const RoomIdSet &set)
{
    return convertTo<TinyRoomIdSet>(set);
}

template<typename Set>
struct NODISCARD TestHelper final
{
public:
    TestHelper() = delete;

public:
    static void test()
    {
        test0();
        test1();
    }

private:
    static void test0()
    {
        constexpr RoomId VAL{42};

        Set set;
        set.insert(VAL);
        TEST_ASSERT(set.size() == 1);
        TEST_ASSERT(set.first() == VAL);
        set.erase(VAL);
        TEST_ASSERT(set.size() == 0);
        TEST_ASSERT(set.empty());
    }

    static void test1()
    {
        constexpr size_t SIZE = 5;
        constexpr size_t KEEP = 1;
        static_assert(SIZE > KEEP);

        Set set;
        for (uint32_t i = 0; i < SIZE; ++i) {
            set.insert(RoomId{i});
            TEST_ASSERT(set.size() == i + 1);
        }

        TEST_ASSERT(set.size() == SIZE);

        {
            std::array<bool, SIZE> found{};
            for (const RoomId x : set) {
                found.at(x.asUint32()) = true;
            }

            for (const bool x : found) {
                TEST_ASSERT(x);
            }
        }

        for (uint32_t i = 0; i < SIZE - KEEP; ++i) {
            set.erase(RoomId{i});
        }

        TEST_ASSERT(set.size() == KEEP);
        {
            Set tmp;
            for (const RoomId x : set) {
                tmp.insert(x);
            }
            TEST_ASSERT(tmp == set);
        }
    }
};

namespace test {

// Helper to check contents, useful for COW tests
static bool checkContents(const TinyRoomIdSet& set, const std::vector<RoomId>& expected_elements) {
    if (set.size() != expected_elements.size()) return false;
    for (const RoomId& elem : expected_elements) {
        if (!set.contains(elem)) return false;
    }
    // Also check iteration
    std::set<RoomId> iterated_elements;
    for (RoomId r : set) {
        iterated_elements.insert(r);
    }
    if (iterated_elements.size() != expected_elements.size()) return false;
    for (const RoomId& elem : expected_elements) {
        if (iterated_elements.find(elem) == iterated_elements.end()) return false;
    }
    return true;
}

static void testTinyRoomIdSet_CopyOnWrite()
{
    // Test COW for sets that become "big"
    TinyRoomIdSet set1;
    set1.insert(RoomId{1});
    set1.insert(RoomId{2}); // set1 is now big {1, 2}

    TinyRoomIdSet set2 = set1; // Copy constructor, should share data initially
    TEST_ASSERT(set1 == set2);
    TEST_ASSERT(checkContents(set1, std::vector<RoomId>{{1}, {2}}));
    TEST_ASSERT(checkContents(set2, std::vector<RoomId>{{1}, {2}}));

    set1.insert(RoomId{3}); // Modify set1, COW should occur
    TEST_ASSERT(checkContents(set1, std::vector<RoomId>{{1}, {2}, {3}})); // set1 is {1, 2, 3}
    TEST_ASSERT(checkContents(set2, std::vector<RoomId>{{1}, {2}}));     // set2 should remain {1, 2}
    TEST_ASSERT(set1 != set2);

    TinyRoomIdSet set3;
    set3.insert(RoomId{10});
    set3.insert(RoomId{20}); // set3 is big {10, 20}
    TinyRoomIdSet set4;
    set4 = set3; // Copy assignment, should share data
    TEST_ASSERT(set3 == set4);
    TEST_ASSERT(checkContents(set4, std::vector<RoomId>{{10}, {20}}));

    set3.erase(RoomId{10}); // Modify set3, COW should occur
    TEST_ASSERT(checkContents(set3, std::vector<RoomId>{{20}}));      // set3 is {20}
    TEST_ASSERT(checkContents(set4, std::vector<RoomId>{{10}, {20}})); // set4 should remain {10, 20}
    TEST_ASSERT(set3 != set4);

    // Test COW when original becomes single/empty after erase
    TinyRoomIdSet set5;
    set5.insert(RoomId{100});
    set5.insert(RoomId{200}); // set5 is {100, 200}
    TinyRoomIdSet set6 = set5;

    set5.erase(RoomId{100}); // set5 becomes {200} (single)
    TEST_ASSERT(checkContents(set5, std::vector<RoomId>{{200}}));
    TEST_ASSERT(checkContents(set6, std::vector<RoomId>{{100}, {200}}));
    TEST_ASSERT(set5 != set6);

    set5.erase(RoomId{200}); // set5 becomes {} (empty)
    TEST_ASSERT(set5.empty());
    TEST_ASSERT(checkContents(set6, std::vector<RoomId>{{100}, {200}}));
    TEST_ASSERT(set5 != set6);

    // Test behavior with initially empty or single sets (COW logic not dominant but correctness check)
    TinyRoomIdSet empty1;
    TinyRoomIdSet empty2 = empty1;
    empty1.insert(RoomId{1});
    TEST_ASSERT(checkContents(empty1, std::vector<RoomId>{{1}}));
    TEST_ASSERT(empty2.empty());

    TinyRoomIdSet single1(RoomId{7});
    TinyRoomIdSet single2 = single1;
    single1.insert(RoomId{8}); // single1 becomes big {7,8}
    TEST_ASSERT(checkContents(single1, std::vector<RoomId>{{7}, {8}}));
    TEST_ASSERT(checkContents(single2, std::vector<RoomId>{{7}})); // single2 should remain {7}
}

void testTinyRoomIdSet()
{
    // sizeof(TinyRoomIdSet) is now likely sizeof(std::shared_ptr<const RoomIdSetImplDetail>)
    // which is usually 2 * sizeof(void*). This is platform dependent.
    // The original check static_assert(sizeof(TinyRoomIdSet) == sizeof(uintptr_t)); is no longer valid.
    // We can assert it's larger than a single pointer if that's a useful check,
    // or simply rely on correct functioning. For now, let's remove the exact sizeof check.
    static_assert(sizeof(RoomIdSet) > sizeof(uintptr_t)); // Assuming RoomIdSet is substantially larger
    static_assert(sizeof(RoomIdSet) == sizeof(std::set<RoomId>));

    TestHelper<RoomIdSet>::test(); // Test original RoomIdSet behavior
    TestHelper<TinyRoomIdSet>::test(); // Test TinyRoomIdSet basic logical behavior

    testTinyRoomIdSet_CopyOnWrite(); // Test specific COW behaviors
}
} // namespace test
