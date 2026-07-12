// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "ClientLineModel.h"

#include "../configuration/configuration.h"
#include "../global/TextUtils.h"

#include <algorithm>

#include <QRegularExpression>
#include <QStringList>

namespace { // anonymous

// Identical to the regex used by AnsiTextHelper::displayText() (see
// client/displaywidget.cpp): escape + optional non-letter bytes + an
// optional trailing letter.
const QRegularExpression &ansiRegex()
{
    static const QRegularExpression regex{R"regex(\x1B[^A-Za-z\x1B]*[A-Za-z]?)regex"};
    return regex;
}

QString htmlEscape(const QStringView text)
{
    QString result;
    result.reserve(text.size());
    for (const QChar c : text) {
        switch (c.unicode()) {
        case '&':
            result += QStringLiteral("&amp;");
            break;
        case '<':
            result += QStringLiteral("&lt;");
            break;
        case '>':
            result += QStringLiteral("&gt;");
            break;
        case '"':
            result += QStringLiteral("&quot;");
            break;
        default:
            result += c;
            break;
        }
    }
    return result;
}

} // namespace

ClientLineModel::ClientLineModel(QObject *const parent)
    : QAbstractListModel(parent)
{}

ClientLineModel::~ClientLineModel() = default;

int ClientLineModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    // +1 for the always-present trailing partial line.
    return static_cast<int>(m_finishedLines.size()) + 1;
}

QVariant ClientLineModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid()) {
        return {};
    }
    const int row = index.row();
    const auto numFinished = static_cast<int>(m_finishedLines.size());
    if (row < 0 || row > numFinished) {
        return {};
    }

    const bool isPartialRow = (row == numFinished);
    switch (static_cast<RoleEnum>(role)) {
    case RoleEnum::Html:
        return isPartialRow ? m_partialHtml : m_finishedLines[static_cast<size_t>(row)].html;
    case RoleEnum::Plain:
        return isPartialRow ? m_partialPlain : m_finishedLines[static_cast<size_t>(row)].plain;
    }
    return {};
}

QHash<int, QByteArray> ClientLineModel::roleNames() const
{
    return {
        {static_cast<int>(RoleEnum::Html), "html"},
        {static_cast<int>(RoleEnum::Plain), "plain"},
    };
}

void ClientLineModel::appendText(const QStringView text)
{
    // Bell handling mirrors DisplayWidget::slot_displayText()'s
    // foreach_char(QC_ALERT, ...) split: the alert character never reaches
    // the display; DisplayWidget additionally beeps/flashes, which has no
    // equivalent here since this model has no notion of a window to flash.
    qsizetype pos = 0;
    const qsizetype len = text.size();
    while (pos < len) {
        const qsizetype bellPos = text.indexOf(mmqt::QC_ALERT, pos);
        if (bellPos < 0) {
            processSegment(text.mid(pos));
            break;
        }
        processSegment(text.mid(pos, bellPos - pos));
        pos = bellPos + 1;
    }

    regeneratePartialCache();
    trimScrollback();
}

void ClientLineModel::processSegment(const QStringView segment)
{
    mmqt::foreach_regex(
        ansiRegex(),
        segment,
        [this](const QStringView ansiStr) {
            assert(!ansiStr.isEmpty() && ansiStr.front() == char_consts::C_ESC);
            if (mmqt::isAnsiColor(ansiStr)) {
                if (auto optNewColor = mmqt::parseAnsiColor(m_currentAnsi, ansiStr)) {
                    m_currentAnsi = *optNewColor;
                }
            } else if (mmqt::isAnsiEraseLine(ansiStr)) {
                eraseLastChar();
            } else {
                // Unknown escape: same fallback as AnsiTextHelper::displayText(),
                // which shows "<ESC>" in default formatting followed by the
                // rest of the (unrecognized) escape sequence in the current
                // formatting.
                appendRawWithAnsi(u"<ESC>", RawAnsi{});
                if (ansiStr.length() > 1) {
                    appendRawWithAnsi(ansiStr.mid(1), m_currentAnsi);
                }
            }
        },
        [this](const QStringView textStr) {
            // Backspace splitting mirrors foreach_backspace() in
            // displaywidget.cpp. Unlike the widget (which lazily deletes on
            // the next insert via try_remove_backspace()), this model
            // deletes the preceding character immediately; the two produce
            // the same net result for well-formed "\b<char>" sequences.
            qsizetype start = 0;
            const qsizetype textLen = textStr.size();
            while (start < textLen) {
                const qsizetype bsPos = textStr.indexOf(char_consts::C_BACKSPACE, start);
                if (bsPos < 0) {
                    appendRawWithAnsi(textStr.mid(start), m_currentAnsi);
                    break;
                }
                appendRawWithAnsi(textStr.mid(start, bsPos - start), m_currentAnsi);
                eraseLastChar();
                start = bsPos + 1;
            }
        });
}

void ClientLineModel::appendRawWithAnsi(const QStringView text, const RawAnsi &ansi)
{
    qsizetype start = 0;
    const qsizetype len = text.size();
    while (true) {
        const qsizetype nl = text.indexOf(char_consts::C_NEWLINE, start);
        if (nl < 0) {
            appendToPartial(text.mid(start), ansi);
            break;
        }
        appendToPartial(text.mid(start, nl - start), ansi);
        finishPartialLine();
        start = nl + 1;
        if (start >= len) {
            break;
        }
    }
}

void ClientLineModel::appendToPartial(const QStringView text, const RawAnsi &ansi)
{
    if (text.isEmpty()) {
        return;
    }
    if (m_partialRuns.empty() || m_partialRuns.back().ansi != ansi) {
        m_partialRuns.push_back(Run{ansi, QString()});
    }
    m_partialRuns.back().text += text;
}

void ClientLineModel::finishPartialLine()
{
    const int row = static_cast<int>(m_finishedLines.size());
    beginInsertRows(QModelIndex(), row, row);
    m_finishedLines.push_back(FinishedLine{htmlForRuns(m_partialRuns), plainForRuns(m_partialRuns)});
    m_partialRuns.clear();
    endInsertRows();
}

void ClientLineModel::eraseLastChar()
{
    // Mirrors the practical effect of AnsiTextHelper::displayText()'s
    // "erase to end of line" handling: because the cursor there is always
    // at the append point (nothing follows it), that code path only ever
    // deletes the single character immediately preceding the cursor.
    while (!m_partialRuns.empty() && m_partialRuns.back().text.isEmpty()) {
        m_partialRuns.pop_back();
    }
    if (m_partialRuns.empty()) {
        return;
    }
    QString &text = m_partialRuns.back().text;
    text.chop(1);
    if (text.isEmpty()) {
        m_partialRuns.pop_back();
    }
}

void ClientLineModel::regeneratePartialCache()
{
    m_partialHtml = htmlForRuns(m_partialRuns);
    m_partialPlain = plainForRuns(m_partialRuns);
    const int row = static_cast<int>(m_finishedLines.size());
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx);
}

void ClientLineModel::trimScrollback()
{
    const int limit = std::max(0, getConfig().integratedClient.linesOfScrollback);
    const int excess = static_cast<int>(m_finishedLines.size()) - limit;
    if (excess <= 0) {
        return;
    }
    beginRemoveRows(QModelIndex(), 0, excess - 1);
    m_finishedLines.erase(m_finishedLines.begin(),
                          m_finishedLines.begin() + static_cast<size_t>(excess));
    endRemoveRows();
}

void ClientLineModel::clear()
{
    beginResetModel();
    m_finishedLines.clear();
    m_partialRuns.clear();
    m_partialHtml.clear();
    m_partialPlain.clear();
    m_currentAnsi = RawAnsi{};
    endResetModel();
}

QString ClientLineModel::htmlForRuns(const std::vector<Run> &runs) const
{
    const auto &settings = getConfig().integratedClient;
    const QColor defaultFg = settings.foregroundColor;
    const QColor defaultBg = settings.backgroundColor;

    QString html;
    for (const Run &run : runs) {
        if (run.text.isEmpty()) {
            continue;
        }
        const RawAnsi &ansi = run.ansi;
        const bool intense = ansi.hasBold();

        QColor fg = mmqt::decodeColor(ansi.fg, defaultFg, intense);
        QColor bg = mmqt::decodeColor(ansi.bg, defaultBg, false);
        if (ansi.hasReverse()) {
            mmqt::reverseInPlace(fg);
            mmqt::reverseInPlace(bg);
        }

        QString style;
        if (fg != defaultFg) {
            style += QStringLiteral("color:%1;").arg(fg.name());
        }
        if (bg != defaultBg) {
            style += QStringLiteral("background-color:%1;").arg(bg.name());
        }
        if (ansi.hasBold()) {
            style += QStringLiteral("font-weight:bold;");
        }
        if (ansi.hasItalic()) {
            style += QStringLiteral("font-style:italic;");
        }
        if (ansi.hasUnderline()) {
            style += QStringLiteral("text-decoration:underline;");
        }

        const QString escaped = htmlEscape(run.text);
        if (style.isEmpty()) {
            html += escaped;
        } else {
            html += QStringLiteral("<span style=\"%1\">%2</span>").arg(style, escaped);
        }
    }
    return html;
}

QString ClientLineModel::toPlainText() const
{
    QStringList lines;
    lines.reserve(static_cast<int>(m_finishedLines.size()) + 1);
    for (const FinishedLine &line : m_finishedLines) {
        lines.push_back(line.plain);
    }
    lines.push_back(m_partialPlain);
    return lines.join(QChar::fromLatin1('\n'));
}

QString ClientLineModel::toHtml() const
{
    const auto &settings = getConfig().integratedClient;
    QString body;
    body.reserve(static_cast<int>(m_finishedLines.size()) * 32);
    for (const FinishedLine &line : m_finishedLines) {
        body += line.html;
        body += QStringLiteral("<br>\n");
    }
    body += m_partialHtml;

    return QStringLiteral("<html><body style=\"background-color:%1;color:%2;"
                          "white-space:pre-wrap;\">%3</body></html>")
        .arg(settings.backgroundColor.name(), settings.foregroundColor.name(), body);
}

QString ClientLineModel::plainForRuns(const std::vector<Run> &runs)
{
    QString plain;
    for (const Run &run : runs) {
        plain += run.text;
    }
    return plain;
}
