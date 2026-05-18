// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Nils Schimmelmann <nschimme@gmail.com> (Jahara)

#include "io.h"

#include <cerrno>
#include <cstring>

#ifndef Q_OS_WIN
#include <unistd.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

#ifdef Q_OS_MAC
#include <fcntl.h>
#endif

#ifdef Q_OS_WIN
#include "WinSock.h"

#include <io.h>
#include <windows.h>
#endif

namespace io {

ErrorNumberMessage::ErrorNumberMessage(const int error_number) noexcept
    : m_error_number{error_number}
{
#ifdef Q_OS_WIN
    LPWSTR messageBuffer = nullptr;
    const DWORD size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
                                          | FORMAT_MESSAGE_IGNORE_INSERTS,
                                      nullptr,
                                      static_cast<DWORD>(error_number),
                                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                      reinterpret_cast<LPWSTR>(&messageBuffer),
                                      0,
                                      nullptr);
    if (size > 0) {
        const int res = WideCharToMultiByte(CP_UTF8,
                                            0,
                                            messageBuffer,
                                            static_cast<int>(size),
                                            m_buf,
                                            static_cast<int>(sizeof(m_buf)) - 1,
                                            nullptr,
                                            nullptr);
        if (res > 0) {
            m_buf[res] = '\0';
            // Trim trailing newlines and spaces
            int len = res;
            while (len > 0
                   && (m_buf[len - 1] == '\r' || m_buf[len - 1] == '\n' || m_buf[len - 1] == ' '
                       || m_buf[len - 1] == '\t' || m_buf[len - 1] == '.')) {
                m_buf[--len] = '\0';
            }
            m_str = m_buf;
        }
        LocalFree(messageBuffer);
    }
#elif defined(__GLIBC__)
    /* GNU/Linux version can return a pointer to a static string */
    m_str = ::strerror_r(error_number, m_buf, sizeof(m_buf));
#else
    /* XSI-compliant/BSD version version returns 0 on success */
    if (::strerror_r(m_error_number, m_buf, sizeof(m_buf)) == 0) {
        m_str = m_buf;
    }
#endif
}

namespace { // anonymous
NODISCARD std::string getFriendlyMessage(const int error_number)
{
#ifdef Q_OS_WIN
    switch (error_number) {
    case ERROR_ACCESS_DENIED:
        return "Access denied. You might not have permission to write to this location.";
    case ERROR_SHARING_VIOLATION:
        return "The file is being used by another process.";
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return "The system cannot find the file or path specified.";
    case ERROR_DISK_FULL:
        return "The disk is full.";
    case ERROR_WRITE_FAULT:
        return "The system could not write to the specified device.";
    default:
        return "";
    }
#else
    switch (error_number) {
    case EACCES:
        return "Permission denied.";
    case ENOENT:
        return "No such file or directory.";
    case ENOSPC:
        return "No space left on device.";
    case EBUSY:
        return "Resource busy.";
    case EROFS:
        return "Read-only file system.";
    default:
        return "";
    }
#endif
}
} // namespace

IOException IOException::withErrorNumber(const int error_number)
{
    const std::string friendly = getFriendlyMessage(error_number);
    if (const auto msg = ErrorNumberMessage{error_number}) {
        const std::string sysMsg = msg.getErrorMessage();
        if (friendly.empty()) {
            return IOException{sysMsg};
        }
        if (friendly == sysMsg) {
            return IOException{friendly};
        }
        return IOException{friendly + " (" + sysMsg + ")"};
    }

    const std::string unknown = "unknown error code " + std::to_string(error_number);
    if (!friendly.empty()) {
        return IOException{friendly + " (" + unknown + ")"};
    }
    return IOException{unknown};
}

IOException IOException::withCurrentErrno()
{
    return withErrorNumber(errno);
}

IOException::~IOException() = default;

bool fsync(QFile &file) CAN_THROW
{
    const int handle = file.handle();
#ifdef Q_OS_WIN
    if (::FlushFileBuffers(reinterpret_cast<HANDLE>(::_get_osfhandle(handle))) == 0) {
        throw IOException::withErrorNumber(static_cast<int>(::GetLastError()));
    }
#elif defined(Q_OS_MAC)
    if (::fcntl(handle, F_FULLFSYNC) == -1) {
        throw IOException::withCurrentErrno();
    }
#else
    if (::fsync(handle) == -1) {
        throw IOException::withCurrentErrno();
    }
#endif
    return true;
}

void rename(const QString &from, const QString &to) CAN_THROW
{
#ifdef Q_OS_WIN
    const std::wstring fromW = from.toStdWString();
    const std::wstring toW = to.toStdWString();
    if (::ReplaceFileW(toW.c_str(),
                       fromW.c_str(),
                       nullptr,
                       REPLACEFILE_IGNORE_MERGE_ERRORS,
                       nullptr,
                       nullptr)
        == 0) {
        const auto err = ::GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            if (::MoveFileExW(fromW.c_str(),
                              toW.c_str(),
                              MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)
                == 0) {
                throw IOException::withErrorNumber(static_cast<int>(::GetLastError()));
            }
        } else {
            throw IOException::withErrorNumber(static_cast<int>(err));
        }
    }
#else
    const auto fromEncoded = QFile::encodeName(from);
    const auto toEncoded = QFile::encodeName(to);
    if (::rename(fromEncoded.data(), toEncoded.data()) == -1) {
        throw IOException::withCurrentErrno();
    }
#endif
}

IOResultEnum fsyncNoexcept(QFile &file) noexcept
{
    try {
        return fsync(file) ? IOResultEnum::SUCCESS : IOResultEnum::FAILURE;
    } catch (...) {
        return IOResultEnum::EXCEPTION;
    }
}

bool tuneKeepAlive(qintptr socketDescriptor, int maxIdle, int count, int interval)
{
#ifdef Q_OS_WIN
    const unsigned int socket = static_cast<unsigned int>(socketDescriptor);
    const auto maxIdleInMillis = static_cast<unsigned long>(maxIdle * 1000);
    const auto intervalInMillis = static_cast<unsigned long>(interval * 1000);
    return WinSock::tuneKeepAlive(socket, maxIdleInMillis, intervalInMillis);
#else
    // Enable TCP keepalive
    const int fd = static_cast<int>(socketDescriptor);
    int optVal = 1;
    int ret = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optVal, sizeof(optVal));
    if (ret == -1) {
        qWarning() << "setsockopt(SO_KEEPALIVE) failed with" << ret << errno;
        return false;
    }

#ifdef Q_OS_MAC
    // Tune that we wait until 'maxIdle' (default: 60) seconds
    ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &maxIdle, sizeof(maxIdle));
    if (ret < 0) {
        qWarning() << "setsockopt(TCP_KEEPALIVE) failed with" << ret << errno;
        return false;
    }

    // and then send up to 'count' (default: 4) keepalive packets out, then disconnect if no response
    ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count));
    if (ret < 0) {
        qWarning() << "setsockopt(TCP_KEEPCNT) failed with" << ret << errno;
        return false;
    }

    // Send a keepalive packet out every 'interval' (default: 60) seconds
    ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
    if (ret < 0) {
        qWarning() << "setsockopt(TCP_KEEPINTVL) failed with" << ret << errno;
        return false;
    }
#else
    // Tune that we wait until 'maxIdle' (default: 60) seconds
    ret = setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &maxIdle, sizeof(maxIdle));
    if (ret < 0) {
        qWarning() << "setsockopt(TCP_KEEPIDLE) failed with" << ret << errno;
        return false;
    }

    // and then send up to 'count' (default: 4) keepalive packets out, then disconnect if no response
    ret = setsockopt(fd, SOL_TCP, TCP_KEEPCNT, &count, sizeof(count));
    if (ret < 0) {
        qWarning() << "setsockopt(TCP_KEEPCNT) failed with" << ret << errno;
        return false;
    }

    // Send a keepalive packet out every 'interval' (default: 60) seconds
    ret = setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, &interval, sizeof(interval));
    if (ret < 0) {
        qWarning() << "setsockopt(TCP_KEEPINTVL) failed with" << ret << errno;
        return false;
    }

#endif
    // Verify that the keepalive option is enabled
    optVal = 0;
    socklen_t optLen = sizeof(optVal);
    ret = getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optVal, &optLen);
    if (ret == -1) {
        qWarning() << "getsockopt(SO_KEEPALIVE) failed with" << ret << errno;
        return false;
    }
    if (!optVal) {
        qWarning() << "SO_KEEPALIVE was not enabled" << optVal;
        return false;
    }
    return true;
#endif
}

} // namespace io
