#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QStringList>

class AdvancedGraphicsModel;
class QWidget;

// Q_PROPERTY façade over Configuration::CanvasSettings (see
// ../configuration/configuration.h), mirroring graphicspage.cpp's
// apply-on-change semantics. CanvasSettings has no ChangeMonitor, so
// reload() re-emits sig_changed unconditionally.
//
// Every setter also emits sig_graphicsSettingsChanged, which
// PreferencesController forwards as its own signal of the same name —
// mirroring GraphicsPage::sig_graphicsSettingsChanged -> ConfigDialog's relay
// (see configdialog.cpp) so MainWindow can re-render the canvas the same way
// it does for the widget dialog. AdvancedGraphicsModel's own
// sig_graphicsSettingsChanged is relayed the same way (mirrors
// AdvancedGraphicsGroupBox::sig_graphicsSettingsChanged ->
// GraphicsPage::slot_graphicsSettingsChanged in graphicspage.cpp).
//
// antialiasingSamplesLabels/antialiasingSamplesIndex mirror
// GraphicsPage::slot_loadConfig()'s antialiasingSamplesComboBox population
// (0, 1, 2, 4, 8, ... up to OpenGLConfig::getMaxSamples()), following the
// deviceNames/selectedDeviceIndex precedent set by AudioPageAdapter.
class NODISCARD_QOBJECT GraphicsPageAdapter final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(
        QColor backgroundColor READ getBackgroundColor WRITE setBackgroundColor NOTIFY sig_changed)
    Q_PROPERTY(QColor roomDarkColor READ getRoomDarkColor WRITE setRoomDarkColor NOTIFY sig_changed)
    Q_PROPERTY(QColor roomDarkLitColor READ getRoomDarkLitColor WRITE setRoomDarkLitColor NOTIFY
                   sig_changed)
    Q_PROPERTY(QColor connectionNormalColor READ getConnectionNormalColor WRITE
                   setConnectionNormalColor NOTIFY sig_changed)

    Q_PROPERTY(QStringList antialiasingSamplesLabels READ getAntialiasingSamplesLabels CONSTANT)
    Q_PROPERTY(int antialiasingSamplesIndex READ getAntialiasingSamplesIndex WRITE
                   setAntialiasingSamplesIndex NOTIFY sig_changed)

    Q_PROPERTY(bool trilinearFiltering READ getTrilinearFiltering WRITE setTrilinearFiltering NOTIFY
                   sig_changed)
    Q_PROPERTY(bool showUnsavedChanges READ getShowUnsavedChanges WRITE setShowUnsavedChanges NOTIFY
                   sig_changed)
    Q_PROPERTY(
        bool showMissingMapId READ getShowMissingMapId WRITE setShowMissingMapId NOTIFY sig_changed)
    Q_PROPERTY(bool showUnmappedExits READ getShowUnmappedExits WRITE setShowUnmappedExits NOTIFY
                   sig_changed)
    Q_PROPERTY(bool drawDoorNames READ getDrawDoorNames WRITE setDrawDoorNames NOTIFY sig_changed)
    Q_PROPERTY(bool drawUpperLayersTextured READ getDrawUpperLayersTextured WRITE
                   setDrawUpperLayersTextured NOTIFY sig_changed)

    Q_PROPERTY(int weatherAtmosphereIntensity READ getWeatherAtmosphereIntensity WRITE
                   setWeatherAtmosphereIntensity NOTIFY sig_changed)
    Q_PROPERTY(int weatherPrecipitationIntensity READ getWeatherPrecipitationIntensity WRITE
                   setWeatherPrecipitationIntensity NOTIFY sig_changed)
    Q_PROPERTY(int weatherTimeOfDayIntensity READ getWeatherTimeOfDayIntensity WRITE
                   setWeatherTimeOfDayIntensity NOTIFY sig_changed)

    Q_PROPERTY(bool use3D READ getUse3D WRITE setUse3D NOTIFY sig_changed)
    Q_PROPERTY(bool autoTilt READ getAutoTilt WRITE setAutoTilt NOTIFY sig_changed)
    Q_PROPERTY(bool showPerfStats READ getShowPerfStats WRITE setShowPerfStats NOTIFY sig_changed)

    Q_PROPERTY(AdvancedGraphicsModel *advancedModel READ getAdvancedModel CONSTANT)

private:
    // Parent for the native QColorDialog invoked by the chooseX() invokables;
    // owned by PreferencesController, not by this adapter.
    QPointer<QWidget> m_dialogParent;
    AdvancedGraphicsModel *m_advancedModel = nullptr;
    int m_maxSamples = 0;
    QStringList m_antialiasingSamplesLabels;
    std::vector<int> m_antialiasingSamplesValues;

public:
    explicit GraphicsPageAdapter(QWidget *dialogParent, QObject *parent);

public:
    NODISCARD QColor getBackgroundColor() const;
    void setBackgroundColor(const QColor &value);
    NODISCARD QColor getRoomDarkColor() const;
    void setRoomDarkColor(const QColor &value);
    NODISCARD QColor getRoomDarkLitColor() const;
    void setRoomDarkLitColor(const QColor &value);
    NODISCARD QColor getConnectionNormalColor() const;
    void setConnectionNormalColor(const QColor &value);

    NODISCARD QStringList getAntialiasingSamplesLabels() const
    {
        return m_antialiasingSamplesLabels;
    }
    NODISCARD int getAntialiasingSamplesIndex() const;
    void setAntialiasingSamplesIndex(int index);

    NODISCARD bool getTrilinearFiltering() const;
    void setTrilinearFiltering(bool value);
    NODISCARD bool getShowUnsavedChanges() const;
    void setShowUnsavedChanges(bool value);
    NODISCARD bool getShowMissingMapId() const;
    void setShowMissingMapId(bool value);
    NODISCARD bool getShowUnmappedExits() const;
    void setShowUnmappedExits(bool value);
    NODISCARD bool getDrawDoorNames() const;
    void setDrawDoorNames(bool value);
    NODISCARD bool getDrawUpperLayersTextured() const;
    void setDrawUpperLayersTextured(bool value);

    NODISCARD int getWeatherAtmosphereIntensity() const;
    void setWeatherAtmosphereIntensity(int value);
    NODISCARD int getWeatherPrecipitationIntensity() const;
    void setWeatherPrecipitationIntensity(int value);
    NODISCARD int getWeatherTimeOfDayIntensity() const;
    void setWeatherTimeOfDayIntensity(int value);

    NODISCARD bool getUse3D() const;
    void setUse3D(bool value);
    NODISCARD bool getAutoTilt() const;
    void setAutoTilt(bool value);
    NODISCARD bool getShowPerfStats() const;
    void setShowPerfStats(bool value);

    NODISCARD AdvancedGraphicsModel *getAdvancedModel() const { return m_advancedModel; }

public:
    // Native QColorDialog pickers, mirroring
    // GraphicsPage::changeColorClicked().
    Q_INVOKABLE void chooseBackgroundColor();
    Q_INVOKABLE void chooseRoomDarkColor();
    Q_INVOKABLE void chooseRoomDarkLitColor();
    Q_INVOKABLE void chooseConnectionNormalColor();

public slots:
    void reload();

signals:
    void sig_changed();
    void sig_graphicsSettingsChanged();
};
