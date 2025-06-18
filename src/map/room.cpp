// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "room.h"

#include "../global/StringView.h"
#include "Compare.h"
#include "RoomHandle.h"
#include "parseevent.h"
#include "../global/Diff.h" // For diff::Diff
#include "../global/logging.h" // For MMLOG (if enabled)

#include <sstream>
#include <vector>
#include <cmath>   // For std::lround, std::isfinite
#include <algorithm> // For std::max, std::min
#include <cassert> // For assert

namespace { // anonymous
static volatile bool spam_and_lag = false;

// Adapted from src/parser/mumexmlparser.cpp
// Renamed from compareDescription back to isMostlyTheSame
NODISCARD static bool isMostlyTheSame(const std::string_view a_sv,
                                      const std::string_view b_sv,
                                      const int cutoff_percent)
{
    // Ensure cutoff_percent is within a reasonable range (e.g., 50-100)
    // The original function asserted: isClamped(cutoff, std::nextafter(50.0, 51.0), std::nextafter(100.0, 99.0))
    // For integer cutoff_percent, let's use a simpler assert or clamp.
    assert(cutoff_percent >= 50 && cutoff_percent <= 100);

    if (a_sv.empty() || b_sv.empty()) {
        return false;
    }

    struct NODISCARD MyDiff final : public diff::Diff<StringView>
    {
    public:
        size_t removedBytes = 0;
        size_t addedBytes = 0;
        size_t commonBytes = 0;

    private:
        void virt_report(diff::SideEnum side, const Range &r) final
        {
            switch (side) {
            case diff::SideEnum::A:
                for (auto x : r) {
                    removedBytes += x.size();
                }
                break;
            case diff::SideEnum::B:
                for (auto x : r) {
                    addedBytes += x.size();
                }
                break;
            case diff::SideEnum::Common:
                for (auto x : r) {
                    commonBytes += x.size();
                }
                break;
            }
        }
    };

    const std::vector<StringView> aWords = StringView{a_sv}.getWords();
    const std::vector<StringView> bWords = StringView{b_sv}.getWords();

    MyDiff diff;
    diff.compare(MyDiff::Range::fromVector(aWords), MyDiff::Range::fromVector(bWords));

    const auto max_change = std::max(diff.removedBytes, diff.addedBytes);
    const auto min_change = std::min(diff.removedBytes, diff.addedBytes);
    const auto total_common_and_min_change = min_change + diff.commonBytes;

    // Original heuristic: if (total < 20 || max_change >= total)
    if (total_common_and_min_change < 20 || max_change >= total_common_and_min_change) {
        return false;
    }

    // Original calculation: const auto ratio = static_cast<double>(total - max_change) / static_cast<double>(total);
    const auto ratio = static_cast<double>(total_common_and_min_change - max_change) / static_cast<double>(total_common_and_min_change);
    const auto percent = ratio * 100.0;
    const auto rounded_percent = static_cast<double>(std::lround(percent * 10.0)) * 0.1;

    // MMLOG() << "[XML parser] Score: " << rounded << "% word match"; // Commented out for now
    return rounded_percent >= static_cast<double>(cutoff_percent);
}

}

RoomModificationTracker::~RoomModificationTracker() = default;

void RoomModificationTracker::notifyModified(const RoomUpdateFlags updateFlags)
{
    if (!updateFlags.empty()) {
        m_needsMapUpdate = true;
    }
    virt_onNotifyModified(updateFlags);
}

NODISCARD static int wordDifference(StringView a, StringView b)
{
    size_t diff = 0;
    while (!a.isEmpty() && !b.isEmpty()) {
        if (a.takeFirstLetter() != b.takeFirstLetter()) {
            ++diff;
        }
    }
    return static_cast<int>(diff + a.size() + b.size());
}

// Renamed from compareName back to compareStrings
ComparisonResultEnum compareStrings(const std::string_view room_name_sv,
                                    const std::string_view event_name_sv,
                                    int prevTolerance,
                                    const bool upToDate = true) // upToDate is not used by name comparison path in main compare
{
    // prevTolerance was originally used as a percentage for descriptions,
    // but for names, it's directly used in a character-difference way.
    // The main 'compare' function passes the 'tolerance' (percentage) argument here.
    // The original 'compareStrings' logic for names effectively ignored 'upToDate'.

    // Let's stick to the original logic of compareStrings for names,
    // where prevTolerance is a percentage that gets converted.
    int absoluteTolerance = utils::clampNonNegative(prevTolerance);
    absoluteTolerance *= static_cast<int>(room_name_sv.size());
    absoluteTolerance /= 100;
    int currentTolerance = absoluteTolerance;

    auto descWords = StringView{room_name_sv}.trim();
    auto eventWords = StringView{event_name_sv}.trim();
    if (!eventWords.isEmpty()) {
        // if event is empty we don't compare (due to blindness)
        while (currentTolerance >= 0) {
            if (descWords.isEmpty()) {
                // Name in room is shorter than event; remaining event chars count against tolerance
                currentTolerance -= eventWords.countNonSpaceChars();
                break;
            }
            if (eventWords.isEmpty()) {
                // Name in event is shorter than room; remaining room chars count against tolerance
                currentTolerance -= descWords.countNonSpaceChars();
                break;
            }
            currentTolerance -= wordDifference(eventWords.takeFirstWord(), descWords.takeFirstWord());
        }
    } else if (!descWords.isEmpty()) { // event name is empty, but room name is not
        currentTolerance -= descWords.countNonSpaceChars();
    }


    if (currentTolerance < 0) {
        return ComparisonResultEnum::DIFFERENT;
    } else if (absoluteTolerance != currentTolerance) { // Some difference was found but within tolerance
        return ComparisonResultEnum::TOLERANCE;
    } else if (event_name_sv.size() != room_name_sv.size()) { // Same content, different whitespace/casing handled by wordDifference
        return ComparisonResultEnum::TOLERANCE;
    }
    return ComparisonResultEnum::EQUAL;
}

ComparisonResultEnum compare(const RawRoom &room, const ParseEvent &event, const int tolerance)
{
    const auto &name = room.getName();
    const auto &desc = room.getDescription();
    const RoomTerrainEnum terrainType = room.getTerrainType();
    bool mapIdMatch = false;
    bool upToDate = true;

    //    if (event == nullptr) {
    //        return ComparisonResultEnum::EQUAL;
    //    }

    if (name.isEmpty() && desc.isEmpty() && terrainType == RoomTerrainEnum::UNDEFINED) {
        // user-created
        return ComparisonResultEnum::TOLERANCE;
    }

    if (event.getServerId() == INVALID_SERVER_ROOMID // fog/darkness results in no MapId
        || room.getServerId() == INVALID_SERVER_ROOMID) {
        mapIdMatch = false;
    } else if (event.getServerId() == room.getServerId()) {
        mapIdMatch = true;
    } else {
        return ComparisonResultEnum::DIFFERENT;
    }

    if (event.getTerrainType() != terrainType) {
        return mapIdMatch ? ComparisonResultEnum::TOLERANCE : ComparisonResultEnum::DIFFERENT;
    }

    switch (compareStrings(name.getStdStringViewUtf8(), // Call updated to compareStrings
                           event.getRoomName().getStdStringViewUtf8(),
                           tolerance /* For names, use existing logic */ )) {
    case ComparisonResultEnum::DIFFERENT:
        return mapIdMatch ? ComparisonResultEnum::TOLERANCE : ComparisonResultEnum::DIFFERENT;
    case ComparisonResultEnum::EQUAL:
        break;
    case ComparisonResultEnum::TOLERANCE:
        upToDate = false;
        break;
    }

    // Logic for description comparison
    bool descIsSimilar;
    // INVALID_SERVER_ROOMID is defined in global/Consts.h, checked earlier for mapIdMatch
    if (event.getServerId() != INVALID_SERVER_ROOMID) {
        // Valid server ID, use isMostlyTheSame
        descIsSimilar = isMostlyTheSame(desc.getStdStringViewUtf8(),
                                        event.getRoomDesc().getStdStringViewUtf8(),
                                        tolerance);
    } else {
        // Invalid server ID, use compareStrings
        // The upToDate parameter for compareStrings was true by default.
        // For descriptions, the original compareStrings call passed the current upToDate state.
        // We need to decide if that's still relevant or if the simpler boolean conversion is enough.
        // The main 'compare' function's 'upToDate' is what we are trying to determine.
        // Let's use the simpler conversion: true if not DIFFERENT.
        ComparisonResultEnum descCompResult = compareStrings(desc.getStdStringViewUtf8(),
                                                             event.getRoomDesc().getStdStringViewUtf8(),
                                                             tolerance,
                                                             upToDate); // Pass current upToDate, as original compareStrings did for desc
        descIsSimilar = (descCompResult != ComparisonResultEnum::DIFFERENT);
        // If compareStrings returned TOLERANCE, descIsSimilar is true,
        // and the original logic for TOLERANCE (setting upToDate = false)
        // needs to be preserved if descCompResult was TOLERANCE.
        if (descCompResult == ComparisonResultEnum::TOLERANCE) {
            // This explicitly handles the case where compareStrings itself determined TOLERANCE,
            // which means upToDate should be false.
            // The later common check `if (!descIsSimilar && mapIdMatch)` might not catch this nuance
            // if descIsSimilar is true for TOLERANCE.
            // However, the existing logic for descIsSimilar already handles this:
            // if mapIdMatch is true and desc is not similar -> upToDate = false.
            // If mapIdMatch is false and desc is not similar -> return DIFFERENT.
            // This structure should be okay.
            // If descCompResult is TOLERANCE, descIsSimilar is true.
            // The original compareStrings would set upToDate = false.
            // The current logic with isMostlyTheSame (if it returns false) sets upToDate = false if mapIdMatch.
            // This seems compatible.
            // Let's simplify: if compareStrings says TOLERANCE, it means it's "similar enough"
            // for descIsSimilar to be true, but it also implies !upToDate.
            // The existing structure is:
            // if (!descIsSimilar) { if (mapIdMatch) upToDate = false; else return DIFFERENT; }
            // This means if descIsSimilar is true (even from TOLERANCE), this block is skipped.
            // So, we need to ensure upToDate is correctly set if compareStrings returned TOLERANCE.
            if (descCompResult == ComparisonResultEnum::TOLERANCE) {
                 // This was the original behavior of compareStrings for descriptions when it returned TOLERANCE
                upToDate = false;
            }
        }
    }

    if (!descIsSimilar) {
        if (mapIdMatch) {
            // This aligns with "DIFFERENT" from original compareStrings when mapIdMatch is true -> TOLERANCE
            // So, we mark it as not upToDate, leading to an overall TOLERANCE if nothing else makes it DIFFERENT.
            upToDate = false;
        } else {
            // This aligns with "DIFFERENT" from original compareStrings when mapIdMatch is false -> DIFFERENT
            return ComparisonResultEnum::DIFFERENT;
        }
    }
    // If descIsSimilar is true, it's like the EQUAL case from original compareStrings, so upToDate remains unchanged by this step.
    // The redundant 'if (!isMostlyTheSameDesc(...))' block and the 'New logic for description:' comment block
    // that was above the 'bool descIsSimilar = compareDescription(...)' line have been removed by this change,
    // as this consolidated block replaces them.


    switch (compareWeakProps(room, event)) {
    case ComparisonResultEnum::DIFFERENT:
        return mapIdMatch ? ComparisonResultEnum::TOLERANCE : ComparisonResultEnum::DIFFERENT;
    case ComparisonResultEnum::EQUAL:
        break;
    case ComparisonResultEnum::TOLERANCE:
        upToDate = false;
        break;
    }

    if (upToDate && event.hasServerId() && !mapIdMatch) {
        // room is missing server id
        upToDate = false;
    }

    if (upToDate && room.getArea() != event.getRoomArea()) {
        // room is missing area
        upToDate = false;
    }

    if (upToDate) {
        return ComparisonResultEnum::EQUAL;
    }
    return ComparisonResultEnum::TOLERANCE;
}

ComparisonResultEnum compareWeakProps(const RawRoom &room, const ParseEvent &event)
{
    bool exitsValid = true;
    // REVISIT: Should tolerance be an integer given known 'weak' params like hidden
    // exits or undefined flags?
    bool tolerance = false;

    const ConnectedRoomFlagsType connectedRoomFlags = event.getConnectedRoomFlags();
    const PromptFlagsType pFlags = event.getPromptFlags();
    if (pFlags.isValid() && connectedRoomFlags.isValid() && connectedRoomFlags.isTrollMode()) {
        const RoomLightEnum lightType = room.getLightType();
        const RoomSundeathEnum sunType = room.getSundeathType();
        if (pFlags.isLit() && lightType != RoomLightEnum::LIT
            && sunType == RoomSundeathEnum::NO_SUNDEATH) {
            // Allow prompt sunlight to override rooms without LIT flag if we know the room
            // is troll safe and obviously not in permanent darkness
            qDebug() << "Updating room to be LIT";
            tolerance = true;

        } else if (pFlags.isDark() && lightType != RoomLightEnum::DARK
                   && sunType == RoomSundeathEnum::NO_SUNDEATH) {
            // Allow prompt sunlight to override rooms without DARK flag if we know the room
            // has at least one sunlit exit and the room is troll safe
            qDebug() << "Updating room to be DARK";
            tolerance = true;
        }
    }

    const ExitsFlagsType eventExitsFlags = event.getExitsFlags();
    if (eventExitsFlags.isValid()) {
        bool previousDifference = false;
        for (const ExitDirEnum dir : ALL_EXITS_NESWUD) {
            const auto &roomExit = room.getExit(dir);
            const ExitFlags roomExitFlags = roomExit.getExitFlags();
            if (roomExitFlags) {
                // exits are considered valid as soon as one exit is found (or if the room is updated)
                exitsValid = true;
                if (previousDifference) {
                    return ComparisonResultEnum::DIFFERENT;
                }
            }
            if (roomExitFlags.isNoMatch()) {
                continue;
            }
            const bool hasLight = connectedRoomFlags.isValid()
                                  && connectedRoomFlags.hasDirectSunlight(dir);
            const auto eventExitFlags = eventExitsFlags.get(dir);
            const auto diff = eventExitFlags ^ roomExitFlags;
            /* MUME has two logic flows for displaying signs on exits:
             *
             * 1) Display one sign for a portal {} or closed door []
             *    i.e. {North} [South]
             *
             * 2) Display two signs from each list in the following order:
             *    a) one option of: * ^ = - ~
             *    b) one option of: open door () or climb up /\ or climb down \/
             *    i.e. *(North)* -/South\- ~East~ *West*
             *
             * You can combine the two flows for each exit: {North} ~East~ *(West)*
            */
            if (diff.isExit() || diff.isDoor()) {
                if (!exitsValid) {
                    // Room was not isUpToDate and no exits were present in the room
                    previousDifference = true;
                } else {
                    if (tolerance) {
                        // Do not be tolerant for multiple differences
                        if (spam_and_lag) {
                            qDebug() << "Found too many differences" << event
                                     << mmqt::toQStringUtf8(room.toStdStringUtf8());
                        }
                        return ComparisonResultEnum::DIFFERENT;

                    } else if (!roomExitFlags.isExit() && eventExitFlags.isDoor()) {
                        // No exit exists on the map so we probably found a secret door
                        if (spam_and_lag) {
                            qDebug() << "Secret door likely found to the" << lowercaseDirection(dir)
                                     << event;
                        }
                        tolerance = true;

                    } else if (roomExit.doorIsHidden() && !eventExitFlags.isDoor()) {
                        if (spam_and_lag) {
                            qDebug() << "Secret exit hidden to the" << lowercaseDirection(dir);
                        }
                    } else if (roomExitFlags.isExit() && roomExitFlags.isDoor()
                               && !eventExitFlags.isExit()) {
                        if (spam_and_lag) {
                            qDebug()
                                << "Door to the" << lowercaseDirection(dir) << "is likely a secret";
                        }
                        tolerance = true;

                    } else {
                        if (spam_and_lag) {
                            qWarning() << "Unknown exit/door tolerance condition to the"
                                       << lowercaseDirection(dir) << event
                                       << mmqt::toQStringUtf8(room.toStdStringUtf8());
                        }
                        return ComparisonResultEnum::DIFFERENT;
                    }
                }
            } else if (diff.isRoad()) {
                if (roomExitFlags.isRoad() && hasLight) {
                    // Orcs/trolls can only see trails/roads if it is dark (but can see climbs)
                    if (spam_and_lag) {
                        qDebug() << "Orc/troll could not see trail to the"
                                 << lowercaseDirection(dir);
                    }

                } else if (roomExitFlags.isRoad() && !eventExitFlags.isRoad()
                           && roomExitFlags.isDoor() && eventExitFlags.isDoor()) {
                    // A closed door is hiding the road that we know is there
                    if (spam_and_lag) {
                        qDebug() << "Closed door masking road/trail to the"
                                 << lowercaseDirection(dir);
                    }

                } else if (!roomExitFlags.isRoad() && eventExitFlags.isRoad()
                           && roomExitFlags.isDoor() && eventExitFlags.isDoor()) {
                    // A known door was previously mapped closed and a new road exit flag was found
                    if (spam_and_lag) {
                        qDebug() << "Previously closed door was hiding road to the"
                                 << lowercaseDirection(dir);
                    }
                    tolerance = true;

                } else {
                    if (spam_and_lag) {
                        qWarning()
                            << "Unknown road tolerance condition to the" << lowercaseDirection(dir)
                            << event << mmqt::toQStringUtf8(room.toStdStringUtf8());
                    }
                    // TODO: Likely an old road/trail that needs to be removed
                    tolerance = true;
                }
            } else if (diff.isClimb()) {
                if (roomExitFlags.isDoor() && roomExitFlags.isClimb()) {
                    // A closed door is hiding the climb that we know is there
                    if (spam_and_lag) {
                        qDebug() << "Door masking climb to the" << lowercaseDirection(dir);
                    }

                } else {
                    if (spam_and_lag) {
                        qWarning()
                            << "Unknown climb tolerance condition to the" << lowercaseDirection(dir)
                            << event << mmqt::toQStringUtf8(room.toStdStringUtf8());
                    }
                    // TODO: Likely an old climb that needs to be removed
                    tolerance = true;
                }
            }
        }
    }
    if (tolerance || !exitsValid) {
        return ComparisonResultEnum::TOLERANCE;
    }
    return ComparisonResultEnum::EQUAL;
}
