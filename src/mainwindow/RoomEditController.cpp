// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "RoomEditController.h"

#include "../configuration/configuration.h"
#include "../display/Filenames.h"
#include "../display/RoadIndex.h"
#include "../global/AnsiHtml.h"
#include "../global/AnsiOstream.h"
#include "../global/TextUtils.h"
#include "../map/Changes.h"
#include "../map/Diff.h"
#include "../map/Map.h"
#include "../map/RawExit.h"
#include "../map/RoomFieldVariant.h"
#include "../map/RoomRevert.h"
#include "../map/enums.h"
#include "../mapdata/mapdata.h"
#include "CheckableFlagModel.h"

#include <algorithm>
#include <functional>
#include <sstream>
#include <utility>

namespace { // anonymous

// Ported from roomeditattrdlg.cpp's anonymous-namespace getPriority()
// overloads: a handful of flags are pinned to the front of the sorted
// mob/load flag lists (mobFlagsListWidget/loadFlagsListWidget both have
// sortingEnabled=true in the .ui), and the rest keep their enum order.
NODISCARD int getPriority(const RoomMobFlagEnum flag)
{
#define X_POS(UPPER, pos) \
    do { \
        if (flag == RoomMobFlagEnum::UPPER) { \
            return (pos) - (NUM_ROOM_MOB_FLAGS); \
        } \
    } while (false)
    X_POS(PASSIVE_MOB, 0);
    X_POS(AGGRESSIVE_MOB, 1);
    X_POS(ELITE_MOB, 2);
    X_POS(SUPER_MOB, 3);
    X_POS(RATTLESNAKE, 4);
    X_POS(QUEST_MOB, 5);
#undef X_POS
    return static_cast<int>(flag);
}

NODISCARD int getPriority(const RoomLoadFlagEnum flag)
{
#define X_POS(UPPER, pos) \
    do { \
        if (flag == RoomLoadFlagEnum::UPPER) { \
            return (pos) - (NUM_ROOM_LOAD_FLAGS); \
        } \
    } while (false)
    X_POS(TREASURE, 0);
    X_POS(ARMOUR, 1);
    X_POS(WEAPON, 2);
    X_POS(EQUIPMENT, 3);
#undef X_POS
    return static_cast<int>(flag);
}

// Ported from roomeditattrdlg.cpp's add_boxed_string(): the multi-line note
// string is normalized to have a trailing newline (if it has any text at
// all), which we need to strip since we're returning a plain QString rather
// than appending to a QTextEdit (whose append() supplies its own newlines).
NODISCARD QString boxedNoteToQString(const RoomNote &note)
{
    std::string_view sv = note.getStdStringViewUtf8();
    trim_newline_inplace(sv);
    return mmqt::toQStringUtf8(sv);
}

NODISCARD mmqt::AnsiHtmlDefaults getAnsiHtmlDefaults()
{
    const auto &settings = getConfig().integratedClient;
    return mmqt::AnsiHtmlDefaults{settings.backgroundColor, settings.foregroundColor, std::nullopt};
}

NODISCARD QString ansiTextToHtml(const std::string &text)
{
    const auto defaults = getAnsiHtmlDefaults();
    return mmqt::ansiToHtml(mmqt::toQStringUtf8(text), defaults.defaultFg, defaults.defaultBg);
}

NODISCARD QUrl terrainIcon(const RoomTerrainEnum type)
{
    // Ported from roomeditattrdlg.cpp's get_terrain_pixmap() lambda: there's
    // no "current room's actual road connections" concept for a standalone
    // preview icon, so ROAD always renders the same 3-way N/E/S junction.
    if (type == RoomTerrainEnum::ROAD) {
        return CheckableFlagModel::iconUrl(getPixmapFilename(TaggedRoad{
            RoadIndexMaskEnum::NORTH | RoadIndexMaskEnum::EAST | RoadIndexMaskEnum::SOUTH}));
    }
    return CheckableFlagModel::iconUrl(getPixmapFilename(type));
}

} // namespace

RoomEditController::RoomEditController(QObject *const parent)
    : QObject(parent)
    , m_mobFlagsModel(new CheckableFlagModel(this))
    , m_loadFlagsModel(new CheckableFlagModel(this))
    , m_exitFlagsModel(new CheckableFlagModel(this))
    , m_doorFlagsModel(new CheckableFlagModel(this))
{
    {
        std::vector<RoomMobFlagEnum> flags(std::begin(ALL_MOB_FLAGS), std::end(ALL_MOB_FLAGS));
        std::stable_sort(flags.begin(), flags.end(), [](auto a, auto b) {
            return getPriority(a) < getPriority(b);
        });
        std::vector<CheckableFlagModel::Row> rows;
        rows.reserve(flags.size());
        for (const RoomMobFlagEnum flag : flags) {
            CheckableFlagModel::Row row;
            row.flagValue = static_cast<int>(flag);
            row.name = getName(flag);
            row.iconSource = CheckableFlagModel::iconUrl(getPixmapFilename(flag));
            row.state = Qt::Unchecked;
            row.checkable = true;
            rows.push_back(std::move(row));
        }
        m_mobFlagsModel->setRows(std::move(rows));
    }

    {
        std::vector<RoomLoadFlagEnum> flags(std::begin(ALL_LOAD_FLAGS), std::end(ALL_LOAD_FLAGS));
        std::stable_sort(flags.begin(), flags.end(), [](auto a, auto b) {
            return getPriority(a) < getPriority(b);
        });
        std::vector<CheckableFlagModel::Row> rows;
        rows.reserve(flags.size());
        for (const RoomLoadFlagEnum flag : flags) {
            CheckableFlagModel::Row row;
            row.flagValue = static_cast<int>(flag);
            row.name = getName(flag);
            row.iconSource = CheckableFlagModel::iconUrl(getPixmapFilename(flag));
            row.state = Qt::Unchecked;
            row.checkable = true;
            rows.push_back(std::move(row));
        }
        m_loadFlagsModel->setRows(std::move(rows));
    }

    {
        std::vector<CheckableFlagModel::Row> rows;
        for (const ExitFlagEnum flag : ALL_EXIT_FLAGS) {
            CheckableFlagModel::Row row;
            row.flagValue = static_cast<int>(flag);
            row.name = mmqt::toQStringUtf8(getName(flag));
            row.state = Qt::Unchecked;
            // UNMAPPED is never checkable (matches the widget: it strips
            // Qt::ItemIsUserCheckable|Qt::ItemIsEnabled right after
            // installWidgets() and never restores them).
            row.checkable = flag != ExitFlagEnum::UNMAPPED;
            rows.push_back(std::move(row));
        }
        m_exitFlagsModel->setRows(std::move(rows));
    }

    {
        std::vector<CheckableFlagModel::Row> rows;
        for (const DoorFlagEnum flag : ALL_DOOR_FLAGS) {
            CheckableFlagModel::Row row;
            row.flagValue = static_cast<int>(flag);
            row.name = mmqt::toQStringUtf8(getName(flag));
            row.state = Qt::Unchecked;
            row.checkable = true;
            rows.push_back(std::move(row));
        }
        m_doorFlagsModel->setRows(std::move(rows));
    }

    m_terrainPreviewIcon = terrainIcon(RoomTerrainEnum::UNDEFINED);
}

RoomHandle RoomEditController::getSelectedRoom()
{
    if (m_roomSelection == nullptr || m_mapData == nullptr) {
        return RoomHandle{};
    }

    const auto &rs = *m_roomSelection;
    if (rs.empty()) {
        return RoomHandle{};
    }
    if (rs.size() == 1) {
        return m_mapData->getCurrentMap().findRoomHandle(rs.getFirstRoomId());
    }

    if (m_currentRoomIndex < 0 || static_cast<size_t>(m_currentRoomIndex) >= m_roomIdData.size()) {
        return RoomHandle{};
    }
    // REVISIT: Does the zero here mean that RoomId{0} won't work properly?
    // Should we change this to INVALID_ROOMID.value()? (ported verbatim from
    // roomeditattrdlg.cpp's getSelectedRoom())
    if (const auto target = RoomId{m_roomIdData[static_cast<size_t>(m_currentRoomIndex)]};
        rs.contains(target)) {
        return m_mapData->getCurrentMap().findRoomHandle(target);
    }
    return RoomHandle{};
}

void RoomEditController::setRoomSelection(const SharedRoomSelection &rs, MapData *const md)
{
    m_roomSelection = rs;
    m_mapData = md;

    m_roomNames.clear();
    m_roomIdData.clear();
    m_currentRoomIndex = 0;

    if (rs != nullptr && md != nullptr) {
        auto &sel = *rs;
        sel.removeMissing(*md);

        const auto &map = md->getCurrentMap();
        auto addRoom = [this, &map](const RoomId id) {
            const RoomHandle room = map.getRoomHandle(id);
            m_roomNames << QString("Room %1: %2")
                               .arg(room.getIdExternal().asUint32())
                               .arg(room.getName().toQString());
            m_roomIdData.push_back(room.getId().asUint32());
        };

        if (sel.size() == 1) {
            addRoom(sel.getFirstRoomId());
        } else if (!sel.empty()) {
            m_roomNames << tr("All");
            m_roomIdData.push_back(0);
            for (const RoomId id : sel) {
                addRoom(id);
            }
        }
    }

    emit sig_roomNamesChanged();
    emit sig_currentRoomIndexChanged();
    refresh();
}

void RoomEditController::setCurrentRoomIndex(const int index)
{
    if (m_currentRoomIndex == index) {
        return;
    }
    m_currentRoomIndex = index;
    emit sig_currentRoomIndexChanged();
    refresh();
}

void RoomEditController::setSelectedExitDir(const int dir)
{
    if (m_selectedExitDir == dir) {
        return;
    }
    m_selectedExitDir = dir;
    emit sig_selectedExitDirChanged();
    refresh();
}

void RoomEditController::updateCommon(const std::function<Change(const RawRoom &)> &getChange,
                                      const bool onlyExecuteAction)
{
    if (m_mapData == nullptr) {
        return;
    }

    if (const auto &r = getSelectedRoom()) {
        m_mapData->applySingleChange(getChange(r.getRaw()));
    } else if (m_roomSelection != nullptr) {
        m_mapData->applyChangesToList(*m_roomSelection, getChange);
    } else {
        return;
    }

    if (!onlyExecuteAction) {
        refresh();
        emit sig_requestUpdate();
    }
}

void RoomEditController::setFieldCommon(const RoomFieldVariant &var,
                                        const FlagModifyModeEnum mode,
                                        const bool onlyExecuteAction)
{
    updateCommon(
        [&var, mode](const RawRoom &room) -> Change {
            return Change{room_change_types::ModifyRoomFlags{room.getId(), var, mode}};
        },
        onlyExecuteAction);
}

void RoomEditController::setSelectedRoomExitField(const ExitFieldVariant &var,
                                                  const ExitDirEnum dir,
                                                  const FlagModifyModeEnum mode)
{
    const RoomHandle r = getSelectedRoom();
    if (!r || m_mapData == nullptr) {
        return;
    }
    m_mapData->applySingleChange(
        Change{exit_change_types::ModifyExitFlags{r.getId(), dir, var, mode}});
}

void RoomEditController::setAlign(const int value)
{
    setFieldCommon(RoomFieldVariant{static_cast<RoomAlignEnum>(value)}, FlagModifyModeEnum::ASSIGN);
}

void RoomEditController::setPortable(const int value)
{
    setFieldCommon(RoomFieldVariant{static_cast<RoomPortableEnum>(value)},
                   FlagModifyModeEnum::ASSIGN);
}

void RoomEditController::setRideable(const int value)
{
    setFieldCommon(RoomFieldVariant{static_cast<RoomRidableEnum>(value)},
                   FlagModifyModeEnum::ASSIGN);
}

void RoomEditController::setLight(const int value)
{
    setFieldCommon(RoomFieldVariant{static_cast<RoomLightEnum>(value)}, FlagModifyModeEnum::ASSIGN);
}

void RoomEditController::setSundeath(const int value)
{
    setFieldCommon(RoomFieldVariant{static_cast<RoomSundeathEnum>(value)},
                   FlagModifyModeEnum::ASSIGN);
}

void RoomEditController::setTerrain(const int value)
{
    setFieldCommon(RoomFieldVariant{static_cast<RoomTerrainEnum>(value)},
                   FlagModifyModeEnum::ASSIGN);
}

void RoomEditController::setDoorName(const QString &name)
{
    setSelectedRoomExitField(ExitFieldVariant{mmqt::makeDoorName(name)},
                             static_cast<ExitDirEnum>(m_selectedExitDir),
                             FlagModifyModeEnum::ASSIGN);
    refresh();
    emit sig_requestUpdate();
}

namespace { // anonymous

NODISCARD FlagModifyModeEnum modeForToggle(const Qt::CheckState current)
{
    // Ported from the widget's mobFlagsListItemChanged()/loadFlagsListItemChanged()/
    // exitFlagsListItemChanged()/doorFlagsListItemChanged(): a row currently
    // Checked toggles to Unchecked (REMOVE); Unchecked or PartiallyChecked
    // toggles to Checked (INSERT).
    return current == Qt::Checked ? FlagModifyModeEnum::REMOVE : FlagModifyModeEnum::INSERT;
}

} // namespace

void RoomEditController::toggleMobFlag(const int row)
{
    const CheckableFlagModel::Row *const r = m_mobFlagsModel->rowAt(row);
    if (r == nullptr) {
        return;
    }
    const RoomMobFlags flags{static_cast<RoomMobFlagEnum>(r->flagValue)};
    setFieldCommon(RoomFieldVariant{flags}, modeForToggle(r->state));
}

void RoomEditController::toggleLoadFlag(const int row)
{
    const CheckableFlagModel::Row *const r = m_loadFlagsModel->rowAt(row);
    if (r == nullptr) {
        return;
    }
    const RoomLoadFlags flags{static_cast<RoomLoadFlagEnum>(r->flagValue)};
    setFieldCommon(RoomFieldVariant{flags}, modeForToggle(r->state));
}

void RoomEditController::toggleExitFlag(const int row)
{
    const CheckableFlagModel::Row *const r = m_exitFlagsModel->rowAt(row);
    if (r == nullptr) {
        return;
    }
    const ExitFlags flags{static_cast<ExitFlagEnum>(r->flagValue)};
    setSelectedRoomExitField(ExitFieldVariant{flags},
                             static_cast<ExitDirEnum>(m_selectedExitDir),
                             modeForToggle(r->state));
    refresh();
    emit sig_requestUpdate();
}

void RoomEditController::toggleDoorFlagByEnum(const DoorFlagEnum flag)
{
    const int flagValue = static_cast<int>(flag);
    Qt::CheckState state = Qt::Unchecked;
    bool found = false;
    for (int i = 0, n = m_doorFlagsModel->rowCount(); i < n; ++i) {
        if (const auto *const row = m_doorFlagsModel->rowAt(i);
            row != nullptr && row->flagValue == flagValue) {
            state = row->state;
            found = true;
            break;
        }
    }
    if (!found) {
        return;
    }
    const DoorFlags flags{flag};
    setSelectedRoomExitField(ExitFieldVariant{flags},
                             static_cast<ExitDirEnum>(m_selectedExitDir),
                             modeForToggle(state));
    refresh();
    emit sig_requestUpdate();
}

void RoomEditController::toggleDoorFlag(const int row)
{
    const CheckableFlagModel::Row *const r = m_doorFlagsModel->rowAt(row);
    if (r == nullptr) {
        return;
    }
    toggleDoorFlagByEnum(static_cast<DoorFlagEnum>(r->flagValue));
}

void RoomEditController::toggleHiddenDoor()
{
    // REVISIT: Remove this feature? (ported verbatim from
    // roomeditattrdlg.cpp's toggleHiddenDoor())
    if (!m_doorFieldsEnabled) {
        return;
    }
    toggleDoorFlagByEnum(DoorFlagEnum::HIDDEN);
}

void RoomEditController::setNoteText(const QString &text)
{
    m_noteText = text;
    emit sig_noteTextChanged();
    m_noteDirty = true;
    emit sig_noteDirtyChanged();
}

void RoomEditController::applyNote()
{
    const auto note = mmqt::makeRoomNote(m_noteText);
    // onlyExecuteAction: matches the widget's roomNoteChanged(), which
    // deliberately does not trigger a canvas repaint.
    setFieldCommon(RoomFieldVariant{note}, FlagModifyModeEnum::ASSIGN, true);
    refresh();
}

void RoomEditController::revertNote()
{
    const RoomHandle r = getSelectedRoom();
    m_noteText = r ? boxedNoteToQString(r.getNote()) : QString();
    emit sig_noteTextChanged();
    m_noteDirty = false;
    emit sig_noteDirtyChanged();
}

void RoomEditController::clearNote()
{
    m_noteText.clear();
    emit sig_noteTextChanged();

    bool dirty = false;
    if (const RoomHandle r = getSelectedRoom()) {
        dirty = !r.getNote().empty();
    }
    m_noteDirty = dirty;
    emit sig_noteDirtyChanged();
}

void RoomEditController::revertDiff()
{
    if (m_mapData == nullptr) {
        return;
    }
    const RoomHandle r = getSelectedRoom();
    if (!r) {
        return;
    }

    MapData &md = *m_mapData;
    const auto savedMap = md.getSavedMap();
    const auto currentMap = md.getCurrentMap();
    const RoomId id = r.getId();

    std::ostringstream oss;
    AnsiOstream aos{oss};
    auto pResult = room_revert::build_plan(aos, currentMap, id, savedMap);
    m_diffHtml = ansiTextToHtml(oss.str());
    emit sig_diffHtmlChanged();

    if (!pResult) {
        emit sig_error(tr("Failed to build revert plan"));
        return;
    }

    const room_revert::RevertPlan &plan = *pResult;
    const ChangeList &changes = plan.changes;
    if (changes.empty() || !md.applyChanges(changes)) {
        emit sig_error(tr("Failed to apply revert changes"));
        return;
    }

    // NOTE: the widget's onRevertDiffClicked() does not emit a canvas
    // repaint request either; ported as-is.
    refresh();
}

void RoomEditController::refresh()
{
    const RoomHandle r = getSelectedRoom();
    m_hasSelectedRoom = static_cast<bool>(r);
    emit sig_hasSelectedRoomChanged();

    if (!r) {
        m_descriptionHtml.clear();
        emit sig_descriptionHtmlChanged();

        m_noteText.clear();
        emit sig_noteTextChanged();
        m_noteDirty = false;
        emit sig_noteDirtyChanged();

        m_statsHtml.clear();
        emit sig_statsHtmlChanged();

        m_diffHtml.clear();
        emit sig_diffHtmlChanged();
        m_revertEnabled = false;
        emit sig_revertEnabledChanged();

        m_terrainType = -1;
        emit sig_terrainTypeChanged();
        m_terrainPreviewIcon = terrainIcon(RoomTerrainEnum::UNDEFINED);
        emit sig_terrainPreviewIconChanged();

        m_align = -1;
        emit sig_alignChanged();
        m_portable = -1;
        emit sig_portableChanged();
        m_rideable = -1;
        emit sig_rideableChanged();
        m_light = -1;
        emit sig_lightChanged();
        m_sundeath = -1;
        emit sig_sundeathChanged();

        m_doorName.clear();
        emit sig_doorNameChanged();
        m_doorFieldsEnabled = false;
        emit sig_doorFieldsEnabledChanged();

        // "All" mode display parity: every mob/load row is PartiallyChecked
        // with NO union computation. exitFlagsModel/doorFlagsModel are left
        // untouched (stale) -- exitsFrame is disabled either way, matching
        // the widget, which doesn't touch them in this branch.
        m_mobFlagsModel->setAllStates(Qt::PartiallyChecked);
        m_loadFlagsModel->setAllStates(Qt::PartiallyChecked);
    } else {
        m_descriptionHtml = ansiTextToHtml(previewRoom(r));
        emit sig_descriptionHtmlChanged();

        m_noteText = boxedNoteToQString(r.getNote());
        emit sig_noteTextChanged();
        m_noteDirty = false;
        emit sig_noteDirtyChanged();

        m_statsHtml = ansiTextToHtml(std::invoke([&r]() -> std::string {
            try {
                std::ostringstream os;
                AnsiOstream aos{os};
                r.getMap().statRoom(aos, r.getId());
                return std::move(os).str();
            } catch (const std::exception &ex) {
                return std::string("Exception: ") + ex.what();
            }
        }));
        emit sig_statsHtmlChanged();

        {
            std::string diffText;
            bool revertible = false;
            if (m_mapData != nullptr) {
                const auto saved = m_mapData->getSavedMap();
                const auto current = m_mapData->getCurrentMap();
                const ExternalRoomId ext = r.getIdExternal();
                if (const auto pOld = saved.findRoomHandle(ext); pOld) {
                    if (const auto pNew = current.findRoomHandle(ext); pNew) {
                        try {
                            std::ostringstream os;
                            AnsiOstream aos{os};
                            OstreamDiffReporter odr{aos};
                            compare(odr, pOld, pNew);
                            diffText = std::move(os).str();
                            revertible = !diffText.empty();
                        } catch (const std::exception &) {
                            diffText.clear();
                        }
                    }
                }
            }
            m_diffHtml = diffText.empty() ? QString() : ansiTextToHtml(diffText);
            emit sig_diffHtmlChanged();
            m_revertEnabled = revertible;
            emit sig_revertEnabledChanged();
        }

        m_terrainType = static_cast<int>(r.getTerrainType());
        emit sig_terrainTypeChanged();
        m_terrainPreviewIcon = terrainIcon(r.getTerrainType());
        emit sig_terrainPreviewIconChanged();

        m_align = static_cast<int>(r.getAlignType());
        emit sig_alignChanged();
        m_portable = static_cast<int>(r.getPortableType());
        emit sig_portableChanged();
        m_rideable = static_cast<int>(r.getRidableType());
        emit sig_rideableChanged();
        m_light = static_cast<int>(r.getLightType());
        emit sig_lightChanged();
        m_sundeath = static_cast<int>(r.getSundeathType());
        emit sig_sundeathChanged();

        const RawExit &e = r.getExit(static_cast<ExitDirEnum>(m_selectedExitDir));

        for (const ExitFlagEnum flag : ALL_EXIT_FLAGS) {
            m_exitFlagsModel->setState(static_cast<int>(flag),
                                       e.getExitFlags().contains(flag) ? Qt::Checked
                                                                       : Qt::Unchecked);
        }
        // Exit flag can only be removed if there are no connections.
        m_exitFlagsModel->setCheckable(static_cast<int>(ExitFlagEnum::EXIT), e.outIsEmpty());
        // Door flag can only be removed if the exit isn't a door, or is a
        // bare door (no flags, no name).
        const bool shouldEnableDoorCheck = !e.exitIsDoor()
                                           || (e.getDoorFlags().empty() && e.getDoorName().empty());
        m_exitFlagsModel->setCheckable(static_cast<int>(ExitFlagEnum::DOOR), shouldEnableDoorCheck);

        m_doorFieldsEnabled = e.exitIsDoor();
        emit sig_doorFieldsEnabledChanged();
        if (e.exitIsDoor()) {
            m_doorName = e.getDoorName().toQString();
            for (const DoorFlagEnum flag : ALL_DOOR_FLAGS) {
                m_doorFlagsModel->setState(static_cast<int>(flag),
                                           e.getDoorFlags().contains(flag) ? Qt::Checked
                                                                           : Qt::Unchecked);
            }
        } else {
            m_doorName.clear();
        }
        emit sig_doorNameChanged();

        for (const RoomMobFlagEnum flag : ALL_MOB_FLAGS) {
            m_mobFlagsModel->setState(static_cast<int>(flag),
                                      r.getMobFlags().contains(flag) ? Qt::Checked : Qt::Unchecked);
        }
        for (const RoomLoadFlagEnum flag : ALL_LOAD_FLAGS) {
            m_loadFlagsModel->setState(static_cast<int>(flag),
                                       r.getLoadFlags().contains(flag) ? Qt::Checked
                                                                       : Qt::Unchecked);
        }
    }

    emit sig_refreshed();
}
