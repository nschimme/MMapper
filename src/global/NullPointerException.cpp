// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Ulf Hermann <ulfonk_mennhar@gmx.de> (Alve)
// Author: Marek Krejza <krejza@gmail.com> (Caligor)

#include "NullPointerException.h"

NullPointerException::NullPointerException()
    : runtime_error("NullPointerException")
{}
NullPointerException::~NullPointerException() = default;
