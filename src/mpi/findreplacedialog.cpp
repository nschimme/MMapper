// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "findreplacedialog.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpacerItem>
#include <QVBoxLayout>

FindReplaceDialog::FindReplaceDialog(QWidget *parent)
    : QDialog(parent)
{
    m_findLabel = new QLabel(tr("&Find what:"));
    m_findLineEdit = new QLineEdit;
    m_findLabel->setBuddy(m_findLineEdit);

    m_replaceLabel = new QLabel(tr("Re&place with:"));
    m_replaceLineEdit = new QLineEdit;
    m_replaceLabel->setBuddy(m_replaceLineEdit);

    m_caseSensitiveCheckBox = new QCheckBox(tr("Case sensiti&ve"));
    m_wrapAroundCheckBox = new QCheckBox(tr("Wrap aroun&d"));
    m_wrapAroundCheckBox->setChecked(true); // Default wrap around to true
    m_wholeWordsCheckBox = new QCheckBox(tr("Whole &words"));

    m_findNextButton = new QPushButton(tr("&Find Next"));
    m_findPreviousButton = new QPushButton(tr("Find &Previous"));
    m_replaceButton = new QPushButton(tr("&Replace"));
    m_replaceAllButton = new QPushButton(tr("Replace &All"));
    m_closeButton = new QPushButton(tr("Close"));

    // Connect buttons to their respective signals
    connect(m_findNextButton, &QPushButton::clicked, this, &FindReplaceDialog::findNext);
    connect(m_findPreviousButton, &QPushButton::clicked, this, &FindReplaceDialog::findPrevious);
    connect(m_replaceButton, &QPushButton::clicked, this, &FindReplaceDialog::replace);
    connect(m_replaceAllButton, &QPushButton::clicked, this, &FindReplaceDialog::replaceAll);
    connect(m_closeButton,
            &QPushButton::clicked,
            this,
            &QDialog::reject); // reject() closes the dialog

    // Main layout
    QVBoxLayout *mainLayout = new QVBoxLayout;
    setLayout(mainLayout);

    // Find section
    QGridLayout *findReplaceLayout = new QGridLayout;
    findReplaceLayout->addWidget(m_findLabel, 0, 0);
    findReplaceLayout->addWidget(m_findLineEdit, 0, 1);
    findReplaceLayout->addWidget(m_replaceLabel, 1, 0);
    findReplaceLayout->addWidget(m_replaceLineEdit, 1, 1);
    mainLayout->addLayout(findReplaceLayout);

    // Options section (checkboxes)
    QHBoxLayout *optionsLayout = new QHBoxLayout;
    optionsLayout->addWidget(m_caseSensitiveCheckBox);
    optionsLayout->addWidget(m_wholeWordsCheckBox);
    optionsLayout->addWidget(m_wrapAroundCheckBox);
    optionsLayout->addStretch(); // Push checkboxes to the left
    mainLayout->addLayout(optionsLayout);

    // Buttons section
    QGridLayout *buttonsLayout = new QGridLayout;
    buttonsLayout->addWidget(m_findNextButton, 0, 0);
    buttonsLayout->addWidget(m_findPreviousButton, 0, 1);
    buttonsLayout->addWidget(m_replaceButton, 1, 0);
    buttonsLayout->addWidget(m_replaceAllButton, 1, 1);
    // Add empty column to push action buttons left, and close button right
    buttonsLayout->addItem(new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum),
                           0,
                           2,
                           2,
                           1);
    buttonsLayout->addWidget(m_closeButton, 0, 3, 2, 1, Qt::AlignRight | Qt::AlignVCenter);

    mainLayout->addLayout(buttonsLayout);

    setWindowTitle(tr("Find and Replace"));
    // setFixedSize(sizeHint()); // Consider making it resizable or setting a minimum size. For now,
    // let's allow resizing.
    adjustSize(); // Adjust size to content

    // Set focus to findLineEdit when dialog opens
    m_findLineEdit->setFocus();
}

QString FindReplaceDialog::getFindText() const
{
    return m_findLineEdit->text();
}

QString FindReplaceDialog::getReplaceText() const
{
    return m_replaceLineEdit->text();
}

bool FindReplaceDialog::isCaseSensitive() const
{
    return m_caseSensitiveCheckBox->isChecked();
}

bool FindReplaceDialog::isWrapAround() const
{
    return m_wrapAroundCheckBox->isChecked();
}

bool FindReplaceDialog::isWholeWords() const
{
    return m_wholeWordsCheckBox->isChecked();
}
