#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2019 The MMapper Authors
// Author: Thomas Equeter <waba@waba.be> (Waba)

#include "../global/RuleOf5.h"
#include "../global/macros.h"

#include <memory>

#include <QFile>
#include <QString>

/*! \brief Save to a file in an atomic way.
 *
 * Currently this does not work on Windows (where a simple file overwriting is
 * then performed).
 */
class NODISCARD FileSaver final
{
private:
    QString m_filename;

    // old comment says "disables copying" ... why? and how?
    // answer: QFile cannot be copied
    std::shared_ptr<QFile> m_file = std::make_shared<QFile>();

public:
    FileSaver() = default;
    ~FileSaver();

public:
    DELETE_CTORS_AND_ASSIGN_OPS(FileSaver);

public:
    NODISCARD QFile &getFile() { return *m_file; }
    NODISCARD std::shared_ptr<QFile> getSharedFile() { return m_file; }

    /*! \exception std::runtime_error if the file can't be opened or a currently
     * open file can't be closed.
     */
    void open(const QString &filename) CAN_THROW;

    /*! \exception std::runtime_error if the file can't be safely closed.
     */
    void close() CAN_THROW;
    // Gives up on the file: closes it and, where a temporary file is used, deletes it instead of
    // renaming it into place, so an existing file at the destination is left as it was. (Where no
    // temporary file is used the destination was already truncated when it was opened.)
    void discard();
};
