#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "../global/macros.h"

#include <QtCore>

class QByteArray;
class QString;

namespace Patterns {
NODISCARD extern bool matchScore(const QString &str);
NODISCARD extern bool matchNoDescriptionPatterns(const QString &);
NODISCARD extern bool matchPattern(const QString &pattern, const QString &str);
} // namespace Patterns
