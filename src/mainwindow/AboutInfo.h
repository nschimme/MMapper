#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QAbstractListModel>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

// List model backing the About dialog's Licenses tab. A QAbstractListModel
// (rather than a QVariantList of QVariantMaps) so the QML Repeater has a
// stable role-based model — a plain QVariantList-of-QVariantMap crashes the
// QML engine's identifier interning when consumed as a Repeater model.
class NODISCARD_QOBJECT LicenseModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum RoleEnum { TitleRole = Qt::UserRole + 1, IntroRole, TextRole };

    struct NODISCARD Entry final
    {
        QString title;
        QString intro;
        QString text;
    };

private:
    QVector<Entry> m_entries;

public:
    explicit LicenseModel(QObject *parent)
        : QAbstractListModel(parent)
    {}
    void setEntries(QVector<Entry> entries);

    NODISCARD int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    NODISCARD QVariant data(const QModelIndex &index, int role) const override;
    NODISCARD QHash<int, QByteArray> roleNames() const override;
};

// Widget-free content provider for the About dialog. Builds its HTML/text
// content once at construction (mirroring AboutDialog's widget-based
// construction verbatim) and exposes it to QML via Q_PROPERTYs, so the same
// content can back either the widget AboutDialog (non-QML build) or
// AboutDialog.qml (QML build). Content is fixed for the lifetime of the
// process, so all properties are CONSTANT.
class NODISCARD_QOBJECT AboutInfo final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString aboutHtml READ getAboutHtml CONSTANT)
    Q_PROPERTY(QString authorsHtml READ getAuthorsHtml CONSTANT)
    Q_PROPERTY(LicenseModel *licenses READ getLicenses CONSTANT)

private:
    QString m_aboutHtml;
    QString m_authorsHtml;
    LicenseModel *m_licenses;

public:
    explicit AboutInfo(QObject *parent);

    NODISCARD const QString &getAboutHtml() const { return m_aboutHtml; }
    NODISCARD const QString &getAuthorsHtml() const { return m_authorsHtml; }
    NODISCARD LicenseModel *getLicenses() const { return m_licenses; }
};
