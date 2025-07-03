// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "findreplacedialog.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
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
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject); // reject() closes the dialog

    // Layout setup
    QHBoxLayout *topLeftLayout = new QHBoxLayout;
    topLeftLayout->addWidget(m_findLabel);
    topLeftLayout->addWidget(m_findLineEdit);

    QHBoxLayout *midLeftLayout = new QHBoxLayout;
    midLeftLayout->addWidget(m_replaceLabel);
    midLeftLayout->addWidget(m_replaceLineEdit);

    QVBoxLayout *leftLayout = new QVBoxLayout;
    leftLayout->addLayout(topLeftLayout);
    leftLayout->addLayout(midLeftLayout);
    leftLayout->addWidget(m_caseSensitiveCheckBox);
    leftLayout->addWidget(m_wrapAroundCheckBox);
    leftLayout->addWidget(m_wholeWordsCheckBox);

    QVBoxLayout *rightLayout = new QVBoxLayout;
    rightLayout->addWidget(m_findNextButton);
    rightLayout->addWidget(m_findPreviousButton);
    rightLayout->addWidget(m_replaceButton);
    rightLayout->addWidget(m_replaceAllButton);
    rightLayout->addStretch(); // Pushes close button to the bottom
    rightLayout->addWidget(m_closeButton);

    QGridLayout *mainLayout = new QGridLayout;
    mainLayout->addLayout(leftLayout, 0, 0);
    mainLayout->addLayout(rightLayout, 0, 1);
    setLayout(mainLayout);

    setWindowTitle(tr("Find and Replace"));
    setFixedSize(sizeHint()); // Fix dialog size to prevent resize

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
