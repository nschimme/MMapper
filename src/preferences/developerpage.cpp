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

// For NamedConfig<T> properties that are graphics-related and not part of CanvasSettings' direct monitor
const QStringList graphicsNamedConfigPropertyNames = {
    "showMissingMapId", "showUnsavedChanges", "showUnmappedExits", // From CanvasSettings (are NamedConfig)
    "MMAPPER_3D", "MMAPPER_AUTO_TILT", "MMAPPER_GL_PERFSTATS"    // From CanvasSettings::Advanced (are NamedConfig)
};


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
        emit sig_graphicsSettingsChanged();
    });

    // Register callback with global XNamedColor changes
    XNamedColor::registerGlobalChangeCallback(m_configLifetime, [this]() {
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
        // Use setters for refactored CanvasSettings properties
        if (propertyNameStr == "drawDoorNames") {
            setConfig().canvas.setDrawDoorNames(checked); success = true;
        } else if (propertyNameStr == "drawUpperLayersTextured") {
            setConfig().canvas.setDrawUpperLayersTextured(checked); success = true;
        } else if (propertyNameStr == "trilinearFiltering") {
            setConfig().canvas.setTrilinearFiltering(checked); success = true;
        } else if (propertyNameStr == "softwareOpenGL") {
            setConfig().canvas.setSoftwareOpenGL(checked); success = true;
        } else {
            // For other bool properties (incl. NamedConfig<bool>), use property.write
            success = property.write(&setConfig(), checked);
            if (success && graphicsNamedConfigPropertyNames.contains(propertyNameStr)) {
                // Emit only for NamedConfigs known to be graphics-related and not covered by CanvasSettings monitor
                emit sig_graphicsSettingsChanged();
            }
        }
        // No direct emit for CanvasSettings props here - handled by its ChangeMonitor
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
        } else {
            success = property.write(&setConfig(), text);
             if (success && graphicsNamedConfigPropertyNames.contains(propertyNameStr)) { // Assuming some NamedConfig<QString> might exist
                 emit sig_graphicsSettingsChanged();
             }
        }
        // No direct emit for CanvasSettings props here
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
        } else {
            success = property.write(&setConfig(), value);
            if (success && graphicsNamedConfigPropertyNames.contains(propertyNameStr)) { // Assuming some NamedConfig<int> might exist
                 emit sig_graphicsSettingsChanged();
            }
        }
        // No direct emit for CanvasSettings props here
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
            // Write to the live config using setConfig()
            if (isXColor) {
                success = property.write(&setConfig(), QVariant::fromValue(XColor(color)));
            } else {
                // This assumes QColor properties are not part of CanvasSettings' direct ChangeMonitor items
                // If they were, they'd need a direct setter.
                success = property.write(&setConfig(), color);
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

    // Use setters for refactored CanvasSettings properties, otherwise property.write()
    QString propNameStr = QLatin1String(property.name());
    bool success = false;
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

        // Signal emission is now handled by ChangeMonitors for CanvasSettings plain members and XNamedColors.
        // For NamedConfig<T> properties, if they were reset via property.write(), we might need to emit here if they are graphics related.
        if (property.isUser() && QLatin1String(property.typeName()).startsWith("NamedConfig")) { // Heuristic for NamedConfig
            if (graphicsNamedConfigPropertyNames.contains(propNameStr)) {
                 emit sig_graphicsSettingsChanged();
            }
        }
        // No other direct emit here.
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
