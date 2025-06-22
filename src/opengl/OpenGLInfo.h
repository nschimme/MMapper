#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include <string>
// Forward declare mutex if only used in .cpp, or include if needed by inline getters (not the case here)

namespace OpenGLInfo {
    // For the highest reportable version determined at startup by OpenGL class
    void setHighestReportableVersionString(const std::string& version);
    NODISCARD std::string getHighestReportableVersionString();

    // For the actual running GL version string (set by MapCanvas or similar)
    void setCurrentRunningVersionString(const std::string& version);
    NODISCARD std::string getCurrentRunningVersionString();

} // namespace OpenGLInfo
