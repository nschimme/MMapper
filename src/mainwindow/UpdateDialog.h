#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QNetworkAccessManager>

#include "../global/Array.h"

class CompareVersion final
{
public:
    explicit CompareVersion(const QString &versionStr) noexcept;
    bool operator>(const CompareVersion &other) const;
    bool operator==(const CompareVersion &other) const;
    operator QString() const;

private:
    MMapper::Array<int, 3> parts;
};

class UpdateDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UpdateDialog(QWidget *parent = nullptr);

    void open() override;

private slots:
    void managerFinished(QNetworkReply *reply);
    void accepted();

private:
    QNetworkAccessManager manager;
    QLabel *text = nullptr;
    QDialogButtonBox *buttonBox = nullptr;
};
