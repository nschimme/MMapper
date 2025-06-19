// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "developerpage.h"
#include "ui_developerpage.h" // Make sure ui_developerpage.h is included

#include <QCheckBox>
#include <QColorDialog>
#include <QDebug> // For debugging output
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaProperty> // Required for QMetaProperty
#include <QPushButton>
#include <QScrollArea> // Required for QScrollArea
#include <QSpinBox>
#include <QVBoxLayout> // Required for QVBoxLayout

#include "../configuration/configuration.h" // Ensure this is included
#include "../global/Color.h"               // For XColor


DeveloperPage::DeveloperPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DeveloperPage)
{
    ui->setupUi(this);
    connect(ui->searchLineEdit, &QLineEdit::textChanged, this, &DeveloperPage::filterSettings);
    // populatePage(); // We'll call this in slot_loadConfig for now
}

DeveloperPage::~DeveloperPage()
{
    delete ui;
    // m_settingLabels and m_settingWidgets are lists of QWidget pointers.
    // The widgets themselves are parented to 'this' or other widgets
    // and will be deleted by Qt's parent-child mechanism.
    // If they were not parented or added to layouts that take ownership,
    // then qDeleteAll would be necessary here for m_settingLabels and m_settingWidgets.
}

void DeveloperPage::slot_loadConfig()
{
    // Clear any existing widgets (important for re-populating)
    // What was here before:
    // QLayoutItem* item;
    // while ((item = ui->settingsLayout->takeAt(0)) != nullptr) {
    //     delete item->widget(); // This would also delete widgets in m_settingWidgets if they are direct children
    //     delete item;
    // }
    // populatePage will handle clearing its own managed widgets and the layout
    populatePage();
}

void DeveloperPage::populatePage()
{
    // Clear previous dynamic widgets and lists
    // Note: Widgets in m_settingWidgets and m_settingLabels are deleted when their parent layout/widget is deleted.
    // Clearing the lists here just removes the pointers, not the objects if they are still in a layout.
    m_settingLabels.clear();
    m_settingWidgets.clear();

    QLayoutItem* item;
    // Safely remove and delete items from ui->settingsLayout
    while ((item = ui->settingsLayout->takeAt(0)) != nullptr) {
        if (item->layout()) { // If it's a layout (like QFormLayout)
            QLayout* layout = item->layout();
            QLayoutItem* innerItem;
            while((innerItem = layout->takeAt(0)) != nullptr) {
                if (innerItem->widget()) {
                    delete innerItem->widget(); // Delete widgets within the form layout
                }
                delete innerItem;
            }
            delete layout; // Delete the form layout itself
        } else if (item->widget()) {
            delete item->widget(); // Delete spacer or other direct widgets
        }
        delete item; // Delete the layout item itself
    }


    Configuration &config = setConfig(); // Get a non-const reference
    const QMetaObject *metaObject = config.staticMetaObject;

    QFormLayout *formLayout = new QFormLayout();
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setHorizontalSpacing(10);
    formLayout->setVerticalSpacing(5);

    for (int i = metaObject->userProperty().propertyOffset(); i < metaObject->propertyCount(); ++i) {
        QMetaProperty property = metaObject->property(i);
        const char *name = property.name();
        QVariant qVariantValue = property.read(&config);
        QWidget *editorWidget = nullptr;
        QLabel *nameLabel = new QLabel(QString::fromUtf8(name) + ":", this);

        // Skip properties that are not user-configurable or are complex objects for now
        // Or properties that don't have a C++ 'set' method usable by QMetaProperty::write
        // if (!property.isUser() || !property.hasStdCppSet()) {
        //     qDebug() << "Skipping property:" << name << "(isUser:" << property.isUser() << ", hasStdCppSet:" << property.hasStdCppSet() << ", type:" << property.typeName() << ")";
        //     delete nameLabel; // Clean up unused label
        //     continue;
        // }


        if (qVariantValue.typeId() == QMetaType::Bool) {
            QCheckBox *checkBox = new QCheckBox(this);
            checkBox->setChecked(qVariantValue.toBool());
            editorWidget = checkBox;
            connect(checkBox, &QCheckBox::toggled, this, [&config, property](bool checked) {
                // qDebug() << "Setting bool:" << property.name() << checked;
                bool success = property.write(&config, checked);
                // if (!success) qDebug() << "Failed to write bool property:" << property.name();
            });
        } else if (qVariantValue.typeId() == QMetaType::QString) {
            QLineEdit *lineEdit = new QLineEdit(this);
            lineEdit->setText(qVariantValue.toString());
            editorWidget = lineEdit;
            connect(lineEdit, &QLineEdit::textEdited, this, [&config, property](const QString &text) {
                // qDebug() << "Setting QString:" << property.name() << text;
                bool success = property.write(&config, text);
                // if (!success) qDebug() << "Failed to write QString property:" << property.name();
            });
        } else if (qVariantValue.typeId() == QMetaType::Int || qVariantValue.typeId() == QMetaType::UInt || qVariantValue.typeId() == QMetaType::LongLong || qVariantValue.typeId() == QMetaType::ULongLong) {
            QSpinBox *spinBox = new QSpinBox(this);
            spinBox->setRange(-2147483647, 2147483647); // Max int range
            spinBox->setValue(qVariantValue.toInt());
            editorWidget = spinBox;
            connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [&config, property](int value) {
                //  qDebug() << "Setting int:" << property.name() << value;
                bool success = property.write(&config, value);
                //  if (!success) qDebug() << "Failed to write int property:" << property.name();
            });
        } else if (strcmp(property.typeName(), "QColor") == 0) {
            QPushButton *button = new QPushButton(qVariantValue.value<QColor>().name(), this);
            button->setFlat(true); // Optional: make it look less like a button
            button->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; border: 1px solid black; padding: 2px; text-align: left; }")
                                  .arg(qVariantValue.value<QColor>().name())
                                  .arg(qVariantValue.value<QColor>().lightness() < 128 ? "white" : "black"));

            editorWidget = button;
            QPointer<QPushButton> buttonPtr(button); // Use QPointer for safety in lambda
            connect(button, &QPushButton::clicked, this, [this, &config, property, buttonPtr]() {
                if (!buttonPtr) return;
                QColor initialColor = property.read(&config).value<QColor>();
                QColor color = QColorDialog::getColor(initialColor, this, "Select Color");
                if (color.isValid()) {
                    //  qDebug() << "Setting QColor:" << property.name() << color.name();
                    bool success = property.write(&config, color);
                    //  if (!success) qDebug() << "Failed to write QColor property:" << property.name();
                    if (success) {
                        buttonPtr->setText(color.name());
                        buttonPtr->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; border: 1px solid black; padding: 2px; text-align: left; }")
                                               .arg(color.name())
                                               .arg(color.lightness() < 128 ? "white" : "black"));
                    }
                }
            });
        } else if (strcmp(property.typeName(), "XColor") == 0) {
            XColor xcolor = qVariantValue.value<XColor>();
            QColor qcolor = xcolor.getColor();
            QPushButton *button = new QPushButton(qcolor.name(), this);
            button->setFlat(true);
            button->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; border: 1px solid black; padding: 2px; text-align: left; }")
                                  .arg(qcolor.name())
                                  .arg(qcolor.lightness() < 128 ? "white" : "black"));
            editorWidget = button;
            QPointer<QPushButton> buttonPtr(button);
            connect(button, &QPushButton::clicked, this, [this, &config, property, buttonPtr]() {
                if (!buttonPtr) return;
                XColor initialXColor = property.read(&config).value<XColor>();
                QColor initialQColor = initialXColor.getColor();
                QColor qcolor = QColorDialog::getColor(initialQColor, this, "Select Color");
                if (qcolor.isValid()) {
                    XColor newXColor(qcolor);
                    //  qDebug() << "Setting XColor (via QColor):" << property.name() << qcolor.name();
                    bool success = property.write(&config, QVariant::fromValue(newXColor));
                    //  if (!success) qDebug() << "Failed to write XColor property:" << property.name();
                    if (success) {
                         buttonPtr->setText(qcolor.name());
                         buttonPtr->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; border: 1px solid black; padding: 2px; text-align: left; }")
                                               .arg(qcolor.name())
                                               .arg(qcolor.lightness() < 128 ? "white" : "black"));
                    }
                }
            });
        }
        // Add more types as needed: float, double, QByteArray (for geometry), enums etc.

        if (editorWidget) {
            formLayout->addRow(nameLabel, editorWidget);
            m_settingLabels.append(nameLabel);
            m_settingWidgets.append(editorWidget);
        } else {
            // qDebug() << "No editor for property type:" << property.typeName() << "for property" << name;
            delete nameLabel; // Clean up unused label
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
        QLabel *label = qobject_cast<QLabel*>(m_settingLabels.at(i)); // QWidget to QLabel
        QWidget *widget = m_settingWidgets.at(i);

        if (label && widget) {
            bool match = lowerText.isEmpty() || label->text().toLower().contains(lowerText);

            if (formLayout) {
                // Find the row index for this label. This is a bit inefficient but necessary.
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
                    // Fallback if row not found (should not happen if label is in this formLayout)
                    label->setVisible(match);
                    widget->setVisible(match);
                }
            } else {
                // Fallback if formLayout is not found (should not happen)
                label->setVisible(match);
                widget->setVisible(match);
            }
        }
    }
}
