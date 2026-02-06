#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "RemoteEditViewModel.h"
#include <QDialog>
#include <QPlainTextEdit>
#include <memory>

class QMenuBar;
class QStatusBar;
class GotoWidget;
class FindReplaceWidget;

class NODISCARD_QOBJECT RemoteTextEdit final : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit RemoteTextEdit(const QString &text, QWidget *parent);
    void replaceAll(const QString &text);
};

class NODISCARD_QOBJECT RemoteEditWidget : public QDialog
{
    Q_OBJECT
private:
    std::unique_ptr<RemoteEditViewModel> m_viewModel;
    RemoteTextEdit *m_textEdit;
    GotoWidget *m_gotoWidget;
    FindReplaceWidget *m_findReplaceWidget;
    QMenuBar *m_menuBar;
    QStatusBar *m_statusBar;

public:
    explicit RemoteEditWidget(bool editSession, QString title, QString body, QWidget *parent);
    ~RemoteEditWidget() override;

signals:
    void sig_save(const QString &text);
    void sig_cancel();

private slots:
    void slot_save(const QString &text);
    void slot_cancel();
};
