// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "roomeditattrdlg.h"
#include "ui_roomeditattrdlg.h"
#include "../mapdata/mapdata.h"
#include "../global/SignalBlocker.h"

RoomEditAttrDlg::RoomEditAttrDlg(MapData &mapData, QWidget *parent)
    : QDialog(parent), ui(new Ui::RoomEditAttrDlg), m_viewModel(mapData, this)
{
    ui->setupUi(this);

    connect(&m_viewModel, &RoomEditAttrViewModel::roomNamesChanged, this, &RoomEditAttrDlg::updateUI);
    connect(&m_viewModel, &RoomEditAttrViewModel::currentRoomIndexChanged, this, &RoomEditAttrDlg::updateUI);
    connect(&m_viewModel, &RoomEditAttrViewModel::roomDescriptionChanged, this, &RoomEditAttrDlg::updateUI);
    connect(&m_viewModel, &RoomEditAttrViewModel::roomNoteChanged, this, &RoomEditAttrDlg::updateUI);

    connect(ui->roomListComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), &m_viewModel, &RoomEditAttrViewModel::setCurrentRoomIndex);
    connect(ui->roomNoteApplyButton, &QPushButton::clicked, &m_viewModel, &RoomEditAttrViewModel::applyNote);
    connect(ui->roomNoteRevertButton, &QPushButton::clicked, &m_viewModel, &RoomEditAttrViewModel::revertNote);
    connect(ui->roomNoteClearButton, &QPushButton::clicked, &m_viewModel, &RoomEditAttrViewModel::clearNote);
    connect(ui->closeButton, &QPushButton::clicked, this, &QDialog::close);

    updateUI();
}

RoomEditAttrDlg::~RoomEditAttrDlg() = default;

void RoomEditAttrDlg::updateUI()
{
    SignalBlocker sb1(*ui->roomListComboBox);
    ui->roomListComboBox->clear();
    ui->roomListComboBox->addItems(m_viewModel.roomNames());
    ui->roomListComboBox->setCurrentIndex(m_viewModel.currentRoomIndex());

    ui->roomDescriptionTextEdit->setPlainText(m_viewModel.roomDescription());

    SignalBlocker sb2(*ui->roomNoteTextEdit);
    ui->roomNoteTextEdit->setPlainText(m_viewModel.roomNote());
}
