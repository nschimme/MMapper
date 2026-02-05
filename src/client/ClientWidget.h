#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "ClientWidgetViewModel.h"
#include "../global/macros.h"
#include <QWidget>

namespace Ui { class ClientWidget; }

class NODISCARD_QOBJECT ClientWidget final : public QWidget
{
    Q_OBJECT
private:
    std::unique_ptr<ClientWidgetViewModel> m_viewModel;
    std::unique_ptr<Ui::ClientWidget> m_ui;

public:
    NODISCARD bool isUsingClient() const;
    explicit ClientWidget(ConnectionListener &l, HotkeyManager &hm, QWidget *parent);
    ~ClientWidget() final;

protected:
    NODISCARD QSize minimumSizeHint() const override;
    NODISCARD bool focusNextPrevChild(bool next) override;

signals:
    void sig_relayMessage(const QString &);

public slots:
    void slot_onVisibilityChanged(bool);
    void slot_saveLog();
    void slot_saveLogAsHtml();
};
