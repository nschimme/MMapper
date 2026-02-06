// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "findroomsdlg.h"
#include "ui_findroomsdlg.h"
#include "../mapdata/mapdata.h"
#include "../global/SignalBlocker.h"
#include <QShortcut>

FindRoomsDlg::FindRoomsDlg(MapData &mapData, QWidget *parent)
    : QDialog(parent), ui(new Ui::FindRoomsDlg), m_viewModel(mapData, this)
{
    ui->setupUi(this);

    connect(&m_viewModel, &FindRoomsViewModel::filterTextChanged, this, &FindRoomsDlg::updateUI);
    connect(&m_viewModel, &FindRoomsViewModel::sig_resultsUpdated, this, &FindRoomsDlg::updateResults);

    connect(ui->lineEdit, &QLineEdit::textChanged, &m_viewModel, &FindRoomsViewModel::setFilterText);
    connect(ui->findButton, &QPushButton::clicked, &m_viewModel, &FindRoomsViewModel::find);
    connect(ui->closeButton, &QPushButton::clicked, this, &QDialog::close);

    // Connect radio buttons to viewModel
    auto setKind = [this](PatternKindsEnum k) { m_viewModel.setSearchKind(static_cast<int>(k)); };
    connect(ui->nameRadioButton, &QRadioButton::clicked, [=] { setKind(PatternKindsEnum::NAME); });
    connect(ui->descRadioButton, &QRadioButton::clicked, [=] { setKind(PatternKindsEnum::DESC); });
    connect(ui->contentsRadioButton, &QRadioButton::clicked, [=] { setKind(PatternKindsEnum::CONTENTS); });
    connect(ui->notesRadioButton, &QRadioButton::clicked, [=] { setKind(PatternKindsEnum::NOTE); });
    connect(ui->exitsRadioButton, &QRadioButton::clicked, [=] { setKind(PatternKindsEnum::EXITS); });
    connect(ui->flagsRadioButton, &QRadioButton::clicked, [=] { setKind(PatternKindsEnum::FLAGS); });
    connect(ui->areaRadioButton, &QRadioButton::clicked, [=] { setKind(PatternKindsEnum::AREA); });
    connect(ui->allRadioButton, &QRadioButton::clicked, [=] { setKind(PatternKindsEnum::ALL); });

    connect(ui->caseCheckBox, &QCheckBox::toggled, &m_viewModel, &FindRoomsViewModel::setCaseSensitive);
    connect(ui->regexCheckBox, &QCheckBox::toggled, &m_viewModel, &FindRoomsViewModel::setUseRegex);

    connect(ui->resultTable, &QTreeWidget::itemDoubleClicked, this, &FindRoomsDlg::slot_itemDoubleClicked);
    connect(ui->selectButton, &QPushButton::clicked, this, &FindRoomsDlg::slot_showSelectedRoom);

    m_showSelectedRoom = std::make_unique<QShortcut>(QKeySequence(Qt::Key_Return), this);
    connect(m_showSelectedRoom.get(), &QShortcut::activated, this, &FindRoomsDlg::slot_showSelectedRoom);

    updateUI();
}

FindRoomsDlg::~FindRoomsDlg() = default;

void FindRoomsDlg::updateUI()
{
    SignalBlocker sb(*ui->lineEdit);
    ui->lineEdit->setText(m_viewModel.filterText());
    ui->findButton->setEnabled(!m_viewModel.filterText().isEmpty());
}

void FindRoomsDlg::updateResults()
{
    ui->resultTable->clear();
    const auto &results = m_viewModel.results();
    for (const auto &id : results) {
        if (auto room = m_viewModel.mapData().findRoomHandle(id)) {
            auto *item = new QTreeWidgetItem(ui->resultTable);
            item->setText(0, room.getName().toQString());
            item->setText(1, room.getArea().toQString());
            item->setData(0, Qt::UserRole, QVariant::fromValue(id));
        }
    }
    ui->roomsFoundLabel->setText(tr("%n room(s) found", "", m_viewModel.roomsFound()));
}

void FindRoomsDlg::slot_itemDoubleClicked(QTreeWidgetItem *) { slot_showSelectedRoom(); }

void FindRoomsDlg::slot_showSelectedRoom()
{
    auto *item = ui->resultTable->currentItem();
    if (!item) return;
    RoomId id = item->data(0, Qt::UserRole).value<RoomId>();
    m_viewModel.mapData().setRoom(id);
    m_viewModel.mapData().forceToRoom(id);
}
