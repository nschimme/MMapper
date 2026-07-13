import QtQuick

// QML-facing replacement for MumeClockWidget. A row of small text chips
// (moon phase / season / weather / fog / countdown), each carrying the same
// background/foreground colors and tooltip the widget version set on its
// QLabels. Root implicit size collapses to 0 when the adapter says the clock
// is hidden (getConfig().mumeClock.display == false), same as
// XpStatusItem.qml, so QQuickWidget::SizeViewToRootObject lets the status bar
// reclaim the space.
Item {
    id: root

    // Context property expected to be set by C++ before this component is
    // instantiated: clock (ClockAdapter).

    visible: clock.shown
    implicitWidth: clock.shown ? row.implicitWidth : 0
    implicitHeight: clock.shown ? row.implicitHeight : 0

    Row {
        id: row
        spacing: 2
        leftPadding: 4
        rightPadding: 2

        Rectangle {
            width: moonLabel.implicitWidth + 4
            height: moonLabel.implicitHeight + 2
            color: clock.moonBgColor

            Text {
                id: moonLabel
                anchors.centerIn: parent
                text: clock.moonText
                color: "black"
            }

            HoverHandler {
                id: moonHover
                onHoveredChanged: {
                    if (hovered && clock.moonTooltip.length > 0) {
                        const p = parent.mapToItem(null, 0, 0);
                        clock.showToolTip(clock.moonTooltip, p.x, p.y);
                    } else {
                        clock.hideToolTip();
                    }
                }
            }
        }

        Rectangle {
            width: seasonLabel.implicitWidth + 4
            height: seasonLabel.implicitHeight + 2
            color: clock.seasonBgColor

            Text {
                id: seasonLabel
                anchors.centerIn: parent
                text: clock.seasonText
                color: clock.seasonFgColor
            }

            HoverHandler {
                id: seasonHover
                onHoveredChanged: {
                    if (hovered && clock.seasonTooltip.length > 0) {
                        const p = parent.mapToItem(null, 0, 0);
                        clock.showToolTip(clock.seasonTooltip, p.x, p.y);
                    } else {
                        clock.hideToolTip();
                    }
                }
            }
        }

        Text {
            visible: clock.weatherVisible
            text: clock.weatherText

            HoverHandler {
                id: weatherHover
                onHoveredChanged: {
                    if (hovered && clock.weatherTooltip.length > 0) {
                        const p = parent.mapToItem(null, 0, 0);
                        clock.showToolTip(clock.weatherTooltip, p.x, p.y);
                    } else {
                        clock.hideToolTip();
                    }
                }
            }
        }

        Text {
            visible: clock.fogVisible
            text: clock.fogText

            HoverHandler {
                id: fogHover
                onHoveredChanged: {
                    if (hovered && clock.fogTooltip.length > 0) {
                        const p = parent.mapToItem(null, 0, 0);
                        clock.showToolTip(clock.fogTooltip, p.x, p.y);
                    } else {
                        clock.hideToolTip();
                    }
                }
            }
        }

        Rectangle {
            width: Math.max(40, countdownLabel.implicitWidth + 4)
            height: countdownLabel.implicitHeight + 2
            color: clock.countdownBgColor

            Text {
                id: countdownLabel
                anchors.centerIn: parent
                text: clock.countdownText
                color: clock.countdownFgColor
            }

            HoverHandler {
                id: countdownHover
                onHoveredChanged: {
                    if (hovered && clock.countdownTooltip.length > 0) {
                        const p = parent.mapToItem(null, 0, 0);
                        clock.showToolTip(clock.countdownTooltip, p.x, p.y);
                    } else {
                        clock.hideToolTip();
                    }
                }
            }
        }
    }

    // Clicking anywhere on the strip forces a re-sync, mirroring
    // MumeClockWidget::mousePressEvent() (which fires regardless of which
    // child label was clicked, since the whole widget owns the handler).
    TapHandler {
        onTapped: clock.clicked()
    }
}
