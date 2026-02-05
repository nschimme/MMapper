// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "displaywidget.h"
#include "../configuration/configuration.h"
#include "../global/AnsiTextUtils.h"
#include <QApplication>
#include <QScrollBar>
#include <QStyle>
#include <QToolTip>
#include <QTextFrame>
#include <QVBoxLayout>

static constexpr int TAB_WIDTH_SPACES = 8;

namespace {
template<typename Q, typename B>
void foreach_char(const QChar q, const QStringView t, Q &&cq, B &&cb) {
    qsizetype p = 0; while (p != t.length()) {
        qsizetype i = t.indexOf(q, p); if (i < 0) { cb(t.mid(p)); break; }
        cb(t.mid(p, i - p)); cq(); p = i + 1;
    }
}
}

FontDefaults::FontDefaults() {
    const auto &s = getConfig().integratedClient;
    defaultBg = s.backgroundColor; defaultFg = s.foregroundColor;
    serverOutputFont.fromString(s.font);
}

AnsiTextHelper::AnsiTextHelper(QTextEdit &it, FontDefaults d) : textEdit(it), cursor(it.document()->rootFrame()->firstCursorPosition()), format(cursor.charFormat()), defaults(std::move(d)) {}
AnsiTextHelper::AnsiTextHelper(QTextEdit &it) : AnsiTextHelper(it, FontDefaults{}) {}

void AnsiTextHelper::init() {
    auto *rf = textEdit.document()->rootFrame();
    QTextFrameFormat ff = rf->frameFormat();
    ff.setBackground(defaults.defaultBg); ff.setForeground(defaults.defaultFg);
    rf->setFrameFormat(ff);
    format = cursor.charFormat(); setDefaultFormat(format, defaults); cursor.setCharFormat(format);
}

DisplayWidget::DisplayWidget(QWidget *const parent)
    : QTextBrowser(parent), m_ansiTextHelper(*this), m_visualBellTimer(new QTimer(this))
{
    setReadOnly(true); setOverwriteMode(true); setUndoRedoEnabled(false);
    setDocumentTitle("MMapper Mud Client"); setTextInteractionFlags(Qt::TextBrowserInteraction);
    setOpenExternalLinks(true); setTabChangesFocus(false);
    m_ansiTextHelper.init();

    QFontMetrics fm(m_viewModel.font());
    setLineWrapMode(QTextEdit::FixedColumnWidth); setWordWrapMode(QTextOption::WordWrap);
    setSizeIncrement(fm.averageCharWidth(), fm.lineSpacing());
    setTabStopDistance(fm.horizontalAdvance(' ') * TAB_WIDTH_SPACES);

    verticalScrollBar()->setSingleStep(fm.lineSpacing());
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn); setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    connect(this, &DisplayWidget::copyAvailable, this, [this](bool a) { m_canCopy = a; if (a) setFocus(); });
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int v) { emit sig_showPreview(v != verticalScrollBar()->maximum()); });

    m_visualBellTimer->setSingleShot(true);
    connect(m_visualBellTimer, &QTimer::timeout, this, [this]() {
        if (auto *rf = document()->rootFrame()) {
            QTextFrameFormat ff = rf->frameFormat(); ff.setBackground(m_viewModel.backgroundColor()); rf->setFrameFormat(ff);
        }
    });

    connect(&m_viewModel, &DisplayViewModel::sig_visualBell, this, &DisplayWidget::handleVisualBell);
    connect(&m_viewModel, &DisplayViewModel::sig_returnFocusToInput, this, &DisplayWidget::sig_returnFocusToInput);
    connect(&m_viewModel, &DisplayViewModel::sig_showPreview, this, &DisplayWidget::sig_showPreview);
    connect(&m_viewModel, &DisplayViewModel::sig_windowSizeChanged, this, &DisplayWidget::sig_windowSizeChanged);
}

DisplayWidget::~DisplayWidget() = default;

QSize DisplayWidget::sizeHint() const {
    const auto &s = getConfig().integratedClient; QFontMetrics fm(m_viewModel.font());
    int sb = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    int fw = frameWidth() * 2;
    return QSize(s.columns * fm.averageCharWidth() + sb + fw, s.rows * fm.lineSpacing() + sb + fw);
}

void DisplayWidget::resizeEvent(QResizeEvent *e) {
    QFontMetrics fm(m_viewModel.font()); int sb = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    int fw = frameWidth() * 2;
    int cols = (width() - sb - fw) / fm.averageCharWidth();
    int rows = (height() - sb - fw) / fm.lineSpacing();
    if constexpr (CURRENT_PLATFORM == PlatformEnum::Linux) rows -= 6; else if constexpr (CURRENT_PLATFORM == PlatformEnum::Windows) rows -= 2;
    setLineWrapColumnOrWidth(cols); verticalScrollBar()->setPageStep(rows);
    QToolTip::showText(mapToGlobal(rect().center()), QString("%1x%2").arg(cols).arg(rows), this, rect(), 1000);
    m_viewModel.windowSizeChanged(cols, rows);
    QTextEdit::resizeEvent(e);
    emit sig_showPreview(verticalScrollBar()->sliderPosition() != verticalScrollBar()->maximum());
}

void DisplayWidget::keyPressEvent(QKeyEvent *e) {
    if (e->modifiers() != Qt::NoModifier || e->key() == Qt::Key_PageUp || e->key() == Qt::Key_PageDown || e->key() == Qt::Key_Home || e->key() == Qt::Key_End || e->matches(QKeySequence::Copy) || e->matches(QKeySequence::SelectAll))
        QTextBrowser::keyPressEvent(e);
    else { m_viewModel.returnFocusToInput(); e->accept(); }
}

void DisplayWidget::slot_displayText(const QStringView str) {
    auto on_bell = [this]() { m_viewModel.handleBell(); };
    foreach_char(mmqt::QC_ALERT, str, on_bell, [this](const QStringView s) { m_ansiTextHelper.displayText(s); });
    m_ansiTextHelper.limitScrollback(m_viewModel.lineLimit());
    if (verticalScrollBar()->sliderPosition() >= verticalScrollBar()->maximum() - 4)
        verticalScrollBar()->setSliderPosition(verticalScrollBar()->maximum());
}

void DisplayWidget::handleVisualBell() {
    if (auto *rf = document()->rootFrame()) {
        QTextFrameFormat ff = rf->frameFormat(); QColor c = m_viewModel.backgroundColor();
        c.setRed(std::min(255, c.red() + 80)); ff.setBackground(c); rf->setFrameFormat(ff);
        m_visualBellTimer->start(250);
    }
}

void setDefaultFormat(QTextCharFormat &f, const FontDefaults &d) {
    f.setFont(d.serverOutputFont); f.setBackground(d.defaultBg); f.setForeground(d.defaultFg);
    f.setFontWeight(QFont::Normal); f.setFontUnderline(false); f.setFontItalic(false); f.setFontStrikeOut(false);
}

void AnsiTextHelper::displayText(const QStringView input) {
    static const QRegularExpression ar(R"(\x1B[^A-Za-z\x1B]*[A-Za-z]?)"), ur(R"(https?:\/\/(www\.)?[-a-zA-Z0-9@:%._\+~#=]{1,256}\.[a-zA-Z0-9()]{1,6}\b([-a-zA-Z0-9()@:%_\+.~#?&//=]*))");
    auto add_raw = [this](const QStringView t, const QTextCharFormat &f) {
        if (cursor.block().text().endsWith(char16_t(0x08))) { cursor.deletePreviousChar(); if (cursor.block().length() > 0) cursor.deletePreviousChar(); }
        cursor.insertText(t.toString(), f);
    };
    mmqt::foreach_regex(ar, input, [this, &add_raw](auto a) {
        if (mmqt::isAnsiColor(a)) { if (auto n = mmqt::parseAnsiColor(currentAnsi, a)) currentAnsi = updateFormat(format, defaults, currentAnsi, *n); }
        else if (mmqt::isAnsiEraseLine(a)) { cursor.movePosition(QTextCursor::Left); cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor); cursor.removeSelectedText(); }
        else { add_raw(u"<ESC>", QTextCharFormat()); if (a.length() > 1) add_raw(a.mid(1), format); }
    }, [this, &add_raw](auto t) {
        foreach_char(char16_t(0x08), t, [this, &add_raw]() { if (cursor.position() > 0) add_raw(u"\x08", QTextCharFormat()); }, [this, &add_raw](auto nt) {
            mmqt::foreach_regex(ur, nt, [this](auto u) { cursor.insertHtml(QString(R"(<a href="%1" style="color: cyan; background-color: #003333;">%2</a>)").arg(QString::fromUtf8(QUrl::fromUserInput(u.toString()).toEncoded()), u.toString().toHtmlEscaped())); }, [this, &add_raw](auto nu) { add_raw(nu, format); });
        });
    });
}

void AnsiTextHelper::limitScrollback(int limit) {
    if (textEdit.document()->lineCount() > limit) {
        int trim = textEdit.document()->lineCount() - limit;
        cursor.movePosition(QTextCursor::Start); cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, trim);
        cursor.removeSelectedText(); cursor.movePosition(QTextCursor::End);
    }
}

static QColor decode(AnsiColorVariant v, QColor d, bool i) {
    if (v.hasRGB()) return QColor::fromRgb(v.getRGB().r, v.getRGB().g, v.getRGB().b);
    if (v.has256()) { auto c = v.get256().color; if (c < 8 && i) c += 8; return mmqt::ansi256toRgb(c); }
    return d;
}

RawAnsi updateFormat(QTextCharFormat &f, const FontDefaults &d, const RawAnsi &b, RawAnsi u) {
    if (b == u) return u;
    if (u == RawAnsi{}) { setDefaultFormat(f, d); return u; }
    auto df = b.getFlags() ^ u.getFlags();
    for (auto flag : df) {
        if (flag == AnsiStyleFlagEnum::Italic) f.setFontItalic(u.hasItalic());
        else if (flag == AnsiStyleFlagEnum::Underline) {
            f.setFontUnderline(u.hasUnderline());
            switch (u.getUnderlineStyle()) {
                case AnsiUnderlineStyleEnum::Dotted: f.setUnderlineStyle(QTextCharFormat::DotLine); break;
                case AnsiUnderlineStyleEnum::Curly: f.setUnderlineStyle(QTextCharFormat::WaveUnderline); break;
                case AnsiUnderlineStyleEnum::Dashed: f.setUnderlineStyle(QTextCharFormat::DashUnderline); break;
                default: f.setUnderlineStyle(QTextCharFormat::SingleUnderline); break;
            }
        }
        else if (flag == AnsiStyleFlagEnum::Strikeout) f.setFontStrikeOut(u.hasStrikeout());
        else if (flag == AnsiStyleFlagEnum::Bold || flag == AnsiStyleFlagEnum::Faint) f.setFontWeight(u.hasBold() ? QFont::Bold : (u.hasFaint() ? QFont::Light : QFont::Normal));
    }
    QColor bg = decode(u.bg, d.defaultBg, false), fg = decode(u.fg, d.defaultFg, u.hasBold()), ul = decode(u.ul, d.defaultUl.value_or(d.defaultFg), u.hasBold());
    if (u.hasReverse()) {
        auto rev = [](QColor &c) { c.setRgb(255-c.red(), 255-c.green(), 255-c.blue()); };
        rev(bg); rev(fg); rev(ul);
    }
    f.setBackground(bg); f.setForeground(fg); f.setUnderlineColor(ul); return u;
}

void setAnsiText(QTextEdit *p, std::string_view t) {
    p->clear(); p->setReadOnly(true); p->setOverwriteMode(true); p->setUndoRedoEnabled(false);
    p->document()->setUndoRedoEnabled(false); AnsiTextHelper h(*p); h.init(); h.displayText(mmqt::toQStringUtf8(t));
    if (p->verticalScrollBar()) p->verticalScrollBar()->setEnabled(true);
}
