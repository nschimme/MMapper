#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/Signal2.h"
#include "../global/macros.h"
#include "../observer/gameobserver.h"
#include "mumeclock.h"
#include "mumemoment.h"

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QWidget>

// QML-facing replacement for MumeClockWidget. Ports its update* logic (via
// the shared clockstrings:: helpers in ClockStrings.h) and its
// mousePressEvent()/updateStatusTips() behavior, but has no QWidget
// dependency, matching the always-compiled mmapper_SRCS pattern used by
// DescriptionAdapter/XpStatusAdapter.
class NODISCARD_QOBJECT ClockAdapter final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString moonText READ getMoonText NOTIFY sig_changed)
    Q_PROPERTY(QString moonTooltip READ getMoonTooltip NOTIFY sig_changed)
    Q_PROPERTY(bool moonVisible READ isMoonVisible NOTIFY sig_changed)
    // Mirrors MumeClockWidget::updateMoonVisibility()'s moonPhaseLabel background tint.
    Q_PROPERTY(QColor moonBgColor READ getMoonBgColor NOTIFY sig_changed)
    Q_PROPERTY(QString seasonText READ getSeasonText NOTIFY sig_changed)
    Q_PROPERTY(QString seasonTooltip READ getSeasonTooltip NOTIFY sig_changed)
    // Mirrors MumeClockWidget::updateSeason()'s seasonLabel colors.
    Q_PROPERTY(QColor seasonBgColor READ getSeasonBgColor NOTIFY sig_changed)
    Q_PROPERTY(QColor seasonFgColor READ getSeasonFgColor NOTIFY sig_changed)
    Q_PROPERTY(QString weatherText READ getWeatherText NOTIFY sig_changed)
    Q_PROPERTY(QString weatherTooltip READ getWeatherTooltip NOTIFY sig_changed)
    Q_PROPERTY(bool weatherVisible READ isWeatherVisible NOTIFY sig_changed)
    Q_PROPERTY(QString fogText READ getFogText NOTIFY sig_changed)
    Q_PROPERTY(QString fogTooltip READ getFogTooltip NOTIFY sig_changed)
    Q_PROPERTY(bool fogVisible READ isFogVisible NOTIFY sig_changed)
    Q_PROPERTY(QString countdownText READ getCountdownText NOTIFY sig_changed)
    Q_PROPERTY(QString countdownTooltip READ getCountdownTooltip NOTIFY sig_changed)
    // Mirrors MumeClockWidget::updateTime()'s timeLabel colors.
    Q_PROPERTY(QColor countdownBgColor READ getCountdownBgColor NOTIFY sig_changed)
    Q_PROPERTY(QColor countdownFgColor READ getCountdownFgColor NOTIFY sig_changed)
    // See MumeClockWidget::updateCountdown()'s "FIXME: Use ChangeMonitor"
    // comment: like the widget, this is only re-evaluated against
    // getConfig().mumeClock.display on the next tick, not the instant the
    // config changes (mumeClock has no ChangeMonitor wiring today).
    Q_PROPERTY(bool shown READ isShown NOTIFY sig_changed)

private:
    Signal2Lifetime m_lifetime;
    MumeClock &m_clock;
    // The QQuickWidget hosting ClockStrip.qml, used to translate the QML
    // scene coordinates showToolTip() receives into global screen
    // coordinates. Set post-construction (the widget doesn't exist yet when
    // MainWindow constructs this adapter); QPointer so a widget destroyed
    // out from under us just falls back to QCursor::pos() instead of
    // dangling.
    QPointer<QWidget> m_toolTipWidget;

    QString m_moonText;
    QString m_moonTooltip;
    bool m_moonVisible = false;
    QColor m_moonBgColor{Qt::gray};
    QString m_seasonText;
    QString m_seasonTooltip;
    QColor m_seasonBgColor{Qt::transparent};
    QColor m_seasonFgColor{Qt::black};
    QString m_weatherText;
    QString m_weatherTooltip;
    bool m_weatherVisible = false;
    QString m_fogText;
    QString m_fogTooltip;
    bool m_fogVisible = false;
    QString m_countdownText;
    QString m_countdownTooltip;
    QColor m_countdownBgColor{Qt::gray};
    QColor m_countdownFgColor{Qt::white};
    bool m_shown = false;

public:
    explicit ClockAdapter(GameObserver &observer, MumeClock &clock, QObject *parent);

public:
    NODISCARD QString getMoonText() const { return m_moonText; }
    NODISCARD QString getMoonTooltip() const { return m_moonTooltip; }
    NODISCARD bool isMoonVisible() const { return m_moonVisible; }
    NODISCARD QColor getMoonBgColor() const { return m_moonBgColor; }
    NODISCARD QString getSeasonText() const { return m_seasonText; }
    NODISCARD QString getSeasonTooltip() const { return m_seasonTooltip; }
    NODISCARD QColor getSeasonBgColor() const { return m_seasonBgColor; }
    NODISCARD QColor getSeasonFgColor() const { return m_seasonFgColor; }
    NODISCARD QString getWeatherText() const { return m_weatherText; }
    NODISCARD QString getWeatherTooltip() const { return m_weatherTooltip; }
    NODISCARD bool isWeatherVisible() const { return m_weatherVisible; }
    NODISCARD QString getFogText() const { return m_fogText; }
    NODISCARD QString getFogTooltip() const { return m_fogTooltip; }
    NODISCARD bool isFogVisible() const { return m_fogVisible; }
    NODISCARD QString getCountdownText() const { return m_countdownText; }
    NODISCARD QString getCountdownTooltip() const { return m_countdownTooltip; }
    NODISCARD QColor getCountdownBgColor() const { return m_countdownBgColor; }
    NODISCARD QColor getCountdownFgColor() const { return m_countdownFgColor; }
    NODISCARD bool isShown() const { return m_shown; }

public:
    // Must be called once after the QQuickWidget hosting ClockStrip.qml is
    // constructed (see MainWindow::setupStatusBar()); used by showToolTip()
    // to map QML scene coordinates to global screen coordinates.
    void setToolTipWidget(QWidget *widget) { m_toolTipWidget = widget; }

public:
    // QML calls this from a TapHandler; replicates
    // MumeClockWidget::mousePressEvent(): forces precision to MINUTE, resets
    // the last sync epoch to now, and recomputes the countdown/tooltips.
    Q_INVOKABLE void clicked();

    // ClockStrip.qml's HoverHandlers call these instead of using attached
    // QtQuick.Controls ToolTip properties: a QtQuick.Controls ToolTip is a
    // popup rendered *inside* the QQuickWidget's own (tiny, transparent)
    // scene, so on macOS it ends up clipped by the widget's bounds,
    // rendered transparently, or positioned relative to the wrong window --
    // effectively illegible. QToolTip is a native, top-level, OS-drawn
    // widget positioned at the screen cursor, so it renders correctly
    // regardless of the QQuickWidget's own size/opacity/backend.
    // x/y are the scene coordinates (relative to the hovered chip's
    // top-left) reported by ClockStrip.qml's HoverHandlers via
    // mapToItem(null, 0, 0); showToolTip() maps those to global screen
    // coordinates via m_toolTipWidget before handing off to QToolTip, so the
    // native popup lands next to the actual mouse position instead of at
    // the QQuickWidget's own top-left corner (see setToolTipWidget()).
    Q_INVOKABLE void showToolTip(const QString &text, qreal x, qreal y);
    Q_INVOKABLE void hideToolTip();

signals:
    void sig_changed();

private:
    void updateCountdown(const MumeMoment &moment);
    void updateTime(MumeTimeEnum time);
    void updateMoonPhase(MumeMoonPhaseEnum phase);
    void updateMoonVisibility(MumeMoonVisibilityEnum visibility);
    void updateSeason(MumeSeasonEnum season);
    void updateWeather(PromptWeatherEnum weather);
    void updateFog(PromptFogEnum fog);
    // Unlike MumeClockWidget (which only recomputes these lazily on
    // QEvent::HoverEnter, via its event() override, to avoid the string
    // formatting cost on every tick), this recomputes them on every tick:
    // QML tooltips are plain property reads with no equivalent "about to be
    // shown" hook, so staying always-fresh is simpler and the cost is
    // negligible.
    void updateStatusTips(const MumeMoment &moment);
};
