#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QObject>
#include <QPointer>
#include <QString>

class QWidget;

// Q_PROPERTY façade over Configuration::MumeClientProtocolSettings (see
// ../configuration/configuration.h), mirroring mumeprotocolpage.cpp's
// apply-on-change semantics. MumeClientProtocolSettings has no
// ChangeMonitor, so reload() re-emits sig_changed unconditionally.
class NODISCARD_QOBJECT MumeProtocolPageAdapter final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool internalRemoteEditor READ getInternalRemoteEditor WRITE setInternalRemoteEditor
                   NOTIFY sig_changed)
    Q_PROPERTY(QString externalRemoteEditorCommand READ getExternalRemoteEditorCommand WRITE
                   setExternalRemoteEditorCommand NOTIFY sig_changed)

private:
    // Parent for the native QFileDialog invoked by browseForEditor(); owned
    // by PreferencesController, not by this adapter.
    QPointer<QWidget> m_dialogParent;

public:
    explicit MumeProtocolPageAdapter(QWidget *dialogParent, QObject *parent);

public:
    NODISCARD bool getInternalRemoteEditor() const;
    void setInternalRemoteEditor(bool value);

    NODISCARD QString getExternalRemoteEditorCommand() const;
    void setExternalRemoteEditorCommand(const QString &value);

public:
    // Opens a native "choose editor" file dialog (mirrors
    // MumeProtocolPage::slot_externalEditorBrowseButtonClicked's
    // quote-escaping) and, if a file was chosen, updates the property/config
    // and emits sig_changed.
    Q_INVOKABLE void browseForEditor();

public slots:
    void reload();

signals:
    void sig_changed();
};
