#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <functional>
#include <vector>

#include <QAbstractListModel>
#include <QString>

// QAbstractListModel façade over the five FixedPoint<D> rows in
// Configuration::CanvasSettings::Advanced (fov/verticalAngle/horizontalAngle/
// layerHeight/maximumFps; see ../configuration/configuration.h), mirroring
// AdvancedGraphicsGroupBox's SliderSpinboxButtonImpl rows (see
// AdvancedGraphics.h/.cpp) without any widgets. Each row is type-erased into
// a Row struct of get/set/reset callbacks so a single model can host
// FixedPoint<0> (maximumFps) and FixedPoint<1> (the other four) uniformly.
//
// NOTE: like AdvancedGraphicsGroupBox, this model takes persistent references
// into the live Configuration::canvas.advanced singleton; that object lives
// for the process lifetime, so this is safe.
class NODISCARD_QOBJECT AdvancedGraphicsModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum RoleEnum {
        NameRole = Qt::UserRole + 1,
        ValueRole,
        DisplayValueRole,
        MinRole,
        MaxRole,
        DigitsRole,
        Is3DOnlyRole
    };
    Q_ENUM(RoleEnum)

private:
    struct NODISCARD Row final
    {
        QString name;
        int min = 0;
        int max = 0;
        int digits = 0;
        bool is3DOnly = true;
        std::function<int()> get;
        std::function<void(int)> set;
        std::function<void()> reset;
    };
    std::vector<Row> m_rows;

public:
    explicit AdvancedGraphicsModel(QObject *parent);
    ~AdvancedGraphicsModel() final;

public:
    NODISCARD int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    NODISCARD QVariant data(const QModelIndex &index, int role) const override;
    NODISCARD QHash<int, QByteArray> roleNames() const override;

public:
    // Sets row's underlying FixedPoint to rawValue (already scaled by 10^digits);
    // mirrors SliderSpinboxButtonImpl's slider/spinbox handlers writing m_fp.set().
    Q_INVOKABLE void setValue(int row, int rawValue);
    // Resets row's underlying FixedPoint to its defaultValue; mirrors the
    // "Reset" QPushButton in SliderSpinboxButtonImpl.
    Q_INVOKABLE void reset(int row);

public slots:
    // Re-emits dataChanged for every row so QML resyncs against the live
    // Configuration; mirrors SliderSpinboxButtonImpl::forcedUpdate().
    void reload();

signals:
    void sig_graphicsSettingsChanged();
};
