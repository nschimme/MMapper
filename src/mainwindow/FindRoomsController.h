#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"
#include "../mapdata/roomselection.h"

#include <glm/vec2.hpp>

#include <QList>
#include <QObject>
#include <QString>
#include <QtCore>

class MapData;
class FindRoomsModel;

// Lifts the search/select/edit/activate operations FindRoomsDlg's slots
// perform (see findroomsdlg.cpp) into a QObject that can be driven from QML.
// Mirrors GroupController's role for GroupWidget: owns the FindRoomsModel and
// exposes the same signal surface FindRoomsDlg emitted, so MainWindow's
// existing slot connections (sig_center -> MapWindow::slot_centerOnWorldPos,
// sig_newRoomSelection -> MapCanvas::slot_setRoomSelection, sig_log ->
// MainWindow::slot_log, sig_editSelection -> MainWindow::slot_onEditRoomSelection)
// work unchanged against this controller.
class NODISCARD_QOBJECT FindRoomsController final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString resultSummary READ getResultSummary NOTIFY sig_resultSummaryChanged)

private:
    MapData &m_mapData;
    FindRoomsModel *m_model = nullptr;
    QString m_resultSummary;

public:
    explicit FindRoomsController(MapData &mapData, QObject *parent);

public:
    NODISCARD FindRoomsModel *getModel() { return m_model; }
    NODISCARD QString getResultSummary() const { return m_resultSummary; }

public:
    // kind is a PatternKindsEnum value (see mapdata/roomfilter.h); passed as
    // int so QML can select it from a ComboBox/RadioButton index without
    // needing the C++ enum registered.
    Q_INVOKABLE void find(const QString &text, int kind, bool caseSensitive, bool regex);
    // Row indices refer to rows in getModel().
    Q_INVOKABLE void selectRows(const QList<int> &rows);
    Q_INVOKABLE void editRows(const QList<int> &rows);
    Q_INVOKABLE void activateRow(int row);
    Q_INVOKABLE void clear();

signals:
    void sig_center(glm::vec2 worldPos); // connects to MapWindow
    void sig_newRoomSelection(const SigRoomSelection &);
    void sig_editSelection();
    void sig_log(const QString &, const QString &);
    void sig_error(const QString &);
    void sig_resultSummaryChanged();

private:
    void setResultSummary(QString text);
};
