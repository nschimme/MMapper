// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AdvancedGraphicsModel.h"

#include "../configuration/configuration.h"

#include <cmath>

AdvancedGraphicsModel::AdvancedGraphicsModel(QObject *const parent)
    : QAbstractListModel(parent)
{
    auto &advanced = setConfig().canvas.advanced;

    auto addRow = [this](const QString &name, auto &fp, bool is3DOnly) {
        using FP = std::decay_t<decltype(fp)>;
        Row row;
        row.name = name;
        row.min = fp.min;
        row.max = fp.max;
        row.digits = FP::digits;
        row.is3DOnly = is3DOnly;
        row.get = [&fp]() -> int { return fp.get(); };
        row.set = [&fp](int value) { fp.set(value); };
        row.reset = [&fp]() { fp.reset(); };
        m_rows.emplace_back(std::move(row));
    };

    addRow(tr("Field of View (fovy)"), advanced.fov, true);
    addRow(tr("Vertical Angle (pitch up from straight down)"), advanced.verticalAngle, true);
    addRow(tr("Horizontal Angle (yaw)"), advanced.horizontalAngle, true);
    addRow(tr("Layer height (in rooms)"), advanced.layerHeight, true);
    addRow(tr("Maximum FPS"), advanced.maximumFps, false);
}

AdvancedGraphicsModel::~AdvancedGraphicsModel() = default;

int AdvancedGraphicsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_rows.size());
}

QVariant AdvancedGraphicsModel::data(const QModelIndex &index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= m_rows.size()) {
        return {};
    }
    const Row &row = m_rows.at(static_cast<size_t>(index.row()));
    switch (role) {
    case NameRole:
        return row.name;
    case ValueRole:
        return row.get();
    case DisplayValueRole:
        return static_cast<double>(row.get()) / std::pow(10.0, row.digits);
    case MinRole:
        return row.min;
    case MaxRole:
        return row.max;
    case DigitsRole:
        return row.digits;
    case Is3DOnlyRole:
        return row.is3DOnly;
    default:
        return {};
    }
}

QHash<int, QByteArray> AdvancedGraphicsModel::roleNames() const
{
    return {{NameRole, "name"},
            {ValueRole, "value"},
            {DisplayValueRole, "displayValue"},
            {MinRole, "min"},
            {MaxRole, "max"},
            {DigitsRole, "digits"},
            {Is3DOnlyRole, "is3DOnly"}};
}

void AdvancedGraphicsModel::setValue(const int row, const int rawValue)
{
    if (row < 0 || static_cast<size_t>(row) >= m_rows.size()) {
        return;
    }
    m_rows.at(static_cast<size_t>(row)).set(rawValue);
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {ValueRole, DisplayValueRole});
    emit sig_graphicsSettingsChanged();
}

void AdvancedGraphicsModel::reset(const int row)
{
    if (row < 0 || static_cast<size_t>(row) >= m_rows.size()) {
        return;
    }
    m_rows.at(static_cast<size_t>(row)).reset();
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {ValueRole, DisplayValueRole});
    emit sig_graphicsSettingsChanged();
}

void AdvancedGraphicsModel::reload()
{
    if (m_rows.empty()) {
        return;
    }
    emit dataChanged(index(0, 0), index(static_cast<int>(m_rows.size()) - 1, 0));
}
