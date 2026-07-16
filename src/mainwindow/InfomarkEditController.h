#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"
#include "../map/infomark.h"

#include <memory>
#include <vector>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtCore>

class InfomarkSelection;
class MapData;

// Lifts the field-population/create/modify operations InfomarksEditDlg's
// slots perform (see infomarkseditdlg.cpp) into a QObject that can be driven
// from QML. Mirrors FindRoomsController's role for FindRoomsDlg: owns no
// widgets, exposes the widget's fields as Q_PROPERTYs, and its button clicks
// as Q_INVOKABLEs (see InfomarkEditDialog.qml).
//
// QML contract (stub-drift guard: TestQml.cpp's InfomarkEditControllerStub
// must keep this surface in sync):
//   Q_PROPERTY QStringList markerNames  -- index 0 is always "Create New
//     Marker", followed by one entry per marker in the selection (in
//     selection order).
//   Q_PROPERTY int currentIndex (WRITE setCurrentIndex) -- an index into
//     markerNames; writing it repopulates type/markerClass/text/x1/y1/x2/y2/
//     layer/angle/canModify from either the selected marker (index > 0) or
//     the selection's bounding box (index 0, "Create New Marker").
//   Q_PROPERTY int type (Q_INVOKABLE setType(int) to change it) -- an
//     InfomarkTypeEnum value. NOTE: mirrors a genuine widget quirk --
//     setType() only "sticks" when currentIndex is 0 (Create New Marker);
//     if a real marker is selected, the type/markerClass snap back to that
//     marker's stored values immediately (see refresh()), so the Type combo
//     is effectively read-only while modifying an existing marker.
//   Q_PROPERTY int markerClass (Q_INVOKABLE setMarkerClass(int)) -- an
//     InfomarkClassEnum value; unlike type, changing it never triggers a
//     refresh (matches the widget's objectClassesList, which has no
//     currentIndexChanged connection).
//   Q_PROPERTY QString text, int x1/y1/x2/y2/layer, int angle, bool
//     canModify -- read-only from QML's perspective; QML should copy them
//     into local editable fields whenever sig_refreshed() fires (on
//     setInfomarkSelection(), setCurrentIndex(), or setType()), not bind to
//     them continuously, since create()/modify() take the locally-edited
//     values as arguments instead.
//   Q_INVOKABLE create(...) / modify(...) -- port of updateMark() plus the
//     Add/UpdateInfomark change application; QML is responsible for
//     replicating updateMark()'s get_text() text-normalization lambda
//     locally before calling either (empty text + TEXT type becomes "New
//     Marker"; non-empty text + non-TEXT type becomes "").
//   signal sig_error(QString) -- replaces QMessageBox::warning(); QML shows
//     it as inline red text (see FindRoomsDialog.qml's precedent).
class NODISCARD_QOBJECT InfomarkEditController final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QStringList markerNames READ getMarkerNames NOTIFY sig_markerNamesChanged)
    Q_PROPERTY(
        int currentIndex READ getCurrentIndex WRITE setCurrentIndex NOTIFY sig_currentIndexChanged)
    Q_PROPERTY(int type READ getType NOTIFY sig_typeChanged)
    Q_PROPERTY(int markerClass READ getMarkerClass NOTIFY sig_markerClassChanged)
    Q_PROPERTY(QString text READ getText NOTIFY sig_textChanged)
    Q_PROPERTY(int x1 READ getX1 NOTIFY sig_x1Changed)
    Q_PROPERTY(int y1 READ getY1 NOTIFY sig_y1Changed)
    Q_PROPERTY(int x2 READ getX2 NOTIFY sig_x2Changed)
    Q_PROPERTY(int y2 READ getY2 NOTIFY sig_y2Changed)
    Q_PROPERTY(int layer READ getLayer NOTIFY sig_layerChanged)
    Q_PROPERTY(int angle READ getAngle NOTIFY sig_angleChanged)
    Q_PROPERTY(bool canModify READ getCanModify NOTIFY sig_canModifyChanged)

private:
    std::shared_ptr<InfomarkSelection> m_selection;
    std::vector<InfomarkId> m_markers;
    MapData *m_mapData = nullptr;

    QStringList m_markerNames;
    int m_currentIndex = 0;
    int m_type = 0;
    int m_markerClass = 0;
    QString m_text;
    int m_x1 = 0;
    int m_y1 = 0;
    int m_x2 = 0;
    int m_y2 = 0;
    int m_layer = 0;
    int m_angle = 0;
    bool m_canModify = false;

public:
    explicit InfomarkEditController(QObject *parent);

public:
    // NOTE: the selection is allowed to be null.
    void setInfomarkSelection(const std::shared_ptr<InfomarkSelection> &is, MapData *md);

public:
    NODISCARD QStringList getMarkerNames() const { return m_markerNames; }
    NODISCARD int getCurrentIndex() const { return m_currentIndex; }
    NODISCARD int getType() const { return m_type; }
    NODISCARD int getMarkerClass() const { return m_markerClass; }
    NODISCARD QString getText() const { return m_text; }
    NODISCARD int getX1() const { return m_x1; }
    NODISCARD int getY1() const { return m_y1; }
    NODISCARD int getX2() const { return m_x2; }
    NODISCARD int getY2() const { return m_y2; }
    NODISCARD int getLayer() const { return m_layer; }
    NODISCARD int getAngle() const { return m_angle; }
    NODISCARD bool getCanModify() const { return m_canModify; }

    void setCurrentIndex(int index);

    Q_INVOKABLE void setType(int type);
    Q_INVOKABLE void setMarkerClass(int cls);

    Q_INVOKABLE void create(
        int type, int cls, const QString &text, int x1, int y1, int x2, int y2, int layer, int angle);
    Q_INVOKABLE void modify(
        int type, int cls, const QString &text, int x1, int y1, int x2, int y2, int layer, int angle);

private:
    void updateMarkers();
    void refresh();
    NODISCARD InfomarkHandle getCurrentInfomark() const;

signals:
    void sig_markerNamesChanged();
    void sig_currentIndexChanged();
    void sig_typeChanged();
    void sig_markerClassChanged();
    void sig_textChanged();
    void sig_x1Changed();
    void sig_y1Changed();
    void sig_x2Changed();
    void sig_y2Changed();
    void sig_layerChanged();
    void sig_angleChanged();
    void sig_canModifyChanged();
    // Fired at the end of refresh() (i.e. after setInfomarkSelection(),
    // setCurrentIndex(), or setType()); QML should re-copy the properties
    // above into its local editable fields when this fires.
    void sig_refreshed();
    void sig_error(const QString &);
};
