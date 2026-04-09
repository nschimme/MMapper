#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "../configuration/ConfigValue.h"
#include "../global/Color.h"
#include "../global/macros.h"
#include "ui_graphicspage.h"

#include <memory>
#include <vector>

#include <QString>
#include <QWidget>
#include <QtCore>

class AdvancedGraphicsGroupBox;
class QObject;
class XNamedColor;

namespace Ui {
class GraphicsPage;
}

class Configuration;

class NODISCARD_QOBJECT GraphicsPage final : public QWidget
{
    Q_OBJECT

public:
    explicit GraphicsPage(QWidget *parent, Configuration &config);
    ~GraphicsPage() final;

private:
    template<typename T>
    bool changeColorClicked(ConfigValue<T> &color)
    {
        T val = color.get();
        if (changeColorClickedImpl(val)) {
            color.set(val);
            return true;
        }
        return false;
    }
    bool changeColorClickedImpl(Color &color);
    Ui::GraphicsPage *const ui;
    Configuration &m_config;
    std::unique_ptr<AdvancedGraphicsGroupBox> m_advanced;

signals:

public slots:
    void slot_loadConfig();
    void slot_drawNeedsUpdateStateChanged(int);
    void slot_drawNotMappedExitsStateChanged(int);
    void slot_drawDoorNamesStateChanged(int);
    void slot_drawUpperLayersTexturedStateChanged(int);
    // this slot just calls the signal... not useful
};
