// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "ParseTree.h"

#include "../configuration/configuration.h"
#include "../global/AnsiOstream.h"
#include "../global/LineUtils.h"
#include "../global/PrintUtils.h"
#include "../global/logging.h"
#include "Compare.h"
#include "Map.h"
#include "World.h"
#include "WorldAreaMap.h" // Required for LocalAreaInfo

#include <deque>
#include <optional>

#include <variant> // Required for std::variant and std::visit

RoomIdSet getRooms(const Map &map, const ParseTree &tree, const ParseEvent &event)
{
    // REVISIT: use Timer here instead of manually doing the same thing with Clock::now(),
    // which would have the added benefit of reporting times for lookups that fail.
    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();
    static volatile bool fallbackToCurrentArea = true;
    static volatile bool fallbackToRemainder = true;
    static volatile bool fallbackToWholeMap = true;

    // Define a variant type for the set pointers, reinstating it
    using SetPtrVariant = std::variant<const ImmRoomIdSet*, const ImmUnorderedRoomIdSet*, std::nullptr_t>;

    const SetPtrVariant pSetVariant = [&map, &event, &tree]() -> SetPtrVariant { // Returns SetPtrVariant
        const World &world = map.getWorld();
        const RoomArea &areaName = event.getRoomArea();

        const RoomName &name = event.getRoomName();
        const RoomDesc &desc = event.getRoomDesc();

        const bool hasName = !name.empty();
        const bool hasDesc = !desc.empty();

        if (hasName && hasDesc) {
            // tree.name_desc now holds ImmUnorderedRoomIdSet
            if (auto set = tree.name_desc.find(NameDesc{name, desc})) {
                return set; // Returns const ImmUnorderedRoomIdSet*
            }
            MMLOG() << "[getRooms] Failed to find a match with name+desc. Falling back to name or desc...";
        }

        if (hasName) {
            // tree.name_only now holds ImmUnorderedRoomIdSet
            if (auto ptr = tree.name_only.find(name)) {
                return ptr; // Returns const ImmUnorderedRoomIdSet*
            }
        }

        if (hasDesc) {
            // tree.desc_only now holds ImmUnorderedRoomIdSet
            if (auto ptr = tree.desc_only.find(desc)) {
                return ptr; // Returns const ImmUnorderedRoomIdSet*
            }
        }

        if (fallbackToCurrentArea) {
            MMLOG() << "[getRooms] Falling back to the current area!";
            MMLOG() << "[getRooms] event: " << mmqt::toStdStringUtf8(event.toQString());
            try {
                const LocalAreaInfo& localInfo = world.getAreaInfoMap().findLocalArea(areaName);
                if (!localInfo.roomSet.empty()) { // Check for non-empty
                     return &localInfo.roomSet; // Return const ImmUnorderedRoomIdSet*
                } else {
                    MMLOG() << "[getRooms] Area was empty.";
                }
            } catch (const std::runtime_error&) {
                MMLOG() << "[getRooms] Area does not exist.";
            }
            // Fall through if area not found or empty
        }

        if (fallbackToRemainder && !areaName.empty()) {
            MMLOG() << "[getRooms] Falling back to the remainder area...";
            try {
                const LocalAreaInfo& localInfo = world.getAreaInfoMap().findLocalArea(RoomArea{});
                if (!localInfo.roomSet.empty()) { // Check for non-empty
                    return &localInfo.roomSet; // Return const ImmUnorderedRoomIdSet*
                } else {
                    MMLOG() << "[getRooms] Fallback area was empty.";
                }
            } catch (const std::runtime_error&) {
                MMLOG() << "[getRooms] Fallback area does not exist.";
            }
            // Fall through if area not found or empty
            }
        }

        if (fallbackToWholeMap) {
            MMLOG() << "[getRooms] Falling back to the whole map...";
            return &world.getRoomSet(); // Returns const ImmRoomIdSet*
        }

        MMLOG() << "[getRooms] Failed to find a match; giving up.";
        MMLOG() << "[getRooms] event: " << mmqt::toStdStringUtf8(event.toQString());
        return nullptr; // Return nullptr for SetPtrVariant
    }();

    const auto t1 = Clock::now();
    RoomIdSet results;
    Clock::time_point t2_for_timing = t1; // Initialize t2 for timing log
    Clock::time_point t3_for_timing = t1; // Initialize t3 for timing log

    // Reinstate std::visit
    std::visit(
        [&](auto&& arg_pSet_variant_val) {
            // Common processing logic for pointer types
            auto processSet = [&](const auto* set_ptr) {
                if (!set_ptr) {
                    MMLOG() << "[getRooms] Unable to find any matches (variant holds a null pointer).";
                    t2_for_timing = Clock::now();
                    t3_for_timing = t2_for_timing;
                    return;
                }

                const auto& set = *set_ptr; // Dereference the pointer to get the actual set
                const int tolerance = getConfig().pathMachine.matchingTolerance;
                auto tryReport = [&event, tolerance](const RoomHandle &room) {
                    if (::compare(room.getRaw(), event, tolerance) == ComparisonResultEnum::DIFFERENT) {
                        return false;
                    }
                    return true;
                };

                MMLOG() << "[getRooms] Found " << set.size() << " potential match(es).";
                t2_for_timing = Clock::now();
                size_t numReported = 0;
                set.for_each([&](const RoomId id) { // Use for_each from the actual set object
                    if (const auto optRoom = map.findRoomHandle(id)) {
                        if (tryReport(optRoom)) {
                            results.insert(id);
                            ++numReported;
                        }
                    }
                });
                t3_for_timing = Clock::now();
                MMLOG() << "[getRooms] Reported " << numReported << " potential match(es).";
            };

            using ArgType = std::decay_t<decltype(arg_pSet_variant_val)>;

            if constexpr (std::is_same_v<ArgType, const ImmRoomIdSet*>) {
                processSet(arg_pSet_variant_val);
            } else if constexpr (std::is_same_v<ArgType, const ImmUnorderedRoomIdSet*>) {
                processSet(arg_pSet_variant_val);
            } else if constexpr (std::is_same_v<ArgType, std::nullptr_t>) {
                MMLOG() << "[getRooms] Unable to find any matches (variant holds std::nullptr_t).";
                t2_for_timing = Clock::now();
                t3_for_timing = t2_for_timing;
            }
        },
        pSetVariant // Use the original pSetVariant name
    );

    const auto t_end_visit = Clock::now();

    auto &&os_log = MMLOG();
    os_log << "[getRooms] timing follows:\n";

    auto report_timing = [&os_log](std::string_view what, Clock::time_point a, Clock::time_point b) {
        auto ms = static_cast<double>(static_cast<std::chrono::nanoseconds>(b - a).count()) * 1e-6;
        os_log << "[TIMER] [getRooms] " << what << ": " << ms << " ms\n";
    };

    report_timing("part0. lookup rooms (lambda)", t0, t1);
    report_timing("part1. (visitor setup, etc.)", t1, t2_for_timing);
    report_timing("part2. for(...) tryReport() ", t2_for_timing, t3_for_timing);
    report_timing("overall", t0, t_end_visit);

    return results;
}

void ParseTree::printStats(ProgressCounter & /*pc*/, AnsiOstream &os) const
{
    static constexpr RawAnsi green = getRawAnsi(AnsiColor16Enum::green);
    static constexpr RawAnsi yellow = getRawAnsi(AnsiColor16Enum::yellow);

    auto C = [](auto x) {
        static_assert(std::is_integral_v<decltype(x)>);
        return ColoredValue{green, x};
    };

    {
        const size_t total_name = name_only.size();
        const size_t total_desc = desc_only.size();
        const size_t total_name_desc = name_desc.size();

        os << "\n";
        os << "Total name combinations:              " << C(total_name) << ".\n";
        os << "Total desc combinations:              " << C(total_desc) << ".\n";
        os << "Total name+desc combinations:         " << C(total_name_desc) << ".\n";

        const auto countUnique = [](const auto &map) -> size_t {
            return static_cast<size_t>(std::count_if(std::begin(map), std::end(map), [](auto &kv) {
                return kv.second.size() == 1;
            }));
        };

        const size_t unique_name = countUnique(name_only);
        const size_t unique_desc = countUnique(desc_only);
        const size_t unique_name_Desc = countUnique(name_desc);

        os << "\n";
        os << "  unique name:              " << C(unique_name) << ".\n";
        os << "  unique desc:              " << C(unique_desc) << ".\n";
        os << "  unique name+desc:         " << C(unique_name_Desc) << ".\n";

        os << "\n";
        os << "  non-unique names:             " << C(total_name - unique_name) << ".\n";
        os << "  non-unique descs:             " << C(total_desc - unique_desc) << ".\n";
        os << "  non-unique name+desc:         " << C(total_name_desc - unique_name_Desc) << ".\n";
    }

    {
        auto count_nonunique = [](const auto &map) -> size_t {
            size_t nonunique = 0;
            for (const auto &kv : map) {
                const auto n = kv.second.size();
                if (n == 1) {
                    continue;
                }
                nonunique += n;
            }
            return nonunique;
        };

        const size_t nonunique_name = count_nonunique(name_only);
        const size_t nonunique_desc = count_nonunique(desc_only);
        const size_t nonunique_name_desc = count_nonunique(name_desc);

        os << "\n";
        os << "  rooms w/ non-unique names:             " << C(nonunique_name) << ".\n";
        os << "  rooms w/ non-unique descs:             " << C(nonunique_desc) << ".\n";
        os << "  rooms w/ non-unique name+desc:         " << C(nonunique_name_desc) << ".\n";
    }

    struct NODISCARD DisplayHelper final
    {
        static void print(AnsiOstream &os, const RoomName &name)
        {
            os.writeQuotedWithColor(green, yellow, name.getStdStringViewUtf8());
            os << "\n";
        }
        static void print(AnsiOstream &os, const RoomDesc &desc)
        {
            foreachLine(desc.getStdStringViewUtf8(), [&os](std::string_view line) {
                os.writeQuotedWithColor(green, yellow, line);
                os << "\n";
            });
        }

        static void print(AnsiOstream &os, const NameDesc &v2key)
        {
            os << "Name:\n";
            print(os, v2key.name);
            os << "Desc:\n";
            print(os, v2key.desc);
        }
    };

    auto printMostCommon = [&os](const auto &map, std::string_view thing) {
        using Map = decltype(map);
        using Pair = decltype(*std::declval<Map>().begin());
        using Key = std::remove_const_t<decltype(std::declval<Pair>().first)>;
        // using Value = decltype(std::declval<Pair>().second);

        const Key defaultKey{};
        std::optional<Key> mostCommon;
        size_t mostCommonCount = 0;

        for (const auto &kv : map) {
            if (kv.first == defaultKey) {
                continue;
            }

            const auto count = kv.second.size();
            if (!mostCommon || count > mostCommonCount) {
                mostCommon = kv.first;
                mostCommonCount = count;
            }
        }

        if (mostCommon && mostCommonCount > 1) {
            os << "\n";
            // Note: This excludes default/empty strings.
            os << "Most common " << thing << " appears " << ColoredValue{green, mostCommonCount}
               << " time" << ((mostCommonCount == 1) ? "" : "s") << ":\n";

            DisplayHelper::print(os, *mostCommon);
        }
    };
    printMostCommon(name_only, "name");
    printMostCommon(desc_only, "desc");
    printMostCommon(name_desc, "name+desc");
}
