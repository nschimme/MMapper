#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QColor>
#include <QObject>
#include <QString>

// Q_PROPERTY façade over the plain-struct GroupManagerSettings subgroup of
// Configuration (see ../configuration/configuration.h). Unlike most other
// config subgroups, GroupManagerSettings has no ChangeMonitor: it is a bag
// of public fields written directly by callers (most notably the
// preferences dialog). That means this façade cannot passively observe
// changes; each setter here notifies QML itself, but any code path that
// writes setConfig().groupManager.* directly (outside of this class'
// setters) will leave QML holding stale values until reload() is called
// explicitly. MainWindow is responsible for calling reload() after any
// dialog that may have touched these fields (e.g. ConfigDialog) closes or
// signals a change. Other external mutations not funneled through such a
// hook are accepted as stale until the next reload().
class NODISCARD_QOBJECT QmlConfig final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool npcHide READ getNpcHide WRITE setNpcHide NOTIFY npcHideChanged)
    Q_PROPERTY(
        bool npcSortBottom READ getNpcSortBottom WRITE setNpcSortBottom NOTIFY npcSortBottomChanged)
    Q_PROPERTY(QColor groupColor READ getGroupColor WRITE setGroupColor NOTIFY groupColorChanged)

    // Read-only façade over integratedClient's font/color settings, resolved
    // the same way DescriptionAdapter::resolveConfig() resolves its own
    // font/color properties (see ../media/DescriptionAdapter.cpp). Used by
    // ClientDisplay.qml. There are no setters: the integrated client's
    // font/colors are only ever changed via the preferences dialog, which
    // must call reload() afterward (same as every other property here).
    Q_PROPERTY(QString clientFontFamily READ getClientFontFamily NOTIFY clientFontChanged)
    Q_PROPERTY(int clientFontPointSize READ getClientFontPointSize NOTIFY clientFontChanged)
    Q_PROPERTY(QColor clientBgColor READ getClientBgColor NOTIFY clientColorsChanged)
    Q_PROPERTY(QColor clientFgColor READ getClientFgColor NOTIFY clientColorsChanged)

    // Read-only façade over two more integratedClient settings used by the
    // QML client input/preview: whether Enter clears the input box (see
    // InputWidget::gotInput()) and how many trailing lines the peek preview
    // shows (see PreviewWidget). Same reload()-driven staleness contract as
    // every other property here.
    Q_PROPERTY(bool clientClearInputOnEnter READ getClientClearInputOnEnter NOTIFY
                   clientClearInputOnEnterChanged)
    Q_PROPERTY(int clientPreviewLines READ getClientPreviewLines NOTIFY clientPreviewLinesChanged)

    // Read-only façade over the integrated client's configured terminal
    // size, used by ClientPanel.qml to mirror DisplayWidget::sizeHint()'s
    // columns/rows-based minimum size (see displaywidget.cpp). Same
    // reload()-driven staleness contract as every other property here.
    Q_PROPERTY(int clientColumns READ getClientColumns NOTIFY clientSizeChanged)
    Q_PROPERTY(int clientRows READ getClientRows NOTIFY clientSizeChanged)

private:
    // Cache of the last-known values, used only to detect external changes
    // in reload(); the getters below always read the live Configuration.
    bool m_npcHide = false;
    bool m_npcSortBottom = false;
    QColor m_groupColor;
    QString m_clientFontFamily;
    int m_clientFontPointSize = -1;
    QColor m_clientBgColor;
    QColor m_clientFgColor;
    bool m_clientClearInputOnEnter = false;
    int m_clientPreviewLines = 0;
    int m_clientColumns = 0;
    int m_clientRows = 0;

public:
    explicit QmlConfig(QObject *parent = nullptr);

public:
    NODISCARD bool getNpcHide() const;
    void setNpcHide(bool value);

    NODISCARD bool getNpcSortBottom() const;
    void setNpcSortBottom(bool value);

    NODISCARD QColor getGroupColor() const;
    void setGroupColor(const QColor &value);

    NODISCARD QString getClientFontFamily() const;
    NODISCARD int getClientFontPointSize() const;
    NODISCARD QColor getClientBgColor() const;
    NODISCARD QColor getClientFgColor() const;

    NODISCARD bool getClientClearInputOnEnter() const;
    NODISCARD int getClientPreviewLines() const;

    NODISCARD int getClientColumns() const;
    NODISCARD int getClientRows() const;

public slots:
    // Re-syncs the cached values against the live Configuration and emits
    // the appropriate *Changed signals for anything that differs. Must be
    // called whenever code outside this class may have written to
    // getConfig().groupManager.
    void reload();

signals:
    void npcHideChanged();
    void npcSortBottomChanged();
    void groupColorChanged();
    void clientFontChanged();
    void clientColorsChanged();
    void clientClearInputOnEnterChanged();
    void clientPreviewLinesChanged();
    void clientSizeChanged();
};
