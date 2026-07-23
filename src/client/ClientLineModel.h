#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/AnsiTextUtils.h"
#include "../global/macros.h"

#include <vector>

#include <QAbstractListModel>
#include <QString>
#include <QStringView>

// A QAbstractListModel exposing MUD output as a sequence of rows, one per
// finished line plus a trailing always-present partial (in-progress) line.
// This mirrors the ANSI-decoding and control-character semantics of
// AnsiTextHelper::displayText() (see client/displaywidget.h/.cpp) closely
// enough to serve as a drop-in replacement for the QML client display, but
// it produces small cached HTML strings per row instead of mutating a
// QTextDocument. See ClientLineModel.cpp for the specific ways this
// diverges from the widget (URL linkification IS implemented, reusing
// AnsiTextToDocument's url_regex/style from global/AnsiHtml.cpp; the visual
// bell is signaled via sig_visualBell() instead of flashing a document
// background directly; and backspace/erase-line handling is simplified to
// an immediate delete-previous-character rather than the widget's
// lazy/document-cursor based approach).
class NODISCARD_QOBJECT ClientLineModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum class NODISCARD RoleEnum {
        Html = Qt::UserRole + 1,
        Plain,
    };

private:
    struct NODISCARD Run final
    {
        RawAnsi ansi;
        QString text;
    };

    struct NODISCARD FinishedLine final
    {
        QString html;
        QString plain;
    };

private:
    std::vector<FinishedLine> m_finishedLines;
    std::vector<Run> m_partialRuns;
    QString m_partialHtml;
    QString m_partialPlain;
    RawAnsi m_currentAnsi;
    // True when the last appendText() call ended in a '\r' whose line break
    // has already been emitted; a leading '\n' in the next call is then
    // swallowed so a "\r\n" pair split across chunks yields ONE break.
    bool m_pendingCr = false;

public:
    explicit ClientLineModel(QObject *parent = nullptr);
    ~ClientLineModel() final;

public:
    NODISCARD int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    NODISCARD QVariant data(const QModelIndex &index, int role) const override;
    NODISCARD QHash<int, QByteArray> roleNames() const override;

public:
    // Parses `text` for ANSI escapes using the same decoding rules as
    // AnsiTextHelper::displayText(), carrying ANSI state across calls, and
    // appends the result to the model (finishing lines at line-break
    // boundaries and always leaving a trailing partial-line row). "\r\n" is
    // treated as a single line break and a lone '\r' breaks the line too,
    // mirroring QTextCursor::insertText(); like the ANSI state, a trailing
    // '\r' carries across calls so a split "\r\n" still yields one break.
    // After processing,
    // finished lines are capped at getConfig().integratedClient.linesOfScrollback.
    void appendText(QStringView text);

    Q_INVOKABLE void clear();
    // Copies toPlainText() to the system clipboard; mirrors LogModel::copyAll()
    // and backs ClientDisplay.qml's "Copy All" context-menu action.
    Q_INVOKABLE void copyAll() const;

public:
    // Joins every finished line's plain text (plus the current partial
    // line) with '\n' separators. Used for saving logs / feeding QML text
    // fields that want raw text.
    NODISCARD QString toPlainText() const;
    // Wraps the equivalent join of each line's cached HTML in a minimal
    // <html><body> shell styled with the configured client colors. This is
    // a much simpler approximation of QTextDocument::toHtml() (used by the
    // legacy DisplayWidget, see ClientWidget::slot_saveLogAsHtml()): no
    // per-run <head>/<style> metadata, just inline styles already baked
    // into each row's cached HTML by htmlForRuns().
    NODISCARD QString toHtml() const;

signals:
    // Emitted once per QC_ALERT (bell) character found in appendText()'s
    // input, but only when config.integratedClient.visualBell is enabled;
    // mirrors DisplayWidget::slot_displayText()'s on_bell() visual-flash
    // branch (see client/displaywidget.cpp), except the actual flashing is
    // left to the QML delegate (ClientDisplay.qml) since this model has no
    // window of its own to flash.
    void sig_visualBell();

private:
    void processSegment(QStringView segment);
    void appendRawWithAnsi(QStringView text, const RawAnsi &ansi);
    void appendToPartial(QStringView text, const RawAnsi &ansi);
    void finishPartialLine();
    void eraseLastChar();
    void regeneratePartialCache();
    void trimScrollback();

private:
    NODISCARD QString htmlForRuns(const std::vector<Run> &runs) const;
    NODISCARD static QString plainForRuns(const std::vector<Run> &runs);
};
