// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "infomarkseditdlg.h"

#include "../configuration/configuration.h"
#include "../display/InfomarkSelection.h"
#include "../display/mapcanvas.h"
#include "../map/coordinate.h"
#include "../map/infomark.h"
#include "../mapdata/mapdata.h"

#include <cassert>
#include <memory>

#include <QString>
#include <QtWidgets>

InfomarksEditDlg::InfomarksEditDlg(QWidget *const parent)
    : QDialog(parent)
{
    setupUi(this);
    readSettings();

    connect(closeButton, &QAbstractButton::clicked, this, [this]() { this->accept(); });
}

void InfomarksEditDlg::setInfomarkSelection(const std::shared_ptr<InfomarkSelection> &is,
                                            MapData *const md,
                                            MapCanvas *const mc)
{
    // NOTE: the selection is allowed to be null.
    assert(md != nullptr);
    assert(mc != nullptr);

    // NOTE: We don't own these.
    m_selection = is;
    m_mapData = md;
    m_mapCanvas = mc;

    updateMarkers();
    updateDialog();
}

InfomarksEditDlg::~InfomarksEditDlg()
{
    writeSettings();
}

void InfomarksEditDlg::readSettings()
{
    restoreGeometry(getConfig().infomarksDialog.geometry);
}

void InfomarksEditDlg::writeSettings()
{
    setConfig().infomarksDialog.geometry = saveGeometry();
}

void InfomarksEditDlg::connectAll()
{
    std::ignore = deref(objectsList);
    std::ignore = deref(objectType);
    std::ignore = deref(objectCreate);
    std::ignore = deref(objectModify);

    disconnectAll();

    m_connections += connect(objectsList,
                             QOverload<int>::of(&QComboBox::currentIndexChanged),
                             this,
                             &InfomarksEditDlg::slot_objectListCurrentIndexChanged);
    m_connections += connect(objectType,
                             QOverload<int>::of(&QComboBox::currentIndexChanged),
                             this,
                             &InfomarksEditDlg::slot_objectTypeCurrentIndexChanged);
    m_connections += connect(objectCreate,
                             &QAbstractButton::clicked,
                             this,
                             &InfomarksEditDlg::slot_createClicked);
    m_connections += connect(objectModify,
                             &QAbstractButton::clicked,
                             this,
                             &InfomarksEditDlg::slot_modifyClicked);
}

void InfomarksEditDlg::slot_objectListCurrentIndexChanged(int /*unused*/)
{
    updateDialog();
}

void InfomarksEditDlg::slot_objectTypeCurrentIndexChanged(int /*unused*/)
{
    updateDialog();
}

void InfomarksEditDlg::slot_createClicked()
{
    auto &mapData = deref(m_mapData);
    RawInfomark im;
    updateMark(im);

    bool success = mapData.applySingleChange(Change{infomark_change_types::AddInfomark{im}});
    if (success) {
        updateMarkers();
        updateDialog();
    } else {
        QMessageBox::warning(this, "Error", "Failed to create infomark.");
    }
}

void InfomarksEditDlg::updateMark(RawInfomark &im)
{
    const Coordinate pos1(m_x1->value(), m_y1->value(), m_layer->value());
    const Coordinate pos2(m_x2->value(), m_y2->value(), m_layer->value());

    const int angle = static_cast<int>(std::lround(m_rotationAngle->value()));
    const InfomarkTypeEnum type = getType();

    auto get_text = [this, type]() -> InfomarkText {
        QString text = objectText->text();
        if (type == InfomarkTypeEnum::TEXT) {
            if (text.isEmpty()) {
                text = "New Marker";
                objectText->setText(text);
            }
        } else {
            if (!text.isEmpty()) {
                text = "";
                objectText->setText(text);
            }
        }
        // REVISIT: sanitize before calling objectText->setText(), or after?
        return mmqt::makeInfomarkText(text);
    };

    im.setType(type);
    im.setText(get_text());
    im.setClass(getClass());
    im.setPosition1(pos1);
    im.setPosition2(pos2);
    im.setRotationAngle(angle);
}

void InfomarksEditDlg::slot_modifyClicked()
{
    const InfomarkHandle &current = getCurrentInfomark();
    if (!current) {
        return;
    }

    RawInfomark mark = current.getRawCopy();
    updateMark(mark);

    if (!m_mapData->applySingleChange(
            Change{infomark_change_types::UpdateInfomark{current.getId(), mark}})) {
        QMessageBox::warning(this, "Error", "Failed to modify infomark.");
    }
}

void InfomarksEditDlg::disconnectAll()
{
    m_connections.disconnectAll();
}

void InfomarksEditDlg::updateMarkers()
{
    m_markers.clear();
    if (m_selection != nullptr) {
        m_markers.reserve(m_selection->size());
    }

    objectsList->clear();
    objectsList->addItem("Create New Marker", QVariant(-1));

    assert(m_selection);
    const InfomarkSelection &sel = deref(m_selection);

    // note: mutable lambda is reqquired for the "n" counter variable to be modified.
    sel.for_each([this, n = 0](const InfomarkHandle &marker) mutable {
        switch (marker.getType()) {
        case InfomarkTypeEnum::TEXT:
        case InfomarkTypeEnum::LINE:
        case InfomarkTypeEnum::ARROW:
            assert(m_markers.size() == static_cast<size_t>(n));
            m_markers.emplace_back(marker.getId());
            objectsList->addItem(marker.getText().toQString(), QVariant(n));
            ++n;
            break;
        }
    });

    if (m_selection->size() == 1) {
        objectsList->setCurrentIndex(1);
    }
}

void InfomarksEditDlg::updateDialog()
{
    class NODISCARD DisconnectReconnectAntiPattern final
    {
    private:
        InfomarksEditDlg &m_self;

    public:
        explicit DisconnectReconnectAntiPattern(InfomarksEditDlg &self)
            : m_self{self}
        {
            m_self.disconnectAll();
        }
        ~DisconnectReconnectAntiPattern() { m_self.connectAll(); }
    } antiPattern{*this};

    const InfomarkHandle &marker = getCurrentInfomark();
    if (marker) {
        objectType->setCurrentIndex(static_cast<int>(marker.getType()));
        objectClassesList->setCurrentIndex(static_cast<int>(marker.getClass()));
    }

    switch (getType()) {
    case InfomarkTypeEnum::TEXT:
        m_x2->setEnabled(false);
        m_y2->setEnabled(false);
        m_rotationAngle->setEnabled(true);
        objectText->setEnabled(true);
        break;
    case InfomarkTypeEnum::LINE:
        m_x2->setEnabled(true);
        m_y2->setEnabled(true);
        m_rotationAngle->setEnabled(false);
        objectText->setEnabled(false);
        break;
    case InfomarkTypeEnum::ARROW:
        m_x2->setEnabled(true);
        m_y2->setEnabled(true);
        m_rotationAngle->setEnabled(false);
        objectText->setEnabled(false);
        break;
    }

    if (!marker) {
        objectText->clear();
        m_x1->setValue(m_selection->getPosition1().x);
        m_y1->setValue(m_selection->getPosition1().y);
        m_x2->setValue(m_selection->getPosition2().x);
        m_y2->setValue(m_selection->getPosition2().y);
        m_rotationAngle->setValue(0.0);
        m_layer->setValue(m_selection->getPosition1().z);

        objectCreate->setEnabled(true);
        objectModify->setEnabled(false);
    } else {
        objectText->setText(marker.getText().toQString());
        m_x1->setValue(marker.getPosition1().x);
        m_y1->setValue(marker.getPosition1().y);
        m_x2->setValue(marker.getPosition2().x);
        m_y2->setValue(marker.getPosition2().y);
        m_rotationAngle->setValue(static_cast<double>(marker.getRotationAngle()));
        m_layer->setValue(marker.getPosition1().z);

        objectCreate->setEnabled(false);
        objectModify->setEnabled(true);
    }
}

InfomarkTypeEnum InfomarksEditDlg::getType()
{
    // FIXME: This needs bounds checking.
    return static_cast<InfomarkTypeEnum>(objectType->currentIndex());
}

InfomarkClassEnum InfomarksEditDlg::getClass()
{
    // FIXME: This needs bounds checking.
    return static_cast<InfomarkClassEnum>(objectClassesList->currentIndex());
}

InfomarkHandle InfomarksEditDlg::getCurrentInfomark()
{
    const auto &db = m_mapData->getCurrentMap().getInfomarkDb();
    bool ok = false;
    int n = objectsList->itemData(objectsList->currentIndex()).toInt(&ok);
    if (!ok || n == -1 || n >= static_cast<int>(m_markers.size())) {
        return InfomarkHandle{db, INVALID_INFOMARK_ID};
    }
    auto id = m_markers.at(static_cast<size_t>(n));
    return db.find(id);
}

void InfomarksEditDlg::setCurrentInfomark(InfomarkId id)
{
    int i = 0;
    for (size_t j = 0, len = m_markers.size(); j < len; ++j) {
        if (m_markers[j] == id) {
            i = static_cast<int>(j) + 1;
            break;
        }
    }

    objectsList->setCurrentIndex(i);
}
