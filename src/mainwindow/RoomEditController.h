#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"
#include "../map/DoorFlags.h"
#include "../map/ExitDirection.h"
#include "../map/ExitFieldVariant.h"
#include "../map/ExitFlags.h"
#include "../map/RoomHandle.h"
#include "../map/mmapper2room.h"
#include "../mapdata/roomselection.h"

#include <functional>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QtCore>

class Change;
class CheckableFlagModel;
class MapData;
class RoomFieldVariant;

// Lifts the field-population/commit logic RoomEditAttrDlg's slots and
// updateDialog() perform (see roomeditattrdlg.cpp) into a QObject that can be
// driven from QML. Mirrors InfomarkEditController's/FindRoomsController's
// role: owns no widgets, exposes the widget's fields as Q_PROPERTYs and its
// button clicks / checkbox toggles as Q_INVOKABLEs. Unlike the widget, every
// field edit commits to MapData immediately (there is no separate "OK"/
// "Apply" step, except for the room note -- see noteText below).
//
// QML contract (stub-drift guard: TestQml.cpp's RoomEditControllerStub must
// keep this surface in sync when it's added):
//   Q_PROPERTY QStringList roomNames -- mirrors the widget's roomListComboBox:
//     when the selection has more than one room, index 0 is "All" followed by
//     one "Room <id>: <name>" entry per room (in selection order); when the
//     selection has exactly one room, it's the sole entry (no "All").
//   Q_PROPERTY int currentRoomIndex (WRITE setCurrentRoomIndex) -- an index
//     into roomNames; writing it re-publishes every property below from
//     either the selected room, or (index 0 / "All", or an empty selection)
//     the widget's "no room" display state.
//   Q_PROPERTY bool hasSelectedRoom -- false when "All" (or nothing) is
//     selected; QML should disable the exits/description/note/stats/diff
//     controls when false, matching updateDialog()'s !r.exists() branch.
//   Q_PROPERTY CheckableFlagModel* mobFlagsModel / loadFlagsModel -- whole-
//     room flags, rows sorted by the widget's getPriority() (a handful of
//     flags pinned first, the rest in enum order), with icons. In "All" mode
//     every row is PartiallyChecked with NO union computation (matches the
//     widget exactly -- it never inspects the other rooms in the selection).
//   Q_PROPERTY CheckableFlagModel* exitFlagsModel / doorFlagsModel -- flags
//     for the exit named by selectedExitDir; text-only rows (no icons), enum
//     order (the widget doesn't sort these lists). The EXIT row is checkable
//     only when the exit has no outgoing connection (outIsEmpty()); the DOOR
//     row is checkable only when the exit isn't a door, or is a door with no
//     flags and an empty name; UNMAPPED is never checkable. Door row
//     checkability never changes -- doorFieldsEnabled controls the whole
//     list's usability instead (see below), matching the widget's
//     doorFlagsListWidget->setEnabled() rather than per-item flags.
//   Q_PROPERTY int selectedExitDir (WRITE setSelectedExitDir) -- an
//     ExitDirEnum ordinal, 0-5 (NESWUD); writing it re-publishes
//     exitFlagsModel/doorFlagsModel/doorName/doorFieldsEnabled (and, like the
//     widget's updateDialog(), everything else too).
//   Q_PROPERTY QString doorName (WRITE setDoorName) -- commits immediately
//     (ASSIGN) unlike the widget's editingFinished-only line edit.
//   Q_PROPERTY bool doorFieldsEnabled -- true only when a room is selected
//     and the selected exit is a door.
//   Q_PROPERTY int align/portable/rideable/light/sundeath (WRITE
//     setAlign()/etc.) -- the matching *Enum's ordinal; -1 in "All" mode or
//     when no room is selected (a sentinel with no widget equivalent -- the
//     widget just leaves whichever checkable QGroupBox unchecked).
//   Q_PROPERTY int terrainType (WRITE setTerrain) -- a RoomTerrainEnum
//     ordinal; -1 sentinel as above.
//   Q_PROPERTY QUrl terrainPreviewIcon -- the terrain pixmap for the current
//     terrainType, with the widget's ROAD special case (a 3-way N/E/S
//     junction icon, since there's no "current room's road connections"
//     concept outside of the actual map); RoomTerrainEnum::UNDEFINED's icon
//     when no room is selected.
//   Q_PROPERTY QString noteText (WRITE setNoteText) -- setNoteText() only
//     marks noteDirty; it does NOT commit (mirrors the widget's note tab,
//     which requires an explicit Apply). QML should copy this into a local
//     editable field on sig_refreshed() and call setNoteText() as the user
//     types, then applyNote()/revertNote()/clearNote() for the buttons.
//   Q_PROPERTY bool noteDirty -- see above.
//   Q_INVOKABLE applyNote() -- commits noteText as the room's note
//     (onlyExecuteAction: does NOT emit sig_requestUpdate, matching the
//     widget's roomNoteChanged()), then refreshes.
//   Q_INVOKABLE revertNote() -- discards local edits, re-reads noteText from
//     the room; does not touch MapData.
//   Q_INVOKABLE clearNote() -- empties noteText; marks noteDirty only if the
//     room actually had a note (a genuine widget quirk -- see
//     roomeditattrdlg.cpp's roomNoteClearButton handler).
//   Q_PROPERTY QString descriptionHtml/statsHtml/diffHtml -- read-only ANSI
//     previews rendered via mmqt::ansiToHtml() with the integrated client's
//     configured colors, equivalent to previewRoom()/Map::statRoom()/
//     OstreamDiffReporter vs. the saved map. Unlike the widget, diffHtml is
//     empty when there is nothing to show (no informational placeholder
//     text) -- QML is expected to supply its own empty-state text.
//   Q_PROPERTY bool revertEnabled -- true only when diffHtml reflects an
//     actual, revertible diff.
//   Q_INVOKABLE revertDiff() -- port of onRevertDiffClicked(): builds and
//     applies a room_revert::RevertPlan, replacing QMessageBox::warning()
//     with sig_error(QString). Like the widget, this does NOT emit
//     sig_requestUpdate() (a widget quirk, ported as-is).
//   Q_INVOKABLE toggleMobFlag(int row)/toggleLoadFlag(int row) -- ports the
//     widget's itemChanged tri-state switch: a row currently Checked is
//     REMOVEd; Unchecked or PartiallyChecked is INSERTed. The resulting
//     state is whatever refresh() re-reads from MapData afterward (in "All"
//     mode that's always PartiallyChecked again, regardless of the change's
//     success -- no optimistic/local state is kept).
//   Q_INVOKABLE toggleExitFlag(int row)/toggleDoorFlag(int row) -- same
//     tri-state rule, applied to the exit named by selectedExitDir via
//     ModifyExitFlags; always emits sig_requestUpdate() and refreshes
//     afterward (matches the widget's unconditional post-switch
//     updateDialog()).
//   Q_INVOKABLE toggleHiddenDoor() -- the Ctrl+H shortcut's action: flips the
//     HIDDEN door flag through the same path as toggleDoorFlag(); a no-op
//     when doorFieldsEnabled is false.
// Signals: sig_requestUpdate() (canvas repaint request), sig_error(QString)
// (replaces QMessageBox::warning()), a NOTIFY signal per property above, and
// sig_refreshed() fired at the end of every bulk refresh (setRoomSelection(),
// setCurrentRoomIndex(), setSelectedExitDir(), any commit, applyNote(),
// revertNote(), clearNote(), revertDiff()) for QML to re-copy properties into
// local editable fields.
class NODISCARD_QOBJECT RoomEditController final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList roomNames READ getRoomNames NOTIFY sig_roomNamesChanged)
    Q_PROPERTY(int currentRoomIndex READ getCurrentRoomIndex WRITE setCurrentRoomIndex NOTIFY
                   sig_currentRoomIndexChanged)
    Q_PROPERTY(bool hasSelectedRoom READ getHasSelectedRoom NOTIFY sig_hasSelectedRoomChanged)

    Q_PROPERTY(CheckableFlagModel *mobFlagsModel READ getMobFlagsModel CONSTANT)
    Q_PROPERTY(CheckableFlagModel *loadFlagsModel READ getLoadFlagsModel CONSTANT)
    Q_PROPERTY(CheckableFlagModel *exitFlagsModel READ getExitFlagsModel CONSTANT)
    Q_PROPERTY(CheckableFlagModel *doorFlagsModel READ getDoorFlagsModel CONSTANT)

    Q_PROPERTY(int selectedExitDir READ getSelectedExitDir WRITE setSelectedExitDir NOTIFY
                   sig_selectedExitDirChanged)
    Q_PROPERTY(QString doorName READ getDoorName WRITE setDoorName NOTIFY sig_doorNameChanged)
    Q_PROPERTY(bool doorFieldsEnabled READ getDoorFieldsEnabled NOTIFY sig_doorFieldsEnabledChanged)

    Q_PROPERTY(int align READ getAlign WRITE setAlign NOTIFY sig_alignChanged)
    Q_PROPERTY(int portable READ getPortable WRITE setPortable NOTIFY sig_portableChanged)
    Q_PROPERTY(int rideable READ getRideable WRITE setRideable NOTIFY sig_rideableChanged)
    Q_PROPERTY(int light READ getLight WRITE setLight NOTIFY sig_lightChanged)
    Q_PROPERTY(int sundeath READ getSundeath WRITE setSundeath NOTIFY sig_sundeathChanged)
    Q_PROPERTY(int terrainType READ getTerrainType WRITE setTerrain NOTIFY sig_terrainTypeChanged)
    Q_PROPERTY(
        QUrl terrainPreviewIcon READ getTerrainPreviewIcon NOTIFY sig_terrainPreviewIconChanged)

    Q_PROPERTY(QString noteText READ getNoteText WRITE setNoteText NOTIFY sig_noteTextChanged)
    Q_PROPERTY(bool noteDirty READ getNoteDirty NOTIFY sig_noteDirtyChanged)

    Q_PROPERTY(QString descriptionHtml READ getDescriptionHtml NOTIFY sig_descriptionHtmlChanged)
    Q_PROPERTY(QString statsHtml READ getStatsHtml NOTIFY sig_statsHtmlChanged)
    Q_PROPERTY(QString diffHtml READ getDiffHtml NOTIFY sig_diffHtmlChanged)
    Q_PROPERTY(bool revertEnabled READ getRevertEnabled NOTIFY sig_revertEnabledChanged)

private:
    SharedRoomSelection m_roomSelection;
    MapData *m_mapData = nullptr;

    CheckableFlagModel *m_mobFlagsModel = nullptr;
    CheckableFlagModel *m_loadFlagsModel = nullptr;
    CheckableFlagModel *m_exitFlagsModel = nullptr;
    CheckableFlagModel *m_doorFlagsModel = nullptr;

    QStringList m_roomNames;
    std::vector<uint32_t> m_roomIdData; // parallel to m_roomNames
    int m_currentRoomIndex = 0;
    bool m_hasSelectedRoom = false;

    int m_selectedExitDir = 0; // ExitDirEnum::NORTH, matching exitNButton's default-checked state
    QString m_doorName;
    bool m_doorFieldsEnabled = false;

    int m_align = -1;
    int m_portable = -1;
    int m_rideable = -1;
    int m_light = -1;
    int m_sundeath = -1;
    int m_terrainType = -1;
    QUrl m_terrainPreviewIcon;

    QString m_noteText;
    bool m_noteDirty = false;

    QString m_descriptionHtml;
    QString m_statsHtml;
    QString m_diffHtml;
    bool m_revertEnabled = false;

public:
    explicit RoomEditController(QObject *parent);

public:
    void setRoomSelection(const SharedRoomSelection &, MapData *);

public:
    NODISCARD QStringList getRoomNames() const { return m_roomNames; }
    NODISCARD int getCurrentRoomIndex() const { return m_currentRoomIndex; }
    NODISCARD bool getHasSelectedRoom() const { return m_hasSelectedRoom; }

    NODISCARD CheckableFlagModel *getMobFlagsModel() const { return m_mobFlagsModel; }
    NODISCARD CheckableFlagModel *getLoadFlagsModel() const { return m_loadFlagsModel; }
    NODISCARD CheckableFlagModel *getExitFlagsModel() const { return m_exitFlagsModel; }
    NODISCARD CheckableFlagModel *getDoorFlagsModel() const { return m_doorFlagsModel; }

    NODISCARD int getSelectedExitDir() const { return m_selectedExitDir; }
    NODISCARD QString getDoorName() const { return m_doorName; }
    NODISCARD bool getDoorFieldsEnabled() const { return m_doorFieldsEnabled; }

    NODISCARD int getAlign() const { return m_align; }
    NODISCARD int getPortable() const { return m_portable; }
    NODISCARD int getRideable() const { return m_rideable; }
    NODISCARD int getLight() const { return m_light; }
    NODISCARD int getSundeath() const { return m_sundeath; }
    NODISCARD int getTerrainType() const { return m_terrainType; }
    NODISCARD QUrl getTerrainPreviewIcon() const { return m_terrainPreviewIcon; }

    NODISCARD QString getNoteText() const { return m_noteText; }
    NODISCARD bool getNoteDirty() const { return m_noteDirty; }

    NODISCARD QString getDescriptionHtml() const { return m_descriptionHtml; }
    NODISCARD QString getStatsHtml() const { return m_statsHtml; }
    NODISCARD QString getDiffHtml() const { return m_diffHtml; }
    NODISCARD bool getRevertEnabled() const { return m_revertEnabled; }

    void setCurrentRoomIndex(int index);
    void setSelectedExitDir(int dir);
    void setDoorName(const QString &name);

    void setAlign(int value);
    void setPortable(int value);
    void setRideable(int value);
    void setLight(int value);
    void setSundeath(int value);
    void setTerrain(int value);

    void setNoteText(const QString &text);

    Q_INVOKABLE void toggleMobFlag(int row);
    Q_INVOKABLE void toggleLoadFlag(int row);
    Q_INVOKABLE void toggleExitFlag(int row);
    Q_INVOKABLE void toggleDoorFlag(int row);
    Q_INVOKABLE void toggleHiddenDoor();

    Q_INVOKABLE void applyNote();
    Q_INVOKABLE void revertNote();
    Q_INVOKABLE void clearNote();

    Q_INVOKABLE void revertDiff();

private:
    NODISCARD RoomHandle getSelectedRoom();

    void updateCommon(const std::function<Change(const RawRoom &)> &getChange,
                      bool onlyExecuteAction = false);
    void setFieldCommon(const RoomFieldVariant &var,
                        FlagModifyModeEnum mode,
                        bool onlyExecuteAction = false);
    void setSelectedRoomExitField(const ExitFieldVariant &var,
                                  ExitDirEnum dir,
                                  FlagModifyModeEnum mode);

    void toggleDoorFlagByEnum(DoorFlagEnum flag);

    void refresh();

signals:
    void sig_requestUpdate();
    void sig_error(const QString &);

    void sig_roomNamesChanged();
    void sig_currentRoomIndexChanged();
    void sig_hasSelectedRoomChanged();

    void sig_selectedExitDirChanged();
    void sig_doorNameChanged();
    void sig_doorFieldsEnabledChanged();

    void sig_alignChanged();
    void sig_portableChanged();
    void sig_rideableChanged();
    void sig_lightChanged();
    void sig_sundeathChanged();
    void sig_terrainTypeChanged();
    void sig_terrainPreviewIconChanged();

    void sig_noteTextChanged();
    void sig_noteDirtyChanged();

    void sig_descriptionHtmlChanged();
    void sig_statsHtmlChanged();
    void sig_diffHtmlChanged();
    void sig_revertEnabledChanged();

    // Fired at the end of every refresh(); QML should re-copy properties into
    // local editable fields when this fires (see the class doc comment).
    void sig_refreshed();
};
