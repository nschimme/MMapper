#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../global/macros.h"

#include <QColor>
#include <QWidget>

class QCheckBox;
class QPushButton;
class QLabel;
class QColorDialog;

class NODISCARD_QOBJECT GroupPage final : public QWidget
{
    Q_OBJECT

public:
    explicit GroupPage(QWidget *parent = nullptr);
    ~GroupPage() final;

signals:
    void sig_settingsChanged(); // To notify ConfigDialog if needed, though saving on apply/close is typical

public slots:
    void slot_loadConfig();
    void slot_saveConfig(); // Could be connected to a general "apply" button in ConfigDialog
                            // or called when the dialog is accepted.

private slots:
    void slot_filterNpcsChanged(int state);
    void slot_chooseColor();

private:
    void setupUi();

    QCheckBox *m_filterNpcsCheckBox = nullptr;
    QPushButton *m_colorButton = nullptr;
    QLabel *m_colorPreviewLabel = nullptr; // To show the selected color

    QCheckBox *m_overrideNpcColorCheckBox = nullptr;
    QPushButton *m_npcOverrideColorButton = nullptr;
    QLabel *m_npcOverrideColorPreviewLabel = nullptr;
    QCheckBox *m_sortNpcsToBottomCheckBox = nullptr;

    QColor m_selectedColor;
    QColor m_selectedNpcOverrideColor;

private slots:
    void slot_chooseNpcOverrideColor();
};
