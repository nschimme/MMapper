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
#include <QMenu> // Required for QMenu
#include <QMetaProperty>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QStringList>

#include "../configuration/configuration.h"
#include "../global/Color.h"

// TODO: Maintainability: The current mechanism of using string lists (e.g., knownGraphicsBoolPropertyNames)
// to determine if a property change should emit sig_graphicsSettingsChanged is not ideal, as it can
// become outdated if property names in Configuration.h change or new graphics-related properties are added.
// A more robust solution would involve a callback or observer pattern integrated more deeply into
// the Configuration class's substructs (e.g., CanvasSettings, ColorSettings using ChangeMonitor)
// or individual properties (e.g., XNamedColor exposing change signals).
// This would allow DeveloperPage to subscribe to these notifications directly rather than inferring based on name/type.
// This change is significant and deferred for future improvement. For now, these lists must be kept
// manually synchronized with graphics-affecting properties in Configuration.

const QStringList knownGraphicsBoolPropertyNames = {
    "drawUpperLayersTextured", "drawDoorNames", "trilinearFiltering", "softwareOpenGL",
    "showMissingMapId", "showUnsavedChanges", "showUnmappedExits",
    "MMAPPER_3D", "MMAPPER_AUTO_TILT", "MMAPPER_GL_PERFSTATS"
};
const QStringList knownGraphicsIntPropertyNames = { "antialiasingSamples", "fov" };
const QStringList knownGraphicsStringPropertyNames = { "resourcesDirectory" };


DeveloperPage::DeveloperPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DeveloperPage)
    , m_defaultConfig(std::make_unique<Configuration>(getConfig())) // Initialize with a copy of current config
{
    ui->setupUi(this);
    connect(ui->searchLineEdit, &QLineEdit::textChanged, this, &DeveloperPage::filterSettings);

    // Reset the copied configuration to its default state.
    // This assumes Configuration::reset() loads pristine defaults if no QSettings exist or are ignored.
    // For this to work robustly, Configuration::reset() should ideally load compiled-in defaults,
    // not just re-read from QSettings which might already be user-modified.
    // We proceed with the assumption it leads to a representation of defaults.
    if (m_defaultConfig) {
        m_defaultConfig->reset();
    }
}

DeveloperPage::~DeveloperPage()
{
    delete ui;
}

void DeveloperPage::slot_loadConfig()
{
    // Re-initialize m_defaultConfig when config is loaded/reloaded to ensure it's fresh
    // This is important if the main config is reloaded from disk, defaults might change based on version
    m_defaultConfig = std::make_unique<Configuration>(getConfig());
    if (m_defaultConfig) {
        m_defaultConfig->reset();
    }
    populatePage();
}

// Helper methods for creating editors
QCheckBox* DeveloperPage::createBoolEditor(Configuration &config, const QMetaProperty &property) {
    QCheckBox *checkBox = new QCheckBox(this);
    checkBox->setChecked(property.read(&config).toBool());
    checkBox->setContextMenuPolicy(Qt::CustomContextMenu);
    checkBox->setProperty("propertyName", QLatin1String(property.name()));
    connect(checkBox, &QWidget::customContextMenuRequested, this, [this, checkBox](const QPoint &pos) {
        m_contextMenuPropertyName = checkBox->property("propertyName").toString();
        QMenu contextMenu(this);
        QAction *resetAction = contextMenu.addAction("Reset to Default");
        connect(resetAction, &QAction::triggered, this, &DeveloperPage::onResetToDefaultTriggered);
        contextMenu.exec(checkBox->mapToGlobal(pos));
    });
    connect(checkBox, &QCheckBox::toggled, this, [this, &config, property](bool checked) {
        bool success = property.write(&config, checked);
        if (success) {
            QString propertyName = QLatin1String(property.name());
            if (knownGraphicsBoolPropertyNames.contains(propertyName)) {
                emit sig_graphicsSettingsChanged();
            }
        }
    });
    return checkBox;
}

QLineEdit* DeveloperPage::createStringEditor(Configuration &config, const QMetaProperty &property) {
    QLineEdit *lineEdit = new QLineEdit(this);
    lineEdit->setText(property.read(&config).toString());
    lineEdit->setContextMenuPolicy(Qt::CustomContextMenu);
    lineEdit->setProperty("propertyName", QLatin1String(property.name()));
    connect(lineEdit, &QWidget::customContextMenuRequested, this, [this, lineEdit](const QPoint &pos) {
        m_contextMenuPropertyName = lineEdit->property("propertyName").toString();
        QMenu contextMenu(this);
        QAction *resetAction = contextMenu.addAction("Reset to Default");
        connect(resetAction, &QAction::triggered, this, &DeveloperPage::onResetToDefaultTriggered);
        contextMenu.exec(lineEdit->mapToGlobal(pos));
    });
    connect(lineEdit, &QLineEdit::textEdited, this, [this, &config, property](const QString &text) {
        bool success = property.write(&config, text);
        if (success) {
            QString propertyName = QLatin1String(property.name());
            if (knownGraphicsStringPropertyNames.contains(propertyName)) {
                emit sig_graphicsSettingsChanged();
            }
        }
    });
    return lineEdit;
}

QSpinBox* DeveloperPage::createIntEditor(Configuration &config, const QMetaProperty &property) {
    QSpinBox *spinBox = new QSpinBox(this);
    spinBox->setRange(-2147483647, 2147483647);
    spinBox->setValue(property.read(&config).toInt());
    spinBox->setContextMenuPolicy(Qt::CustomContextMenu);
    spinBox->setProperty("propertyName", QLatin1String(property.name()));
    connect(spinBox, &QWidget::customContextMenuRequested, this, [this, spinBox](const QPoint &pos) {
        m_contextMenuPropertyName = spinBox->property("propertyName").toString();
        QMenu contextMenu(this);
        QAction *resetAction = contextMenu.addAction("Reset to Default");
        connect(resetAction, &QAction::triggered, this, &DeveloperPage::onResetToDefaultTriggered);
        contextMenu.exec(spinBox->mapToGlobal(pos));
    });
    connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, &config, property](int value) {
        bool success = property.write(&config, value);
        if (success) {
            QString propertyName = QLatin1String(property.name());
            if (knownGraphicsIntPropertyNames.contains(propertyName)) {
                emit sig_graphicsSettingsChanged();
            }
        }
    });
    return spinBox;
}

QPushButton* DeveloperPage::createColorEditor(Configuration &config, const QMetaProperty &property, const QString& typeName) {
    QColor initialQColor;
    bool isXColor = (typeName == "XColor");

    if (isXColor) {
        initialQColor = property.read(&config).value<XColor>().getColor();
    } else {
        initialQColor = property.read(&config).value<QColor>();
    }

    QPushButton *button = new QPushButton(initialQColor.name(), this);
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
    connect(button, &QPushButton::clicked, this, [this, &config, property, buttonPtr, isXColor]() {
        if (!buttonPtr) return;
        QColor currentColor;
        if (isXColor) {
            currentColor = property.read(&config).value<XColor>().getColor();
        } else {
            currentColor = property.read(&config).value<QColor>();
        }
        QColor color = QColorDialog::getColor(currentColor, this, "Select Color");
        if (color.isValid()) {
            bool success;
            if (isXColor) {
                success = property.write(&config, QVariant::fromValue(XColor(color)));
            } else {
                success = property.write(&config, color);
            }
            if (success) {
                buttonPtr->setText(color.name());
                buttonPtr->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; border: 1px solid black; padding: 2px; text-align: left; }")
                                       .arg(color.name())
                                       .arg(color.lightness() < 128 ? "white" : "black"));
                emit sig_graphicsSettingsChanged();
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

    Configuration &liveConfig = setConfig(); // Get current live configuration
    const QMetaObject *liveMetaObject = liveConfig.staticMetaObject;
    int propertyIndex = liveMetaObject->indexOfProperty(m_contextMenuPropertyName.toUtf8().constData());

    if (propertyIndex == -1) {
        qWarning() << "DeveloperPage: Could not find property" << m_contextMenuPropertyName << "in live config for reset.";
        m_contextMenuPropertyName.clear();
        return;
    }

    QMetaProperty property = liveMetaObject->property(propertyIndex);
    QVariant defaultValue;

    // Read the default value from our m_defaultConfig instance
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

    bool success = property.write(&liveConfig, defaultValue);
    if (success) {
        // Update the UI widget
        QWidget* editorWidgetToUpdate = nullptr;
        for (int i = 0; i < m_settingWidgets.count(); ++i) {
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
        } else {
            // If not found, maybe the page was repopulated? For safety, repopulate.
            // This is a bit heavy-handed.
            populatePage();
        }

        // Emit signals if necessary
        QString propNameStr = QLatin1String(property.name());
        bool isGraphicsProp = false;
        if (property.typeId() == QMetaType::Bool && knownGraphicsBoolPropertyNames.contains(propNameStr)) isGraphicsProp = true;
        else if (property.typeId() == QMetaType::QString && knownGraphicsStringPropertyNames.contains(propNameStr)) isGraphicsProp = true;
        else if (property.typeId() == QMetaType::Int && knownGraphicsIntPropertyNames.contains(propNameStr)) isGraphicsProp = true;
        else if (QLatin1String(property.typeName()) == QLatin1String("XColor") || QLatin1String(property.typeName()) == QLatin1String("QColor")) isGraphicsProp = true;

        if (isGraphicsProp) {
            emit sig_graphicsSettingsChanged();
        }
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

    Configuration &config = setConfig();
    const QMetaObject *metaObject = config.staticMetaObject;

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setHorizontalSpacing(10);
    formLayout->setVerticalSpacing(5);

    for (int i = metaObject->userProperty().propertyOffset(); i < metaObject->propertyCount(); ++i) {
        QMetaProperty property = metaObject->property(i);
        // const char *name = property.name(); // Not used directly if propertyName comes from widget
        QVariant qVariantValue = property.read(&config);
        QWidget *editorWidget = nullptr;
        QLabel *nameLabel = new QLabel(QString::fromUtf8(property.name()) + ":", this); // Use property.name() for label

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
