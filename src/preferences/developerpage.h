#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "../global/macros.h"
#include "ui_developerpage.h"

#include <QWidget>
#include <QPointer> // For QPointer for safe signal handling with lambdas
#include <memory> // For std::unique_ptr
// It's better to include the full header if we are using std::unique_ptr<Configuration>
// and calling methods on it, rather than just forward declaring.
#include "../../src/configuration/configuration.h"


class QCheckBox;
class QLineEdit;
class QSpinBox;
class QPushButton;
class QMetaProperty;
// Configuration is now included above
// class Configuration;

class NODISCARD_QOBJECT DeveloperPage final : public QWidget
{
    Q_OBJECT

signals:
    void sig_graphicsSettingsChanged();

public:
    explicit DeveloperPage(QWidget *parent = nullptr);
    ~DeveloperPage() final;

public slots:
    void slot_loadConfig();

private Q_SLOTS:
    void onResetToDefaultTriggered();

private:
    void populatePage();
    void filterSettings(const QString &text);

    // Helper methods for creating editors
    QCheckBox* createBoolEditor(Configuration &config, const QMetaProperty &property);
    QLineEdit* createStringEditor(Configuration &config, const QMetaProperty &property);
    QSpinBox* createIntEditor(Configuration &config, const QMetaProperty &property);
    QPushButton* createColorEditor(Configuration &config, const QMetaProperty &property, const QString& typeName);


    Ui::DeveloperPage *const ui;
    QList<QWidget*> m_settingLabels;
    QList<QWidget*> m_settingWidgets;
    std::unique_ptr<Configuration> m_defaultConfig;
    QString m_contextMenuPropertyName;
};
