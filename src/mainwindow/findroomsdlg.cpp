// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Kalev Lember <kalev@smartlink.ee> (Kalev)
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "findroomsdlg.h"

#include "../configuration/configuration.h"
#include "../global/AsyncTasks.h"
#include "../global/parserutils.h"
#include "../global/progresscounter.h"
#include "../global/window_utils.h"
#include "../map/ExitDirection.h"
#include "../map/coordinate.h"
#include "../map/roomid.h"
#include "../mapdata/GenericFind.h"
#include "../mapdata/mapdata.h"
#include "../mapdata/roomfilter.h"
#include "../mapdata/roomselection.h"
#include "../mapdata/shortestpath.h"

#include <cmath>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <QString>
#include <QtGui>
#include <QtWidgets>

NODISCARD static QString formatDistance(const double dist)
{
    if (dist == std::floor(dist)) {
        return QString::number(static_cast<int64_t>(dist));
    }
    return QString::number(dist, 'f', 2);
}

FindRoomsDlg::FindRoomsDlg(MapData &md, QWidget *const parent)
    : QDialog(parent)
    , m_mapData{md}
{
    setupUi(this);

    adjustResultTable();

    m_showSelectedRoom = std::make_unique<QShortcut>(QKeySequence(tr("Space", "Select result item")),
                                                     resultTable);
    m_showSelectedRoom->setContext(Qt::WidgetShortcut);

    selectButton->setEnabled(false);
    editButton->setEnabled(false);

    findButton->setDefault(true);

    connect(lineEdit, &QLineEdit::textChanged, this, &FindRoomsDlg::slot_enableFindButton);
    connect(findButton, &QAbstractButton::clicked, this, &FindRoomsDlg::slot_findClicked);
    connect(closeButton, &QAbstractButton::clicked, this, &QWidget::close);
    connect(resultTable,
            &QTreeWidget::itemDoubleClicked,
            this,
            &FindRoomsDlg::slot_itemDoubleClicked);
    connect(m_showSelectedRoom.get(),
            &QShortcut::activated,
            this,
            &FindRoomsDlg::slot_showSelectedRoom);
    connect(resultTable, &QTreeWidget::itemSelectionChanged, this, [this]() {
        const bool enabled = !resultTable->selectedItems().isEmpty();
        selectButton->setEnabled(enabled);
        editButton->setEnabled(enabled);
    });
    connect(selectButton, &QAbstractButton::clicked, this, [this]() {
        auto map = m_mapData.getCurrentMap();
        RoomIdSet tmpSet;
        glm::vec2 sum{0.f, 0.f};
        for (const auto &selectedItem : resultTable->selectedItems()) {
            const auto id = ExternalRoomId{selectedItem->text(0).toUInt()};
            if (const auto room = map.findRoomHandle(id)) {
                sum += room.getPosition().to_vec2();
                tmpSet.insert(room.getId());
            }
        }

        if (!tmpSet.empty()) {
            // note: half-room offset to the room center
            const glm::vec2 offset{0.5f, 0.5f};
            const auto worldPos = sum / static_cast<float>(tmpSet.size()) + offset;
            emit sig_center(worldPos);
        }

        auto shared = RoomSelection::createSelection(std::move(tmpSet));
        emit sig_newRoomSelection(SigRoomSelection{std::move(shared)});
    });
    connect(editButton, &QAbstractButton::clicked, this, [this]() {
        const auto &map = m_mapData;
        const auto &currentMap = map.getCurrentMap();
        RoomIdSet set;
        for (const auto &selectedItem : resultTable->selectedItems()) {
            const auto extid = ExternalRoomId{selectedItem->text(0).toUInt()};
            if (const auto room = currentMap.findRoomHandle(extid)) {
                set.insert(room.getId());
            }
        }
        auto tmpSel = RoomSelection::createSelection(std::move(set));
        emit sig_newRoomSelection(SigRoomSelection{std::move(tmpSel)});
        emit sig_editSelection();
    });

    label->setFocusProxy(lineEdit);

    readSettings();
}

FindRoomsDlg::~FindRoomsDlg()
{
    if (m_currentTask) {
        m_currentTask->requestCancel();
        m_currentTask.reset();
    }
    resultTable->clear();
}

void FindRoomsDlg::showEvent(QShowEvent *const event)
{
    QDialog::showEvent(event);
    lineEdit->setFocus();
}

void FindRoomsDlg::readSettings()
{
    restoreGeometry(getConfig().findRoomsDialog.geometry);
}

void FindRoomsDlg::writeSettings()
{
    setConfig().findRoomsDialog.geometry = saveGeometry();
}

void FindRoomsDlg::slot_findClicked()
{
    const Qt::CaseSensitivity cs = caseCheckBox->isChecked() ? Qt::CaseSensitive
                                                             : Qt::CaseInsensitive;
    const bool regex = regexCheckBox->isChecked();

    const std::string text = mmqt::toStdStringUtf8(lineEdit->text());

    if (m_currentTask) {
        m_currentTask->requestCancel();
        m_currentTask.reset();
    }
    const uint64_t gen = ++m_searchGeneration;

    resultTable->clear();
    roomsFoundLabel->setText(tr("Searching..."));
    selectButton->setEnabled(false);
    editButton->setEnabled(false);

    auto kind = PatternKindsEnum::ALL;
    if (nameRadioButton->isChecked()) {
        kind = PatternKindsEnum::NAME;
    } else if (descRadioButton->isChecked()) {
        kind = PatternKindsEnum::DESC;
    } else if (contentsRadioButton->isChecked()) {
        kind = PatternKindsEnum::CONTENTS;
    } else if (exitsRadioButton->isChecked()) {
        kind = PatternKindsEnum::EXITS;
    } else if (notesRadioButton->isChecked()) {
        kind = PatternKindsEnum::NOTE;
    } else if (flagsRadioButton->isChecked()) {
        kind = PatternKindsEnum::FLAGS;
    } else if (areaRadioButton->isChecked()) {
        kind = PatternKindsEnum::AREA;
    }

    try {
        RoomFilter filter(text, cs, regex, kind);
        Map map = m_mapData.getCurrentMap();
        RoomHandle origin = m_mapData.getCurrentRoom();
        const bool hasOrigin = origin.exists();

        struct FoundRoomData
        {
            ExternalRoomId extId;
            RoomId roomId;
            QString name;
            QString toolTip;
        };

        auto backgroundWorker = [this, filter, map, origin, hasOrigin, gen](
                                    const std::shared_ptr<ProgressCounter> &pc) {
            if (pc->hasRequestedCancel()) {
                return;
            }

            const auto foundSet = ::genericFind(map, filter);
            if (pc->hasRequestedCancel()) {
                return;
            }

            std::vector<FoundRoomData> items;
            items.reserve(foundSet.size());
            for (const auto roomId : foundSet) {
                const auto &room = map.getRoomHandle(roomId);
                items.push_back(FoundRoomData{room.getIdExternal(),
                                              roomId,
                                              room.getName().toQString(),
                                              constructToolTip(room)});
            }

            QMetaObject::invokeMethod(
                this,
                [this, gen, items, hasOrigin]() {
                    if (m_searchGeneration != gen) {
                        return;
                    }

                    resultTable->setSortingEnabled(false);
                    resultTable->clear();

                    for (const auto &item : items) {
                        auto *treeItem = new FindRoomTreeWidgetItem(resultTable);
                        treeItem->setText(0, QString::number(item.extId.asUint32()));
                        treeItem->setText(1, item.name);
                        treeItem->setText(2, hasOrigin ? tr("...") : tr("N/A"));
                        treeItem->setToolTip(0, item.toolTip);
                        treeItem->setToolTip(1, item.toolTip);
                        treeItem->setToolTip(2, item.toolTip);
                        treeItem->setData(0,
                                          Qt::UserRole,
                                          QVariant::fromValue(item.roomId.asUint32()));
                    }

                    roomsFoundLabel->setText(
                        tr("%1 room%2 found")
                            .arg(resultTable->topLevelItemCount())
                            .arg((resultTable->topLevelItemCount() == 1) ? "" : "s"));
                    resultTable->setSortingEnabled(true);
                },
                Qt::QueuedConnection);

            if (!hasOrigin || items.empty()) {
                return;
            }

            struct DistUpdate
            {
                RoomId id;
                double dist;
            };

            std::vector<DistUpdate> batch;
            batch.reserve(64);

            auto flushBatch = [this, gen, &batch]() {
                if (batch.empty()) {
                    return;
                }
                auto currentBatch = std::move(batch);
                batch.clear();

                QMetaObject::invokeMethod(
                    this,
                    [this, gen, updates = std::move(currentBatch)]() {
                        if (m_searchGeneration != gen) {
                            return;
                        }

                        resultTable->setSortingEnabled(false);
                        for (int i = 0; i < resultTable->topLevelItemCount(); ++i) {
                            auto *treeItem = resultTable->topLevelItem(i);
                            if (treeItem == nullptr) {
                                continue;
                            }
                            const auto rId = RoomId{treeItem->data(0, Qt::UserRole).toUInt()};
                            for (const auto &u : updates) {
                                if (rId == u.id) {
                                    treeItem->setText(2, formatDistance(u.dist));
                                    break;
                                }
                            }
                        }
                        resultTable->setSortingEnabled(true);
                    },
                    Qt::QueuedConnection);
            };

            class FindRecipient final : public ShortestPathRecipient
            {
            private:
                std::function<void(RoomId, double)> m_cb;

            public:
                explicit FindRecipient(std::function<void(RoomId, double)> cb)
                    : m_cb(std::move(cb))
                {}

            private:
                void virt_receiveShortestPath(QVector<SPNode> spnodes, const int endpoint) override
                {
                    if (endpoint >= 0 && endpoint < spnodes.size()) {
                        const auto &node = spnodes[endpoint];
                        m_cb(node.r.getId(), node.dist);
                    }
                }
            };

            FindRecipient recipient([&](RoomId rId, double dist) {
                if (pc->hasRequestedCancel()) {
                    return;
                }
                batch.push_back(DistUpdate{rId, dist});
                if (batch.size() >= 50) {
                    flushBatch();
                }
            });

            MapData::shortestPathSearch(*pc, origin, recipient, filter, -1, 0);
            flushBatch();

            QMetaObject::invokeMethod(
                this,
                [this, gen]() {
                    if (m_searchGeneration != gen) {
                        return;
                    }

                    resultTable->setSortingEnabled(false);
                    for (int i = 0; i < resultTable->topLevelItemCount(); ++i) {
                        auto *treeItem = resultTable->topLevelItem(i);
                        if (treeItem != nullptr && treeItem->text(2) == tr("...")) {
                            treeItem->setText(2, tr("N/A"));
                        }
                    }
                    resultTable->setSortingEnabled(true);
                },
                Qt::QueuedConnection);
        };

        auto onSuccess = [](const std::shared_ptr<ProgressCounter> &) {};

        m_currentTask = async_tasks::startAsyncTask2(AsyncTaskTypeEnum::Task,
                                                     AllowCancelEnum::Allow,
                                                     "Find Rooms",
                                                     std::move(backgroundWorker),
                                                     std::move(onSuccess));
    } catch (const std::exception &ex) {
        qWarning() << "Exception: " << ex.what();
        mmqt::showCritical(this,
                           "Internal Error",
                           QString::asprintf("An exception occurred: %s\n", ex.what()));
        roomsFoundLabel->clear();
    }
}

QString FindRoomsDlg::constructToolTip(const RoomHandle &room)
{
    // Tooltip doesn't support ANSI, and there's no way to add formatted text.
    return mmqt::previewRoom(room, mmqt::StripAnsiEnum::Yes, mmqt::PreviewStyleEnum::ForDisplay);
}

void FindRoomsDlg::slot_showSelectedRoom()
{
    slot_itemDoubleClicked(resultTable->currentItem());
}

void FindRoomsDlg::slot_itemDoubleClicked(QTreeWidgetItem *const inputItem)
{
    if (inputItem == nullptr) {
        return;
    }

    const auto &map = m_mapData.getCurrentMap();
    const auto extId = ExternalRoomId{inputItem->text(0).toUInt()};
    if (const auto r = map.findRoomHandle(extId)) {
        // REVISIT: should this block be a stand-alone function?
        {
            assert(r.getIdExternal() == extId);
            const Coordinate c = r.getPosition();
            const auto worldPos = c.to_vec2() + glm::vec2{0.5f, 0.5f};
            emit sig_center(worldPos); // connects to MapWindow
        }
        emit sig_log("FindRooms", inputItem->toolTip(0));
    }
}

void FindRoomsDlg::adjustResultTable()
{
    resultTable->setColumnCount(3);
    resultTable->setHeaderLabels(QStringList()
                                 << tr("Room ID") << tr("Room Name") << tr("Distance"));
    resultTable->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    resultTable->setRootIsDecorated(false);
    resultTable->setAlternatingRowColors(true);
    resultTable->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
    resultTable->setSelectionMode(QAbstractItemView::SelectionMode::ExtendedSelection);
}

void FindRoomsDlg::slot_enableFindButton(const QString &text)
{
    findButton->setEnabled(!text.isEmpty());
}

void FindRoomsDlg::closeEvent(QCloseEvent *event)
{
    if (m_currentTask) {
        m_currentTask->requestCancel();
        m_currentTask.reset();
    }
    ++m_searchGeneration;

    writeSettings();
    resultTable->clear();
    roomsFoundLabel->clear();
    lineEdit->clear();
    lineEdit->setFocus();
    selectButton->setEnabled(false);
    editButton->setEnabled(false);
    event->accept();
}

void FindRoomsDlg::slot_on_lineEdit_textChanged()
{
    findButton->setEnabled(lineEdit->hasAcceptableInput());
}
