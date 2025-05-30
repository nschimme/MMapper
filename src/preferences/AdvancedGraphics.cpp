// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors

#include "AdvancedGraphics.h"

#include "../configuration/configuration.h"
#include "../display/MapCanvasConfig.h"
#include "../global/FixedPoint.h"
#include "../global/RuleOf5.h"
#include "../global/SignalBlocker.h"
#include "../global/utils.h"

#include <cassert>
#include <memory>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

class NODISCARD FpSlider final : public QSlider
{
private:
    FixedPoint<1> &m_fp;

public:
    explicit FpSlider(FixedPoint<1> &fp)
        : QSlider(Qt::Orientation::Horizontal)
        , m_fp{fp}
    {
        setRange(m_fp.min, m_fp.max);
        setValue(m_fp.get());
    }
    ~FpSlider() final;
};

FpSlider::~FpSlider() = default;

class NODISCARD FpSpinBox final : public QDoubleSpinBox
{
private:
    using FP = FixedPoint<1>;
    FP &m_fp;

public:
    explicit FpSpinBox(FixedPoint<1> &fp)
        : QDoubleSpinBox()
        , m_fp{fp}
    {
        const double fraction = std::pow(10.0, -FP::digits);
        setRange(static_cast<double>(m_fp.min) * fraction, static_cast<double>(m_fp.max) * fraction);
        setValue(m_fp.getDouble());
        setDecimals(FP::digits);
        setSingleStep(fraction);
    }
    ~FpSpinBox() final;

public:
    NODISCARD int getIntValue() const
    {
        return static_cast<int>(std::lround(std::clamp(value() * std::pow(10.0, FP::digits),
                                                       static_cast<double>(m_fp.min),
                                                       static_cast<double>(m_fp.max))));
    }
    void setIntValue(int value)
    {
        setValue(static_cast<double>(value) * std::pow(10.0, -FP::digits));
    }
};

FpSpinBox::~FpSpinBox() = default;

class NODISCARD SliderSpinboxButton final
{
private:
    using FP = FixedPoint<1>;
    AdvancedGraphicsGroupBox &m_group;
    FP &m_fp;
    FpSlider m_slider;
    FpSpinBox m_spin;
    QPushButton m_reset;
    QHBoxLayout m_horizontal;
    QVector<QWidget *> m_blockable;

public:
    explicit SliderSpinboxButton(AdvancedGraphicsGroupBox &in_group,
                                 QVBoxLayout &vbox,
                                 const QString &name,
                                 FP &in_fp)
        : m_group{in_group}
        , m_fp{in_fp}
        , m_slider(m_fp)
        , m_spin(m_fp)
        , m_reset("Reset")
    {
        auto &group = m_group;
        QObject::connect(&m_slider, &QSlider::valueChanged, &group, [this](int value) {
            const SignalBlocker block_slider{m_slider};
            const SignalBlocker block_spin{m_spin};
            m_fp.set(value);
            m_spin.setIntValue(value);
            m_group.graphicsSettingsChanged();
        });

        QObject::connect(&m_spin,
                         QOverload<double>::of(&FpSpinBox::valueChanged),
                         &group,
                         [this](double /*value*/) {
                             const SignalBlocker block_slider{m_slider};
                             const SignalBlocker block_spin{m_spin};
                             const int value = m_spin.getIntValue();
                             m_fp.set(value);
                             m_slider.setValue(value);
                             m_group.graphicsSettingsChanged();
                         });

        QObject::connect(&m_reset, &QPushButton::clicked, &group, [this](bool) {
            m_slider.setValue(m_fp.defaultValue);
        });

        vbox.addWidget(new QLabel(name));

        auto &horizontal = m_horizontal;
        horizontal.addSpacing(20);
        horizontal.addWidget(&m_slider);
        horizontal.addWidget(&m_spin);
        horizontal.addWidget(&m_reset);
        vbox.addLayout(&horizontal, 0);
    }
    ~SliderSpinboxButton() = default;

    void setEnabled(bool enabled)
    {
        this->m_slider.setEnabled(enabled);
        this->m_spin.setEnabled(enabled);
        this->m_reset.setEnabled(enabled);
    }

    void forcedUpdate()
    {
        const SignalBlocker block_slider{m_slider};
        const SignalBlocker block_spin{m_spin};

        const auto value = m_fp.get();
        m_spin.setIntValue(value);
        m_slider.setValue(value);
        if ((false)) {
            m_group.graphicsSettingsChanged();
        }
    }

    DELETE_CTORS_AND_ASSIGN_OPS(SliderSpinboxButton);
};

static void addLine(QLayout &layout)
{
    auto *const line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout.addWidget(line);
}

AdvancedGraphicsGroupBox::AdvancedGraphicsGroupBox(QGroupBox &groupBox)
    : QObject{&groupBox}
    , m_groupBox{&groupBox}
{
    auto *vertical = new QVBoxLayout(m_groupBox);

    using FP = FixedPoint<1>;

    auto makeSsb = [this, vertical](const QString name, FP &fp) {
        addLine(*vertical);
        m_ssbs.emplace_back(std::make_unique<SliderSpinboxButton>(*this, *vertical, name, fp));
    };

    auto *const checkboxDiag = new QCheckBox("Show Performance Stats");
    checkboxDiag->setChecked(MapCanvasConfig::getShowPerfStats());
    vertical->addWidget(checkboxDiag);

    auto *const checkbox3d = new QCheckBox("3d Mode");
    const bool is3dAtInit = MapCanvasConfig::isIn3dMode();
    checkbox3d->setChecked(is3dAtInit);
    vertical->addWidget(checkbox3d);

    auto *const autoTilt = new QCheckBox("Auto tilt with zoom");
    autoTilt->setChecked(MapCanvasConfig::isAutoTilt());
    vertical->addWidget(autoTilt);

    {
        // NOTE: This is a slight abuse of the interface, because we're taking a persistent reference.
        auto &advanced = setConfig().canvas.advanced;
        makeSsb("Field of View (fovy)", advanced.fov);
        makeSsb("Vertical Angle (pitch up from straight down)", advanced.verticalAngle);
        makeSsb("Horizontal Angle (yaw)", advanced.horizontalAngle);
        makeSsb("Layer height (in rooms)", advanced.layerHeight);
    }

    enableSsbs(is3dAtInit);
    autoTilt->setEnabled(is3dAtInit);

    m_groupBox->setLayout(vertical);

    connect(checkbox3d, &QCheckBox::stateChanged, this, [this, checkbox3d, autoTilt](int) {
        const bool is3d = checkbox3d->isChecked();
        MapCanvasConfig::set3dMode(is3d);
        enableSsbs(is3d);
        autoTilt->setEnabled(is3d);
        graphicsSettingsChanged();
    });

    connect(autoTilt, &QCheckBox::stateChanged, this, [this, autoTilt](int) {
        const bool val = autoTilt->isChecked();
        MapCanvasConfig::setAutoTilt(val);
        graphicsSettingsChanged();
    });

    connect(checkboxDiag, &QCheckBox::stateChanged, this, [this, checkboxDiag](int) {
        const bool show = checkboxDiag->isChecked();
        MapCanvasConfig::setShowPerfStats(show);
        graphicsSettingsChanged();
    });

    MapCanvasConfig::registerChangeCallback(m_lifetime,
                                            [this, checkboxDiag, checkbox3d, autoTilt]() -> void {
                                                SignalBlocker sb1{*checkboxDiag};
                                                SignalBlocker sb2{*checkbox3d};
                                                SignalBlocker sb3{*autoTilt};
                                                for (auto &ssb : m_ssbs) {
                                                    ssb->forcedUpdate();
                                                }
                                                checkboxDiag->setChecked(
                                                    MapCanvasConfig::getShowPerfStats());
                                                checkbox3d->setChecked(
                                                    MapCanvasConfig::isIn3dMode());
                                                autoTilt->setChecked(MapCanvasConfig::isAutoTilt());
                                            });
}

AdvancedGraphicsGroupBox::~AdvancedGraphicsGroupBox() = default;

void AdvancedGraphicsGroupBox::enableSsbs(bool enabled)
{
    for (auto &ssb : m_ssbs) {
        ssb->setEnabled(enabled);
    }
}
