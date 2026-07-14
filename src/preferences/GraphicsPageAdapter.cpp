// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "GraphicsPageAdapter.h"

#include "../configuration/configuration.h"
#include "../global/utils.h"
#include "../opengl/OpenGLConfig.h"
#include "AdvancedGraphicsModel.h"

#include <algorithm>

#include <QColorDialog>

GraphicsPageAdapter::GraphicsPageAdapter(QWidget *const dialogParent, QObject *const parent)
    : QObject(parent)
    , m_dialogParent(dialogParent)
    , m_advancedModel(new AdvancedGraphicsModel(this))
{
    connect(m_advancedModel,
            &AdvancedGraphicsModel::sig_graphicsSettingsChanged,
            this,
            &GraphicsPageAdapter::sig_graphicsSettingsChanged);

    // Mirrors GraphicsPage::slot_loadConfig()'s antialiasingSamplesComboBox
    // population: 0 ("Off"), then i *= 2 starting at 1, up to maxSamples.
    m_maxSamples = OpenGLConfig::getMaxSamples();
    for (int i = 0; i <= m_maxSamples; i *= 2) {
        m_antialiasingSamplesLabels.append(i != 0 ? QStringLiteral("%1x").arg(i) : tr("Off"));
        m_antialiasingSamplesValues.push_back(i);
        if (i == 0) {
            i = 1;
        }
    }
}

QColor GraphicsPageAdapter::getBackgroundColor() const
{
    return getConfig().canvas.backgroundColor.getColor().getQColor();
}

void GraphicsPageAdapter::setBackgroundColor(const QColor &value)
{
    setConfig().canvas.backgroundColor = Color(value);
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

QColor GraphicsPageAdapter::getRoomDarkColor() const
{
    return getConfig().canvas.roomDarkColor.getColor().getQColor();
}

void GraphicsPageAdapter::setRoomDarkColor(const QColor &value)
{
    setConfig().canvas.roomDarkColor = Color(value);
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

QColor GraphicsPageAdapter::getRoomDarkLitColor() const
{
    return getConfig().canvas.roomDarkLitColor.getColor().getQColor();
}

void GraphicsPageAdapter::setRoomDarkLitColor(const QColor &value)
{
    setConfig().canvas.roomDarkLitColor = Color(value);
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

QColor GraphicsPageAdapter::getConnectionNormalColor() const
{
    return getConfig().canvas.connectionNormalColor.getColor().getQColor();
}

void GraphicsPageAdapter::setConnectionNormalColor(const QColor &value)
{
    setConfig().canvas.connectionNormalColor = Color(value);
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

int GraphicsPageAdapter::getAntialiasingSamplesIndex() const
{
    const int samples = std::min(getConfig().canvas.antialiasingSamples.get(), m_maxSamples);
    const auto it = std::find(m_antialiasingSamplesValues.begin(),
                              m_antialiasingSamplesValues.end(),
                              samples);
    if (it == m_antialiasingSamplesValues.end()) {
        return 0;
    }
    return static_cast<int>(std::distance(m_antialiasingSamplesValues.begin(), it));
}

void GraphicsPageAdapter::setAntialiasingSamplesIndex(const int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_antialiasingSamplesValues.size()) {
        return;
    }
    setConfig().canvas.antialiasingSamples.set(
        m_antialiasingSamplesValues.at(static_cast<size_t>(index)));
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

bool GraphicsPageAdapter::getTrilinearFiltering() const
{
    return getConfig().canvas.trilinearFiltering.get();
}

void GraphicsPageAdapter::setTrilinearFiltering(const bool value)
{
    setConfig().canvas.trilinearFiltering.set(value);
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

bool GraphicsPageAdapter::getShowUnsavedChanges() const
{
    return getConfig().canvas.showUnsavedChanges.get();
}

void GraphicsPageAdapter::setShowUnsavedChanges(const bool value)
{
    setConfig().canvas.showUnsavedChanges.set(value);
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

bool GraphicsPageAdapter::getShowMissingMapId() const
{
    return getConfig().canvas.showMissingMapId.get();
}

void GraphicsPageAdapter::setShowMissingMapId(const bool value)
{
    setConfig().canvas.showMissingMapId.set(value);
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

bool GraphicsPageAdapter::getShowUnmappedExits() const
{
    return getConfig().canvas.showUnmappedExits.get();
}

void GraphicsPageAdapter::setShowUnmappedExits(const bool value)
{
    setConfig().canvas.showUnmappedExits.set(value);
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

bool GraphicsPageAdapter::getDrawDoorNames() const
{
    return getConfig().canvas.drawDoorNames;
}

void GraphicsPageAdapter::setDrawDoorNames(const bool value)
{
    setConfig().canvas.drawDoorNames = value;
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

bool GraphicsPageAdapter::getDrawUpperLayersTextured() const
{
    return getConfig().canvas.drawUpperLayersTextured;
}

void GraphicsPageAdapter::setDrawUpperLayersTextured(const bool value)
{
    setConfig().canvas.drawUpperLayersTextured = value;
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

int GraphicsPageAdapter::getWeatherAtmosphereIntensity() const
{
    return getConfig().canvas.weatherAtmosphereIntensity.get();
}

void GraphicsPageAdapter::setWeatherAtmosphereIntensity(const int value)
{
    setConfig().canvas.weatherAtmosphereIntensity.set(value);
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

int GraphicsPageAdapter::getWeatherPrecipitationIntensity() const
{
    return getConfig().canvas.weatherPrecipitationIntensity.get();
}

void GraphicsPageAdapter::setWeatherPrecipitationIntensity(const int value)
{
    setConfig().canvas.weatherPrecipitationIntensity.set(value);
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

int GraphicsPageAdapter::getWeatherTimeOfDayIntensity() const
{
    return getConfig().canvas.weatherTimeOfDayIntensity.get();
}

void GraphicsPageAdapter::setWeatherTimeOfDayIntensity(const int value)
{
    setConfig().canvas.weatherTimeOfDayIntensity.set(value);
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

// NOTE: MapCanvasConfig::isIn3dMode()/set3dMode()/isAutoTilt()/setAutoTilt()/
// getShowPerfStats()/setShowPerfStats() (see ../display/MapCanvasConfig.h)
// are trivial wrappers over getConfig().canvas.advanced.{use3D,autoTilt,
// printPerfStats} (see mapcanvas_gl.cpp), but are implemented in
// mapcanvas_gl.cpp, a heavy OpenGL/display translation unit this adapter
// should not need to link against. Read/write the underlying NamedConfig<bool>
// fields directly instead; behavior is identical.
bool GraphicsPageAdapter::getUse3D() const
{
    return getConfig().canvas.advanced.use3D.get();
}

void GraphicsPageAdapter::setUse3D(const bool value)
{
    setConfig().canvas.advanced.use3D.set(value);
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

bool GraphicsPageAdapter::getAutoTilt() const
{
    return getConfig().canvas.advanced.autoTilt.get();
}

void GraphicsPageAdapter::setAutoTilt(const bool value)
{
    setConfig().canvas.advanced.autoTilt.set(value);
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

bool GraphicsPageAdapter::getShowPerfStats() const
{
    return getConfig().canvas.advanced.printPerfStats.get();
}

void GraphicsPageAdapter::setShowPerfStats(const bool value)
{
    setConfig().canvas.advanced.printPerfStats.set(value);
    emit sig_changed();
    emit sig_graphicsSettingsChanged();
}

void GraphicsPageAdapter::chooseBackgroundColor()
{
    const QColor color = QColorDialog::getColor(getBackgroundColor(), m_dialogParent);
    if (color.isValid()) {
        setBackgroundColor(color);
    }
}

void GraphicsPageAdapter::chooseRoomDarkColor()
{
    const QColor color = QColorDialog::getColor(getRoomDarkColor(), m_dialogParent);
    if (color.isValid()) {
        setRoomDarkColor(color);
    }
}

void GraphicsPageAdapter::chooseRoomDarkLitColor()
{
    const QColor color = QColorDialog::getColor(getRoomDarkLitColor(), m_dialogParent);
    if (color.isValid()) {
        setRoomDarkLitColor(color);
    }
}

void GraphicsPageAdapter::chooseConnectionNormalColor()
{
    const QColor color = QColorDialog::getColor(getConnectionNormalColor(), m_dialogParent);
    if (color.isValid()) {
        setConnectionNormalColor(color);
    }
}

void GraphicsPageAdapter::reload()
{
    m_advancedModel->reload();
    emit sig_changed();
}
