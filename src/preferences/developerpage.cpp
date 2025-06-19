// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "developerpage.h"
#include "ui_developerpage.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDebug>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMetaProperty>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QStringList>

#include "../configuration/configuration.h"
#include "../global/Color.h"
#include "../global/NamedColors.h" // Required for XNamedColor::registerGlobalChangeCallback

// TODO: Maintainability: The current mechanism of using string lists (e.g., knownGraphicsBoolPropertyNames)
// to determine if a property change should emit sig_graphicsSettingsChanged is not ideal, as it can
// become outdated if property names in Configuration.h change or new graphics-related properties are added.
// A more robust solution would involve a callback or observer pattern integrated more deeply into
// the Configuration class's substructs (e.g., CanvasSettings, ColorSettings using ChangeMonitor)
// or individual properties (e.g., XNamedColor exposing change signals).
// This would allow DeveloperPage to subscribe to these notifications directly rather than inferring based on name/type.
// This change is significant and deferred for future improvement. For now, these lists must be kept
// manually synchronized with graphics-affecting properties in Configuration.

// const QStringList knownGraphicsBoolPropertyNames = { ... }; // REMOVED
// const QStringList knownGraphicsIntPropertyNames = { ... }; // REMOVED
// const QStringList knownGraphicsStringPropertyNames = { ... }; // REMOVED

// For NamedConfig<T> properties that are graphics-related.
// This list is now only used if a NamedConfig<T> is NOT part of a struct
// that DeveloperPage already listens to via a more general ChangeMonitor (like CanvasSettings).
// Since all listed NamedConfigs are within CanvasSettings or CanvasSettings::Advanced,
// and DeveloperPage now listens to config.canvas.showMissingMapId etc. directly,
// this list is now empty as all relevant NamedConfig<T> instances for graphics have direct listeners.
const QStringList graphicsNamedConfigPropertyNames = {}; // Now definitively empty


DeveloperPage::DeveloperPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DeveloperPage)
    , m_defaultConfig(std::make_unique<Configuration>(getConfig()))
{
    ui->setupUi(this);
    connect(ui->searchLineEdit, &QLineEdit::textChanged, this, &DeveloperPage::filterSettings);

    if (m_defaultConfig) {
        m_defaultConfig->reset();
    }

    // Initial registration in constructor. slot_loadConfig will also handle this.
    registerChangeMonitors();
}

DeveloperPage::~DeveloperPage()
{
    m_configLifetime.disconnectAll(); // Ensure all connections are severed
    delete ui;
}

void DeveloperPage::registerChangeMonitors() {
    m_configLifetime.disconnectAll(); // Clear previous connections before re-registering

    Configuration &config = getConfig(); // Use getConfig for registering listeners

    // Register callback with CanvasSettings
    config.canvas.registerChangeCallback(m_configLifetime, [this]() {
        this->slot_loadConfig(); // Refresh DeveloperPage's UI
        emit sig_graphicsSettingsChanged();
    });

    // Register callback with global XNamedColor changes
    XNamedColor::registerGlobalChangeCallback(m_configLifetime, [this]() {
        this->slot_loadConfig(); // Refresh DeveloperPage's UI
        emit sig_graphicsSettingsChanged();
    });

    // Register callback with GroupManagerSettings
    config.groupManager.registerChangeCallback(m_configLifetime, [this]() {
        // This callback is for changes in GroupManagerSettings.
        // DeveloperPage currently does not emit a specific signal itself in response to these.
        // This connection is for completeness. If DeveloperPage needed to react,
        // it might emit a general "settingsChanged" or a specific group settings signal.
        // For now, this can be a no-op or a qDebug for tracing.
        // qDebug() << "DeveloperPage: Detected a change in GroupManagerSettings.";
    });

    // Register callback with IntegratedMudClientSettings
    config.integratedClient.registerChangeCallback(m_configLifetime, [this]() {
        // This callback is for changes in IntegratedMudClientSettings.
        // DeveloperPage currently does not have specific UI reactions to these
        // beyond what a full slot_loadConfig() would provide if the dialog is re-shown
        // or settings are reset.
        // qDebug() << "DeveloperPage: Detected a change in IntegratedMudClientSettings.";
    });

    // For NamedConfig<T> members in config.canvas:
    config.canvas.showMissingMapId.registerChangeCallback(m_configLifetime, [this]() {
        this->slot_loadConfig(); // Refresh DeveloperPage's UI
        emit sig_graphicsSettingsChanged();
    });
    config.canvas.showUnsavedChanges.registerChangeCallback(m_configLifetime, [this]() {
        this->slot_loadConfig(); // Refresh DeveloperPage's UI
        emit sig_graphicsSettingsChanged();
    });
    config.canvas.showUnmappedExits.registerChangeCallback(m_configLifetime, [this]() {
        this->slot_loadConfig(); // Refresh DeveloperPage's UI
        emit sig_graphicsSettingsChanged();
    });

    // For NamedConfig<T> members in config.canvas.advanced:
    config.canvas.advanced.use3D.registerChangeCallback(m_configLifetime, [this]() {
        this->slot_loadConfig(); // Refresh DeveloperPage's UI
        emit sig_graphicsSettingsChanged();
    });
    config.canvas.advanced.autoTilt.registerChangeCallback(m_configLifetime, [this]() {
        this->slot_loadConfig(); // Refresh DeveloperPage's UI
        emit sig_graphicsSettingsChanged();
    });
    config.canvas.advanced.printPerfStats.registerChangeCallback(m_configLifetime, [this]() {
        // This one might not be strictly "graphics settings changed" in a visual sense,
        // but it's grouped with other canvas/advanced settings.
        this->slot_loadConfig(); // Refresh DeveloperPage's UI (in case it affects some debug overlay)
        emit sig_graphicsSettingsChanged();
    });
}

void DeveloperPage::slot_loadConfig()
{
    m_defaultConfig = std::make_unique<Configuration>(getConfig());
    if (m_defaultConfig) {
        m_defaultConfig->reset();
    }

    registerChangeMonitors(); // Re-register monitors, ensuring old connections are cleared

    populatePage();
}

// Helper methods for creating editors
QCheckBox* DeveloperPage::createBoolEditor(Configuration &config_ref_for_read, const QMetaProperty &property) {
    QCheckBox *checkBox = new QCheckBox(this);
    checkBox->setChecked(property.read(&config_ref_for_read).toBool());
    checkBox->setContextMenuPolicy(Qt::CustomContextMenu);
    checkBox->setProperty("propertyName", QLatin1String(property.name()));
    connect(checkBox, &QWidget::customContextMenuRequested, this, [this, checkBox](const QPoint &pos) {
        m_contextMenuPropertyName = checkBox->property("propertyName").toString();
        QMenu contextMenu(this);
        QAction *resetAction = contextMenu.addAction("Reset to Default");
        connect(resetAction, &QAction::triggered, this, &DeveloperPage::onResetToDefaultTriggered);
        contextMenu.exec(checkBox->mapToGlobal(pos));
    });

    connect(checkBox, &QCheckBox::toggled, this, [this, property](bool checked) {
        QString propertyNameStr = QLatin1String(property.name());
        bool success = false;
        // Use setters for refactored properties in CanvasSettings, GroupManagerSettings, or IntegratedMudClientSettings
        if (propertyNameStr == "drawDoorNames") {
            setConfig().canvas.setDrawDoorNames(checked); success = true;
        } else if (propertyNameStr == "drawUpperLayersTextured") {
            setConfig().canvas.setDrawUpperLayersTextured(checked); success = true;
        } else if (propertyNameStr == "trilinearFiltering") {
            setConfig().canvas.setTrilinearFiltering(checked); success = true;
        } else if (propertyNameStr == "softwareOpenGL") {
            setConfig().canvas.setSoftwareOpenGL(checked); success = true;
        } else if (propertyNameStr == "npcColorOverride") {
            setConfig().groupManager.setNpcColorOverride(checked); success = true;
        } else if (propertyNameStr == "npcSortBottom") {
            setConfig().groupManager.setNpcSortBottom(checked); success = true;
        } else if (propertyNameStr == "npcHide") {
            setConfig().groupManager.setNpcHide(checked); success = true;
        } else if (propertyNameStr == "clearInputOnEnter") {
            setConfig().integratedClient.setClearInputOnEnter(checked); success = true;
        } else if (propertyNameStr == "autoResizeTerminal") {
            setConfig().integratedClient.setAutoResizeTerminal(checked); success = true;
        } else {
            // For other bool properties (incl. NamedConfig<bool> not directly listened to), use property.write
            success = property.write(&setConfig(), checked);
            // Direct emission for NamedConfigs is removed here.
            // If a NamedConfig is graphics-related and NOT directly listened to above
            // (e.g. not part of config.canvas or config.canvas.advanced),
            // it would need to be added to the direct listeners in registerChangeMonitors,
            // or this DeveloperPage would need to listen to its parent struct's monitor if that exists.
            // The graphicsNamedConfigPropertyNames list is now empty, so this direct emit path is disabled.
            // if (success && graphicsNamedConfigPropertyNames.contains(propertyNameStr)) {
            //     emit sig_graphicsSettingsChanged();
            // }
        }
        // No direct emit for properties covered by a ChangeMonitor DeveloperPage listens to.
    });
    return checkBox;
}

QLineEdit* DeveloperPage::createStringEditor(Configuration &config_ref_for_read, const QMetaProperty &property) {
    QLineEdit *lineEdit = new QLineEdit(this);
    lineEdit->setText(property.read(&config_ref_for_read).toString());
    lineEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    lineEdit->setProperty("propertyName", QLatin1String(property.name()));
    connect(lineEdit, &QWidget::customContextMenuRequested, this, [this, lineEdit](const QPoint &pos) {
        m_contextMenuPropertyName = lineEdit->property("propertyName").toString();
        QMenu contextMenu(this);
        QAction *resetAction = contextMenu.addAction("Reset to Default");
        connect(resetAction, &QAction::triggered, this, &DeveloperPage::onResetToDefaultTriggered);
        contextMenu.exec(lineEdit->mapToGlobal(pos));
    });
    connect(lineEdit, &QLineEdit::textEdited, this, [this, property](const QString &text) {
        QString propertyNameStr = QLatin1String(property.name());
        bool success = false;
        if (propertyNameStr == "resourcesDirectory") {
            setConfig().canvas.setResourcesDirectory(text); success = true;
        } else if (propertyNameStr == "font") { // From IntegratedMudClientSettings
            setConfig().integratedClient.setFont(text); success = true;
        } else {
            success = property.write(&setConfig(), text);
            // Direct emission for NamedConfigs is removed here.
            // if (success && graphicsNamedConfigPropertyNames.contains(propertyNameStr)) {
            //     emit sig_graphicsSettingsChanged();
            // }
        }
        // No direct emit for refactored props here
    });
    return lineEdit;
}

QSpinBox* DeveloperPage::createIntEditor(Configuration &config_ref_for_read, const QMetaProperty &property) {
    QSpinBox *spinBox = new QSpinBox(this);
    spinBox->setRange(-2147483647, 2147483647);
    spinBox->setValue(property.read(&config_ref_for_read).toInt());
    spinBox->setContextMenuPolicy(Qt::CustomContextMenu);
    spinBox->setProperty("propertyName", QLatin1String(property.name()));
    connect(spinBox, &QWidget::customContextMenuRequested, this, [this, spinBox](const QPoint &pos) {
        m_contextMenuPropertyName = spinBox->property("propertyName").toString();
        QMenu contextMenu(this);
        QAction *resetAction = contextMenu.addAction("Reset to Default");
        connect(resetAction, &QAction::triggered, this, &DeveloperPage::onResetToDefaultTriggered);
        contextMenu.exec(spinBox->mapToGlobal(pos));
    });
    connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, property](int value) {
        QString propertyNameStr = QLatin1String(property.name());
        bool success = false;
        if (propertyNameStr == "antialiasingSamples") {
            setConfig().canvas.setAntialiasingSamples(value); success = true;
        } else if (propertyNameStr == "columns") { // From IntegratedMudClientSettings
            setConfig().integratedClient.setColumns(value); success = true;
        } else if (propertyNameStr == "rows") {
            setConfig().integratedClient.setRows(value); success = true;
        } else if (propertyNameStr == "linesOfScrollback") {
            setConfig().integratedClient.setLinesOfScrollback(value); success = true;
        } else if (propertyNameStr == "linesOfInputHistory") {
            setConfig().integratedClient.setLinesOfInputHistory(value); success = true;
        } else if (propertyNameStr == "tabCompletionDictionarySize") {
            setConfig().integratedClient.setTabCompletionDictionarySize(value); success = true;
        } else if (propertyNameStr == "linesOfPeekPreview") {
            setConfig().integratedClient.setLinesOfPeekPreview(value); success = true;
        } else {
            success = property.write(&setConfig(), value);
            // Direct emission for NamedConfigs is removed here.
            // if (success && graphicsNamedConfigPropertyNames.contains(propertyNameStr)) {
            //     emit sig_graphicsSettingsChanged();
            // }
        }
        // No direct emit for refactored props here
    });
    return spinBox;
}

QPushButton* DeveloperPage::createColorEditor(Configuration &config_ref_for_read, const QMetaProperty &property, const QString& typeName) {
    QColor initialQColor;
    bool isXColor = (typeName == "XColor");

    if (isXColor) {
        initialQColor = property.read(&config_ref_for_read).value<XColor>().getColor();
    } else {
        initialQColor = property.read(&config_ref_for_read).value<QColor>();
    }

    QPushButton *button = new QPushButton(initialQColor.name(), this);
    // ... (stylesheet and context menu setup as before) ...
    button->setFlat(true);
    button->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; border: 1px solid black; padding: 2px; text-align: left; }")
                          .arg(initialQColor.name())
                          .arg(initialQColor.lightness() < 128 ? "white" : "black"));
    button->setContextMenuPolicy(Qt::CustomContextMenu);
    button->setProperty("propertyName", QLatin1String(property.name()));
    connect(button, &QWidget::customContextMenuRequested, this, [this, button](const QPoint &pos) {
        m_contextMenuPropertyName = button->property("propertyName").toString();
        QMenu contextMenu(this);
        QAction *resetAction = contextMenu.addAction("Reset to Default");
        connect(resetAction, &QAction::triggered, this, &DeveloperPage::onResetToDefaultTriggered);
        contextMenu.exec(button->mapToGlobal(pos));
    });

    QPointer<QPushButton> buttonPtr(button);
    connect(button, &QPushButton::clicked, this, [this, &config_ref_for_read, property, buttonPtr, isXColor]() { // Pass config_ref_for_read if needed by property.read inside
        if (!buttonPtr) return;
        QColor currentColor;
        // It's safer to re-read from the config object that was passed for initial read,
        // or always use getConfig() if we assume it's the single source of truth for current state.
        // For consistency with other helpers, let's assume property.read(&getConfig()) is okay for current value.
        if (isXColor) {
            currentColor = property.read(&getConfig()).value<XColor>().getColor();
        } else {
            currentColor = property.read(&getConfig()).value<QColor>();
        }
        QColor color = QColorDialog::getColor(currentColor, this, "Select Color");
        if (color.isValid()) {
            bool success;
            QString propertyNameStr = QLatin1String(property.name());
            // Write to the live config using setConfig()
            if (isXColor) {
                success = property.write(&setConfig(), QVariant::fromValue(XColor(color)));
            } else if (typeName == "QColor") {
                if (propertyNameStr == "color") { // GroupManagerSettings
                    setConfig().groupManager.setColor(color); success = true;
                } else if (propertyNameStr == "npcColor") { // GroupManagerSettings
                    setConfig().groupManager.setNpcColor(color); success = true;
                } else if (propertyNameStr == "foregroundColor") { // IntegratedMudClientSettings
                    setConfig().integratedClient.setForegroundColor(color); success = true;
                } else if (propertyNameStr == "backgroundColor") { // IntegratedMudClientSettings
                    setConfig().integratedClient.setBackgroundColor(color); success = true;
                } else {
                    success = property.write(&setConfig(), color);
                }
            } else {
                 success = false;
            }

            if (success) {
                buttonPtr->setText(color.name());
                buttonPtr->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; border: 1px solid black; padding: 2px; text-align: left; }")
                                       .arg(color.name())
                                       .arg(color.lightness() < 128 ? "white" : "black"));
                // No direct emit here for XColor; global XNamedColor monitor handles it.
                // If this QColor is not an XColor and IS graphics related AND not in CanvasSettings, it might need a direct emit.
                // For now, assuming XColor covers the main graphics color cases handled by global monitor.
                // QColor properties in CanvasSettings (if any) would be covered by CanvasSettings monitor if refactored.
                if (!isXColor && QLatin1String(property.typeName()) == QLatin1String("QColor") /* and is graphics related, not in CanvasSettings */) {
                     // emit sig_graphicsSettingsChanged(); // Potentially, if it's a non-XColor, non-CanvasSettings graphics color.
                }
            }
        }
    });
    return button;
}

void DeveloperPage::onResetToDefaultTriggered()
{
    if (m_contextMenuPropertyName.isEmpty() || !m_defaultConfig) {
        qWarning() << "DeveloperPage: Context menu property name or default config not set.";
        return;
    }

    Configuration &liveConfig = setConfig();
    const QMetaObject *liveMetaObject = liveConfig.staticMetaObject;
    int propertyIndex = liveMetaObject->indexOfProperty(m_contextMenuPropertyName.toUtf8().constData());

    if (propertyIndex == -1) {
        qWarning() << "DeveloperPage: Could not find property" << m_contextMenuPropertyName << "in live config for reset.";
        m_contextMenuPropertyName.clear();
        return;
    }

    QMetaProperty property = liveMetaObject->property(propertyIndex);
    QVariant defaultValue;

    const QMetaObject *defaultMetaObject = m_defaultConfig->staticMetaObject;
    int defaultPropertyIndex = defaultMetaObject->indexOfProperty(m_contextMenuPropertyName.toUtf8().constData());
    if (defaultPropertyIndex != -1) {
        QMetaProperty defaultMetaProperty = defaultMetaObject->property(defaultPropertyIndex);
        defaultValue = defaultMetaProperty.read(m_defaultConfig.get());
    } else {
        qWarning() << "DeveloperPage: Could not find property" << m_contextMenuPropertyName << "in default config instance.";
        m_contextMenuPropertyName.clear();
        return;
    }

    if (!defaultValue.isValid()) {
         qWarning() << "DeveloperPage: Default value for" << m_contextMenuPropertyName << "is invalid or could not be read.";
         m_contextMenuPropertyName.clear();
         return;
    }

    // Use setters for refactored CanvasSettings or GroupManagerSettings properties, otherwise property.write()
    QString propNameStr = QLatin1String(property.name());
    bool success = false;
    const QString typeName = QLatin1String(property.typeName());

    if (propNameStr == "drawDoorNames") {
        liveConfig.canvas.setDrawDoorNames(defaultValue.toBool()); success = true;
    } else if (propNameStr == "drawUpperLayersTextured") {
        liveConfig.canvas.setDrawUpperLayersTextured(defaultValue.toBool()); success = true;
    } else if (propNameStr == "trilinearFiltering") {
        liveConfig.canvas.setTrilinearFiltering(defaultValue.toBool()); success = true;
    } else if (propNameStr == "softwareOpenGL") {
        liveConfig.canvas.setSoftwareOpenGL(defaultValue.toBool()); success = true;
    } else if (propNameStr == "resourcesDirectory") {
        liveConfig.canvas.setResourcesDirectory(defaultValue.toString()); success = true;
    } else if (propNameStr == "antialiasingSamples") {
        liveConfig.canvas.setAntialiasingSamples(defaultValue.toInt()); success = true;
    } else if (propNameStr == "npcColorOverride") {
        liveConfig.groupManager.setNpcColorOverride(defaultValue.toBool()); success = true;
    } else if (propNameStr == "npcSortBottom") {
        liveConfig.groupManager.setNpcSortBottom(defaultValue.toBool()); success = true;
    } else if (propNameStr == "npcHide") {
        liveConfig.groupManager.setNpcHide(defaultValue.toBool()); success = true;
    } else if (typeName == "QColor" && propNameStr == "color") {
        liveConfig.groupManager.setColor(defaultValue.value<QColor>()); success = true;
    } else if (typeName == "QColor" && propNameStr == "npcColor") {
        liveConfig.groupManager.setNpcColor(defaultValue.value<QColor>()); success = true;
    } else if (propNameStr == "font") {
        liveConfig.integratedClient.setFont(defaultValue.toString()); success = true;
    } else if (typeName == "QColor" && propNameStr == "foregroundColor") {
        liveConfig.integratedClient.setForegroundColor(defaultValue.value<QColor>()); success = true;
    } else if (typeName == "QColor" && propNameStr == "backgroundColor") {
        liveConfig.integratedClient.setBackgroundColor(defaultValue.value<QColor>()); success = true;
    } else if (propNameStr == "columns") {
        liveConfig.integratedClient.setColumns(defaultValue.toInt()); success = true;
    } else if (propNameStr == "rows") {
        liveConfig.integratedClient.setRows(defaultValue.toInt()); success = true;
    } else if (propNameStr == "linesOfScrollback") {
        liveConfig.integratedClient.setLinesOfScrollback(defaultValue.toInt()); success = true;
    } else if (propNameStr == "linesOfInputHistory") {
        liveConfig.integratedClient.setLinesOfInputHistory(defaultValue.toInt()); success = true;
    } else if (propNameStr == "tabCompletionDictionarySize") {
        liveConfig.integratedClient.setTabCompletionDictionarySize(defaultValue.toInt()); success = true;
    } else if (propNameStr == "clearInputOnEnter") {
        liveConfig.integratedClient.setClearInputOnEnter(defaultValue.toBool()); success = true;
    } else if (propNameStr == "autoResizeTerminal") {
        liveConfig.integratedClient.setAutoResizeTerminal(defaultValue.toBool()); success = true;
    } else if (propNameStr == "linesOfPeekPreview") {
        liveConfig.integratedClient.setLinesOfPeekPreview(defaultValue.toInt()); success = true;
    } else {
        success = property.write(&liveConfig, defaultValue);
    }

    if (success) {
        QWidget* editorWidgetToUpdate = nullptr;
        // ... (UI update logic as before)
        for (int i = 0; i < m_settingWidgets.count(); ++i) { // Renamed from m_settingLabels
            if (m_settingWidgets.at(i)->property("propertyName").toString() == m_contextMenuPropertyName) {
                editorWidgetToUpdate = m_settingWidgets.at(i);
                break;
            }
        }
        if (editorWidgetToUpdate) {
             if (QCheckBox* cb = qobject_cast<QCheckBox*>(editorWidgetToUpdate)) {
                cb->setChecked(defaultValue.toBool());
            } else if (QLineEdit* le = qobject_cast<QLineEdit*>(editorWidgetToUpdate)) {
                le->setText(defaultValue.toString());
            } else if (QSpinBox* sb = qobject_cast<QSpinBox*>(editorWidgetToUpdate)) {
                sb->setValue(defaultValue.toInt());
            } else if (QPushButton* pb = qobject_cast<QPushButton*>(editorWidgetToUpdate)) {
                QColor c;
                if (defaultValue.canConvert<XColor>()) c = defaultValue.value<XColor>().getColor();
                else if (defaultValue.canConvert<QColor>()) c = defaultValue.value<QColor>();

                if (c.isValid()) {
                    pb->setText(c.name());
                    pb->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; border: 1px solid black; padding: 2px; text-align: left; }")
                                      .arg(c.name())
                                      .arg(c.lightness() < 128 ? "white" : "black"));
                }
            }
        }  else {
            populatePage();
        }

        // Signal emission is now entirely handled by the ChangeMonitors that DeveloperPage listens to.
        // When property.write() or a direct setter (for refactored properties) was called above,
        // it triggered the respective ChangeMonitor. DeveloperPage's registered callback for that
        // monitor then calls slot_loadConfig() and emits sig_graphicsSettingsChanged().
        // Therefore, no direct emit is needed in this function anymore.
    } else {
        qWarning() << "DeveloperPage: Failed to write default value for" << m_contextMenuPropertyName;
    }
    m_contextMenuPropertyName.clear();
}


void DeveloperPage::populatePage()
{
    m_settingLabels.clear();
    m_settingWidgets.clear();

    QLayoutItem* item;
    while ((item = ui->settingsLayout->takeAt(0)) != nullptr) {
        if (item->layout()) {
            QLayout* layout = item->layout();
            QLayoutItem* innerItem;
            while((innerItem = layout->takeAt(0)) != nullptr) {
                if (innerItem->widget()) {
                    delete innerItem->widget();
                }
                delete innerItem;
            }
            delete layout;
        } else if (item->widget()) {
            delete item->widget();
        }
        delete item;
    }

    Configuration &config = getConfig(); // Use getConfig for initial read
    const QMetaObject *metaObject = config.staticMetaObject;

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setHorizontalSpacing(10);
    formLayout->setVerticalSpacing(5);

    for (int i = metaObject->userProperty().propertyOffset(); i < metaObject->propertyCount(); ++i) {
        QMetaProperty property = metaObject->property(i);
        QVariant qVariantValue = property.read(&config); // Read from config
        QWidget *editorWidget = nullptr;
        QLabel *nameLabel = new QLabel(QString::fromUtf8(property.name()) + ":", this);

        const QString typeName = QString::fromUtf8(property.typeName());

        if (qVariantValue.typeId() == QMetaType::Bool) {
            editorWidget = createBoolEditor(config, property);
        } else if (qVariantValue.typeId() == QMetaType::QString) {
            editorWidget = createStringEditor(config, property);
        } else if (qVariantValue.typeId() == QMetaType::Int || qVariantValue.typeId() == QMetaType::UInt || qVariantValue.typeId() == QMetaType::LongLong || qVariantValue.typeId() == QMetaType::ULongLong) {
            editorWidget = createIntEditor(config, property);
        } else if (typeName == "QColor" || typeName == "XColor") {
            editorWidget = createColorEditor(config, property, typeName);
        }

        if (editorWidget) {
            formLayout->addRow(nameLabel, editorWidget);
            m_settingLabels.append(nameLabel);
            m_settingWidgets.append(editorWidget);
        } else {
            delete nameLabel;
        }
    }
    ui->settingsLayout->addLayout(formLayout);
}

void DeveloperPage::filterSettings(const QString &text)
{
    QString lowerText = text.toLower();
    QFormLayout *formLayout = nullptr;
    if (ui->settingsLayout->count() > 0 && ui->settingsLayout->itemAt(0)) {
        formLayout = qobject_cast<QFormLayout*>(ui->settingsLayout->itemAt(0)->layout());
    }

    for (int i = 0; i < m_settingLabels.count(); ++i) {
        QLabel *label = qobject_cast<QLabel*>(m_settingLabels.at(i));
        QWidget *widget = m_settingWidgets.at(i);

        if (label && widget) {
            bool match = lowerText.isEmpty() || label->text().toLower().contains(lowerText);

            if (formLayout) {
                int rowIndex = -1;
                for (int r = 0; r < formLayout->rowCount(); ++r) {
                    QLayoutItem *labelItem = formLayout->itemAt(r, QFormLayout::LabelRole);
                    if (labelItem && labelItem->widget() == label) {
                        rowIndex = r;
                        break;
                    }
                }
                if (rowIndex != -1) {
                     formLayout->setRowVisible(rowIndex, match);
                } else {
                    label->setVisible(match);
                    widget->setVisible(match);
                }
            } else {
                label->setVisible(match);
                widget->setVisible(match);
            }
        }
    }
}
