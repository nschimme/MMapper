#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "../proxy/GmcpMessage.h"

#include <QByteArray>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QPixmap>
#include <QString>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

class NODISCARD_QOBJECT DescriptionWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit DescriptionWidget(QWidget *parent = nullptr);
    ~DescriptionWidget() final = default;

public slots:
    void slot_parseGmcpInput(const GmcpMessage &gmcp);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setText(const QString &text);
    void updateLabel();

private:
    QLabel *m_label;
    QTextEdit *m_textEdit;

private:
    QString m_roomName;
    QString m_roomDescription;
    QString m_areaName;
};
