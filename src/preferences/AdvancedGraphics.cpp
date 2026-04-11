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
#include <type_traits>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

SliderSpinboxButton::~SliderSpinboxButton() = default;

template<int D>
class NODISCARD FpSlider final : public QSlider
{
private:
    FixedPoint<D> &m_fp;

public:
    explicit FpSlider(FixedPoint<D> &fp)
        : QSlider(Qt::Orientation::Horizontal)
        , m_fp{fp}
    {
        setRange(m_fp.min, m_fp.max);
        setValue(m_fp.get());
    }
    ~FpSlider() final = default;
};

template<int D>
class NODISCARD FpSpinBox final : public QDoubleSpinBox
{
private:
    using FP = FixedPoint<D>;
    FP &m_fp;
    const double m_multiplier;
    const double m_fraction;

public:
    explicit FpSpinBox(FixedPoint<D> &fp)
        : QDoubleSpinBox()
        , m_fp{fp}
        , m_multiplier{std::pow(10.0, static_cast<double>(FP::digits))}
        , m_fraction{1.0 / m_multiplier}
    {
        setRange(static_cast<double>(m_fp.min) * m_fraction,
                 static_cast<double>(m_fp.max) * m_fraction);
        setValue(m_fp.getDouble());
        setDecimals(FP::digits);
        setSingleStep(m_fraction);
    }
    ~FpSpinBox() final = default;

public:
    NODISCARD int getIntValue() const
    {
        return static_cast<int>(std::lround(std::clamp(value() * m_multiplier,
                                                       static_cast<double>(m_fp.min),
                                                       static_cast<double>(m_fp.max))));
    }
    void setIntValue(int value) { setValue(static_cast<double>(value) * m_fraction); }
};

template<int D>
class NODISCARD SliderSpinboxButtonImpl final : public SliderSpinboxButton
{
private:
    using FP = FixedPoint<D>;
    AdvancedGraphicsGroupBox &m_group;
    FP &m_fp;
    FpSlider<D> m_slider;
    FpSpinBox<D> m_spin;
    QPushButton m_reset;
    QHBoxLayout m_horizontal;

public:
    explicit SliderSpinboxButtonImpl(AdvancedGraphicsGroupBox &in_group,
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
        });

        QObject::connect(&m_spin,
                         QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                         &m_group,
                         [this](double /*value*/) {
                             const SignalBlocker block_slider{m_slider};
                             const SignalBlocker block_spin{m_spin};
                             const int value = m_spin.getIntValue();
                             m_fp.set(value);
                             m_slider.setValue(value);
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
    ~SliderSpinboxButtonImpl() override = default;

    void setEnabled(bool enabled) override
    {
        this->m_slider.setEnabled(enabled);
        this->m_spin.setEnabled(enabled);
        this->m_reset.setEnabled(enabled);
    }

    void forcedUpdate() override
    {
        const SignalBlocker block_slider{m_slider};
        const SignalBlocker block_spin{m_spin};

        const auto value = m_fp.get();
        m_spin.setIntValue(value);
        m_slider.setValue(value);
        if ((false)) {
        }
    }

    DELETE_CTORS_AND_ASSIGN_OPS(SliderSpinboxButtonImpl);
};

static void addLine(QLayout &layout)
{
    auto *const line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout.addWidget(line);
}

AdvancedGraphicsGroupBox::AdvancedGraphicsGroupBox(QGroupBox &groupBox, Configuration &config)
    : QObject{&groupBox}
    , m_groupBox{&groupBox}
    , m_config{config}
{
    auto *vertical = new QVBoxLayout(m_groupBox);

    auto makeSsb = [this, vertical](const QString &name, auto &fp, bool is3DOnly = true) {
        using FP = std::decay_t<decltype(fp)>;
        static_assert(std::is_same_v<FP, FixedPoint<FP::digits>>,
                      "makeSsb expects a FixedPoint<digits> argument");

        addLine(*vertical);
        auto ssb = std::make_unique<SliderSpinboxButtonImpl<FP::digits>>(*this, *vertical, name, fp);
        if (is3DOnly) {
            m_3dSsbs.emplace_back(std::move(ssb));
        } else {
            m_globalSsbs.emplace_back(std::move(ssb));
        }
    };

    auto *const checkboxDiag = new QCheckBox("Show Performance Stats");
    checkboxDiag->setObjectName("checkboxDiag");
    checkboxDiag->setChecked(m_config.canvas.advanced.printPerfStats.get());
    vertical->addWidget(checkboxDiag);

    auto *const checkbox3d = new QCheckBox("3d Mode");
    checkbox3d->setObjectName("checkbox3d");
    const bool is3dAtInit = m_config.canvas.advanced.use3D.get();
    checkbox3d->setChecked(is3dAtInit);
    vertical->addWidget(checkbox3d);

    auto *const autoTilt = new QCheckBox("Auto tilt with zoom");
    autoTilt->setObjectName("autoTilt");
    autoTilt->setChecked(m_config.canvas.advanced.autoTilt.get());
    vertical->addWidget(autoTilt);

    {
        auto &advanced = m_config.canvas.advanced;
        makeSsb("Field of View (fovy)", advanced.fov);
        makeSsb("Vertical Angle (pitch up from straight down)", advanced.verticalAngle);
        makeSsb("Horizontal Angle (yaw)", advanced.horizontalAngle);
        makeSsb("Layer height (in rooms)", advanced.layerHeight);
        makeSsb("Maximum FPS", advanced.maximumFps, false);
    }

    enableSsbs(is3dAtInit);
    autoTilt->setEnabled(is3dAtInit);

    m_groupBox->setLayout(vertical);

    connect(checkbox3d, &QCheckBox::toggled, this, [this, checkbox3d, autoTilt](bool) {
        const bool is3d = checkbox3d->isChecked();
        m_config.canvas.advanced.use3D.set(is3d);
        enableSsbs(is3d);
        autoTilt->setEnabled(is3d);
    });

    connect(autoTilt, &QCheckBox::toggled, this, [this, autoTilt](bool) {
        const bool val = autoTilt->isChecked();
        m_config.canvas.advanced.autoTilt.set(val);
    });

    connect(checkboxDiag, &QCheckBox::toggled, this, [this, checkboxDiag](bool) {
        const bool show = checkboxDiag->isChecked();
        m_config.canvas.advanced.printPerfStats.set(show);
    });

    m_config.canvas.advanced
        .registerChangeCallback(m_lifetime, [this, checkboxDiag, checkbox3d, autoTilt]() -> void {
            SignalBlocker sb1{*checkboxDiag};
            SignalBlocker sb2{*checkbox3d};
            SignalBlocker sb3{*autoTilt};
            for (auto &ssb : m_globalSsbs) {
                ssb->forcedUpdate();
            }
            for (auto &ssb : m_3dSsbs) {
                ssb->forcedUpdate();
            }
            checkboxDiag->setChecked(m_config.canvas.advanced.printPerfStats.get());
            checkbox3d->setChecked(m_config.canvas.advanced.use3D.get());
            autoTilt->setChecked(m_config.canvas.advanced.autoTilt.get());
        });
}

AdvancedGraphicsGroupBox::~AdvancedGraphicsGroupBox() = default;

void AdvancedGraphicsGroupBox::enableSsbs(bool enabled)
{
    for (auto &ssb : m_3dSsbs) {
        ssb->setEnabled(enabled);
    }
}
