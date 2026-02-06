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
#include <QDoubleSpinBox>
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
    ~FpSlider() final = default;
};

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
    ~FpSpinBox() final = default;

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
        QObject::connect(&m_slider, &QSlider::valueChanged, &m_group, [this](int value) {
            SignalBlocker block_spin{m_spin};
            m_fp.set(value);
            m_spin.setIntValue(value);
            emit m_group.sig_graphicsSettingsChanged();
        });

        QObject::connect(&m_spin,
                         QOverload<double>::of(&FpSpinBox::valueChanged),
                         &m_group,
                         [this](double) {
                             SignalBlocker block_slider{m_slider};
                             const int value = m_spin.getIntValue();
                             m_fp.set(value);
                             m_slider.setValue(value);
                             emit m_group.sig_graphicsSettingsChanged();
                         });

        QObject::connect(&m_reset, &QPushButton::clicked, &m_group, [this](bool) {
            m_slider.setValue(m_fp.defaultValue);
        });

        vbox.addWidget(new QLabel(name));
        m_horizontal.addSpacing(20);
        m_horizontal.addWidget(&m_slider);
        m_horizontal.addWidget(&m_spin);
        m_horizontal.addWidget(&m_reset);
        vbox.addLayout(&m_horizontal, 0);
    }

    void setEnabled(bool enabled)
    {
        m_slider.setEnabled(enabled);
        m_spin.setEnabled(enabled);
        m_reset.setEnabled(enabled);
    }

    void forcedUpdate()
    {
        SignalBlocker block_slider{m_slider};
        SignalBlocker block_spin{m_spin};
        const auto value = m_fp.get();
        m_spin.setIntValue(value);
        m_slider.setValue(value);
    }
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
    , m_checkboxDiag(new QCheckBox("Show Performance Stats"))
    , m_checkbox3d(new QCheckBox("3d Mode"))
    , m_autoTilt(new QCheckBox("Auto tilt with zoom"))
{
    auto *vertical = new QVBoxLayout(m_groupBox);

    vertical->addWidget(m_checkboxDiag);
    vertical->addWidget(m_checkbox3d);
    vertical->addWidget(m_autoTilt);

    auto makeSsb = [this, vertical](const QString &name, FixedPoint<1> &fp) {
        addLine(*vertical);
        m_ssbs.emplace_back(std::make_unique<SliderSpinboxButton>(*this, *vertical, name, fp));
    };

    auto &advanced = setConfig().canvas.advanced;
    makeSsb("Field of View (fovy)", advanced.fov);
    makeSsb("Vertical Angle (pitch up from straight down)", advanced.verticalAngle);
    makeSsb("Horizontal Angle (yaw)", advanced.horizontalAngle);
    makeSsb("Layer height (in rooms)", advanced.layerHeight);

    connect(&m_viewModel, &AdvancedGraphicsViewModel::settingsChanged, this, &AdvancedGraphicsGroupBox::updateUI);

    connect(m_checkboxDiag, &QCheckBox::toggled, &m_viewModel, &AdvancedGraphicsViewModel::setShowPerfStats);
    connect(m_checkbox3d, &QCheckBox::toggled, &m_viewModel, &AdvancedGraphicsViewModel::setMode3d);
    connect(m_autoTilt, &QCheckBox::toggled, &m_viewModel, &AdvancedGraphicsViewModel::setAutoTilt);

    connect(&m_viewModel, &AdvancedGraphicsViewModel::settingsChanged, this, &AdvancedGraphicsGroupBox::sig_graphicsSettingsChanged);

    updateUI();
}

AdvancedGraphicsGroupBox::~AdvancedGraphicsGroupBox() = default;

void AdvancedGraphicsGroupBox::updateUI()
{
    SignalBlocker sb1(*m_checkboxDiag);
    SignalBlocker sb2(*m_checkbox3d);
    SignalBlocker sb3(*m_autoTilt);

    m_checkboxDiag->setChecked(m_viewModel.showPerfStats());
    m_checkbox3d->setChecked(m_viewModel.mode3d());
    m_autoTilt->setChecked(m_viewModel.autoTilt());

    for (auto &ssb : m_ssbs) {
        ssb->setEnabled(m_viewModel.mode3d());
        ssb->forcedUpdate();
    }
    m_autoTilt->setEnabled(m_viewModel.mode3d());
}
