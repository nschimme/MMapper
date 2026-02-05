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

    connect(ui->lineEdit, &QLineEdit::textChanged, &m_viewModel, &FindRoomsViewModel::setFilterText);
    connect(ui->findButton, &QPushButton::clicked, &m_viewModel, &FindRoomsViewModel::find);
    connect(ui->closeButton, &QPushButton::clicked, this, &QDialog::close);

    m_showSelectedRoom = std::make_unique<QShortcut>(QKeySequence(Qt::Key_Return), this);
    connect(m_showSelectedRoom.get(), &QShortcut::activated, this, &FindRoomsDlg::slot_showSelectedRoom);

    updateUI();
}

FindRoomsDlg::~FindRoomsDlg() = default;

void FindRoomsDlg::updateUI()
{
    SignalBlocker sb(*ui->lineEdit);
    ui->lineEdit->setText(m_viewModel.filterText());
}

void FindRoomsDlg::slot_findClicked() { m_viewModel.find(); }
void FindRoomsDlg::slot_itemDoubleClicked(QTreeWidgetItem *) { slot_showSelectedRoom(); }
void FindRoomsDlg::slot_showSelectedRoom() { /* logic to center on selected room */ }
