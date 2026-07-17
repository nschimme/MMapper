#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "../global/macros.h"

#include <functional>

#include <QString>

class QWidget;

// The MMAPPER_WITH_QML replacement for preferences/AnsiColorDialog::getColor()
// (kept under NOT WITH_QML): constructs a QmlDialog hosting
// AnsiColorPickerDialog.qml, driven by a fresh AnsiColorPickerController
// (see ../mainwindow/AnsiColorPickerController.h), and invokes callback with
// the result ONLY on accept -- mirroring AnsiColorDialog::getColor() exactly.
//
// Declared in its own widget-free header (rather than as a static method on
// AnsiColorPickerController, which is compiled into TestMainWindow/TestQml
// without Qt6::Quick linked) so preferences/ParserPageAdapter.cpp -- also
// compiled directly into both test binaries -- never has to reference this
// symbol itself; MainWindow wires it in via ParserPageAdapter::setColorPicker()
// instead (see mainwindow.cpp's slot_onPreferences()).
namespace ansi_color_picker {
void getColor(const QString &ansiString, QWidget *parent, std::function<void(QString)> callback);
} // namespace ansi_color_picker
