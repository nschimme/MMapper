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

#include <deque>
#include <optional>

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
