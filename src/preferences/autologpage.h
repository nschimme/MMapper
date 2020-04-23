#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Mattias Viklund <devmew@exedump.com> (Mew_)

#include <QWidget>
#include <QtCore>

namespace Ui {
class AutoLogPage;
}

class AutoLogPage : public QWidget
{
    Q_OBJECT

public:
    explicit AutoLogPage(QWidget *parent = nullptr);
    ~AutoLogPage() override;

public slots:
    void loadConfig();

    void selectLogLocationButtonClicked(int);
    void autoLogCheckBoxChanged(int);
    void maxLogLinesChanged(int);
    void deleteOldLogsCheckBoxChanged(int);
    void deleteLogsOlderThanChanged(int);
    void warnWhenDeletingCheckBoxChanged(int);
    void warnWhenMoreThanChanged(int);

signals:

private:
    Ui::AutoLogPage *ui = nullptr;
};
