// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Thomas Equeter <waba@waba.be> (Waba)

#include "filesaver.h"

#include "../configuration/configuration.h"
#include "../global/TextUtils.h"
#include "../global/io.h"

#include <cstdio>
#include <stdexcept>
#include <tuple>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <QIODevice>

static constexpr const bool USE_TMP_SUFFIX = true;

static const char *const TMP_FILE_SUFFIX = ".tmp";
NODISCARD static auto maybe_add_suffix(const QString &filename)
{
    return USE_TMP_SUFFIX ? (filename + TMP_FILE_SUFFIX) : filename;
}

static void remove_tmp_suffix(const QString &filename) CAN_THROW
{
    if (!USE_TMP_SUFFIX) {
        return;
    }

    const QString from = filename + TMP_FILE_SUFFIX;
    const QString to = filename;

#ifdef Q_OS_WIN
    const std::wstring fromW = from.toStdWString();
    const std::wstring toW = to.toStdWString();
    if (::ReplaceFileW(toW.c_str(), fromW.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)
        == 0) {
        const auto err = ::GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            if (::MoveFileExW(fromW.c_str(), toW.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)
                == 0) {
                throw io::IOException::withErrorNumber(static_cast<int>(::GetLastError()));
            }
        } else {
            throw io::IOException::withErrorNumber(static_cast<int>(err));
        }
    }
#else
    const auto fromEncoded = QFile::encodeName(from);
    const auto toEncoded = QFile::encodeName(to);
    if (::rename(fromEncoded.data(), toEncoded.data()) == -1) {
        throw io::IOException::withCurrentErrno();
    }
#endif
}

FileSaver::~FileSaver()
{
    try {
        close();
    } catch (...) {
    }
}

void FileSaver::open(const QString &filename) CAN_THROW
{
    close();

    m_filename = filename;

    auto &file = deref(m_file);
    file.setFileName(maybe_add_suffix(filename));

    if (!file.open(QFile::WriteOnly)) {
        throw std::runtime_error(mmqt::toStdStringUtf8(file.errorString()));
    }
}

void FileSaver::close() CAN_THROW
{
    auto &file = deref(m_file);
    if (!file.isOpen()) {
        return;
    }

    file.flush();
    // REVISIT: check return value?
    std::ignore = ::io::fsync(file);
    remove_tmp_suffix(m_filename);
    file.close();
}
