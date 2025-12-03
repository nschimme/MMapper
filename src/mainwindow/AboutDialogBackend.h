#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include <QObject>
#include <QString>
#include <QQmlListProperty>

class LicenseInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString introText READ introText CONSTANT)
    Q_PROPERTY(QString licenseText READ licenseText CONSTANT)

public:
    LicenseInfo(const QString &title, const QString &introText, const QString &licenseText, QObject *parent = nullptr)
        : QObject(parent), m_title(title), m_introText(introText), m_licenseText(licenseText) {}

    QString title() const { return m_title; }
    QString introText() const { return m_introText; }
    QString licenseText() const { return m_licenseText; }

private:
    QString m_title;
    QString m_introText;
    QString m_licenseText;
};

class AboutDialogBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString aboutText READ aboutText CONSTANT)
    Q_PROPERTY(QString authorsText READ authorsText CONSTANT)
    Q_PROPERTY(QQmlListProperty<LicenseInfo> licenses READ licenses CONSTANT)

public:
    explicit AboutDialogBackend(QObject *parent = nullptr);

    QString aboutText() const;
    QString authorsText() const;
    QQmlListProperty<LicenseInfo> licenses();

private:
    QList<LicenseInfo*> m_licenses;
};
