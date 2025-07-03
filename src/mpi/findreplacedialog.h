#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

/// Dialog for Find and Replace operations.
/// Provides UI elements for entering find text, replace text,
/// options for search (case sensitive, whole words, wrap around),
/// and triggers signals for find/replace actions.
class FindReplaceDialog : public QDialog
{
    Q_OBJECT

public:
    /// Constructs the FindReplaceDialog.
    /// @param parent The parent widget.
    explicit FindReplaceDialog(QWidget *parent = nullptr);

    /// Returns the text to find.
    QString getFindText() const;
    /// Returns the text to replace with.
    QString getReplaceText() const;
    /// Returns true if the search should be case sensitive.
    bool isCaseSensitive() const;
    /// Returns true if the search should wrap around the document.
    bool isWrapAround() const;
    /// Returns true if the search should match whole words only.
    bool isWholeWords() const;

signals:
    /// Emitted when the "Find Next" button is clicked or action triggered.
    void findNext();
    /// Emitted when the "Find Previous" button isclicked or action triggered.
    void findPrevious();
    /// Emitted when the "Replace" button is clicked or action triggered.
    void replace();
    /// Emitted when the "Replace All" button is clicked or action triggered.
    void replaceAll();

private:
    // UI Elements
    QLabel *m_findLabel;
    QLineEdit *m_findLineEdit;
    QLabel *m_replaceLabel;
    QLineEdit *m_replaceLineEdit;
    QCheckBox *m_caseSensitiveCheckBox;
    QCheckBox *m_wrapAroundCheckBox;
    QCheckBox *m_wholeWordsCheckBox;
    QPushButton *m_findNextButton;
    QPushButton *m_findPreviousButton;
    QPushButton *m_replaceButton;
    QPushButton *m_replaceAllButton;
    QPushButton *m_closeButton;
};
