// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "ClientWidget.h"
#include "ui_ClientWidget.h"
#include "../configuration/configuration.h"
#include <QDateTime>
#include <QFileDialog>

ClientWidget::ClientWidget(ConnectionListener &l, HotkeyManager &hm, QWidget *const parent)
    : QWidget(parent), m_ui(new Ui::ClientWidget)
{
    m_ui->setupUi(this);
    m_viewModel = std::make_unique<ClientWidgetViewModel>(l, hm, m_ui->display->viewModel(), m_ui->input->viewModel(), this);
    m_ui->port->setText(QString::number(getConfig().connection.localPort));
    connect(m_ui->playButton, &QPushButton::clicked, this, [this]() { m_ui->parent->setCurrentIndex(1); });
    connect(m_viewModel.get(), &ClientWidgetViewModel::sig_relayMessage, this, &ClientWidget::sig_relayMessage);

    connect(m_ui->display, &DisplayWidget::sig_windowSizeChanged, [this](int w, int h) { m_ui->display->viewModel()->windowSizeChanged(w, h); });
    connect(m_ui->display, &DisplayWidget::sig_returnFocusToInput, this, [this]() { m_ui->input->setFocus(); });
    connect(m_ui->display, &DisplayWidget::sig_showPreview, this, [this](bool v) { m_ui->preview->setVisible(v); });
}

ClientWidget::~ClientWidget() = default;

QSize ClientWidget::minimumSizeHint() const { return m_ui->display->sizeHint(); }

bool ClientWidget::focusNextPrevChild(bool) {
    if (m_ui->input->hasFocus()) m_ui->display->setFocus();
    else m_ui->input->setFocus();
    return true;
}

void ClientWidget::slot_onVisibilityChanged(bool) {}

void ClientWidget::slot_saveLog() {
    QByteArray c = m_ui->display->document()->toPlainText().toUtf8();
    QFileDialog::saveFileContent(c, "log-" + QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss") + ".txt");
}

void ClientWidget::slot_saveLogAsHtml() {
    QByteArray c = m_ui->display->document()->toHtml().toUtf8();
    QFileDialog::saveFileContent(c, "log-" + QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss") + ".html");
}
bool ClientWidget::isUsingClient() const { return m_ui->parent->currentIndex() != 0; }
