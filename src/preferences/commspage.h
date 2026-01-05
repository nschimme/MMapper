#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2025 The MMapper Authors

#include "../configuration/NamedConfig.h"
#include "../global/utils.h"

#include <QCheckBox>
#include <QColor>
#include <QList>
#include <QWidget>

class QFormLayout;
class QPushButton;

class NODISCARD_QOBJECT CommsPage final : public QWidget
{
    Q_OBJECT

public:
    explicit CommsPage(QWidget *parent);
    ~CommsPage() final;

    DELETE_CTORS_AND_ASSIGN_OPS(CommsPage);

signals:
    void sig_commsSettingsChanged();

public slots:
    void slot_loadConfig();

private slots:
    void slot_onColorClicked();
    void slot_onCheckboxToggled(bool checked);

private:
    // Helper structs to associate widgets with their config objects
    struct ColorSetting
    {
        QPushButton *button = nullptr;
        NamedConfig<QColor> *config = nullptr;
        QString label;
    };

    struct CheckboxSetting
    {
        QCheckBox *checkbox = nullptr;
        NamedConfig<bool> *config = nullptr;
    };

    void setupUI();
    void connectSignals();
    void updateColorButton(QPushButton *button, const QColor &color);

    // Helper methods for UI creation
    void createColorButton(QFormLayout *layout, const QString &label, NamedConfig<QColor> &config);
    void createCheckbox(QFormLayout *layout, NamedConfig<bool> &config);

    QList<ColorSetting> m_colorSettings;
    QList<CheckboxSetting> m_checkboxSettings;
};
