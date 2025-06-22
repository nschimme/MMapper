// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2023 The MMapper Authors

#include "OpenGLInfo.h"
#include "../global/logging.h" // For MMLOG_INFO for logging initial set is good practice

#include <mutex>
#include <string>

namespace OpenGLInfo {

    static std::mutex g_highest_reportable_version_lock;
    static std::string g_highest_reportable_version_string = "GL 2.1 Compat (Default)";

    static std::mutex g_current_running_version_lock;
    static std::string g_current_running_version_string = "GL 2.1 Compat (Default)";

    void setHighestReportableVersionString(const std::string& version) {
        std::unique_lock<std::mutex> lock{g_highest_reportable_version_lock};
        g_highest_reportable_version_string = version;
        MMLOG_INFO() << "[OpenGLInfo] Highest reportable GL version set to: " << g_highest_reportable_version_string.c_str();
    }

    std::string getHighestReportableVersionString() {
        std::unique_lock<std::mutex> lock{g_highest_reportable_version_lock};
        return g_highest_reportable_version_string;
    }

    void setCurrentRunningVersionString(const std::string& version) {
        std::unique_lock<std::mutex> lock{g_current_running_version_lock};
        g_current_running_version_string = version;
        MMLOG_INFO() << "[OpenGLInfo] Current running GL version set to: " << g_current_running_version_string.c_str();
    }

    std::string getCurrentRunningVersionString() {
        std::unique_lock<std::mutex> lock{g_current_running_version_lock};
        return g_current_running_version_string;
    }

} // namespace OpenGLInfo
