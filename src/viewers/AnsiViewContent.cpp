// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 The MMapper Authors

#include "AnsiViewContent.h"

#include "../configuration/configuration.h"
#include "../global/AnsiHtml.h"
#include "../global/TextUtils.h"
#include "../global/utils.h"

AnsiViewContent::AnsiViewContent(const QString &program,
                                 const QString &title,
                                 const std::string_view message,
                                 QObject *const parent)
    : QObject(parent)
{
    // Mirrors mmqt::setWindowTitle2() (see global/window_utils.cpp), which
    // AnsiViewWindow's constructor calls directly on itself; duplicated here
    // (rather than shared) because that helper takes a QWidget&, and this
    // class is deliberately widget-free.
    static const bool programFirst = utils::getEnvBool("MMAPPER_WINDOW_TITLE_PROGRAM_FIRST")
                                         .value_or(false);
    m_title = programFirst ? QString("%1 - %2").arg(program, title)
                           : QString("%1 - %2").arg(title, program);

    const auto &settings = getConfig().integratedClient;
    m_html = mmqt::ansiToHtml(mmqt::toQStringUtf8(message),
                              settings.foregroundColor,
                              settings.backgroundColor);
}
