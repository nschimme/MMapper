#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "DescriptionViewModel.h"
#include <QWidget>
#include <QLabel>
#include <QTextEdit>
#include <QCache>
#include <QFileSystemWatcher>

class NODISCARD_QOBJECT DescriptionWidget final : public QWidget
{
    Q_OBJECT
private:
    DescriptionViewModel m_viewModel;
    QLabel *m_label;
    QTextEdit *m_textEdit;
    QCache<QString, QImage> m_imageCache;
    QFileSystemWatcher m_watcher;

public:
    explicit DescriptionWidget(QWidget *parent = nullptr);
    ~DescriptionWidget() final = default;

    DescriptionViewModel* viewModel() { return &m_viewModel; }
    void updateRoom(const RoomHandle &r) { m_viewModel.updateRoom(r); }

protected:
    void resizeEvent(QResizeEvent *event) override;
    NODISCARD QSize minimumSizeHint() const override;
    NODISCARD QSize sizeHint() const override;

private slots:
    void updateUI();
    void updateBackground();
};
