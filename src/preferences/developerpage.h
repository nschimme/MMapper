#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../global/macros.h"
#include "ui_developerpage.h" // Add this line

#include <QWidget>
#include <QPointer> // For QPointer

// Remove the forward declaration for Ui::DeveloperPage
// class Ui::DeveloperPage;

class NODISCARD_QOBJECT DeveloperPage final : public QWidget
{
    Q_OBJECT

public:
    explicit DeveloperPage(QWidget *parent = nullptr);
    ~DeveloperPage() final;

public slots:
    void slot_loadConfig();

private:
    void populatePage();
    void filterSettings(const QString &text);

    Ui::DeveloperPage *const ui;
    QList<QWidget*> m_settingLabels; // To store QLabel for property names
    QList<QWidget*> m_settingWidgets; // To store QCheckBox, QLineEdit etc.
};
