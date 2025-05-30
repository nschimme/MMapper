#pragma once
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2024 The MMapper Authors

#include "Charset.h"
#include "TaggedString.h"
#include "mm_source_location.h"

#include <ostream>
#include <sstream>

#include <QDebug>

namespace mm {

struct NODISCARD AbstractDebugOStream
{
private:
    QDebug m_debug;
    std::ostringstream m_os_utf8;

protected:
    NODISCARD static QMessageLogger getMessageLogger(const mm::source_location loc)
    {
        return QMessageLogger{loc.file_name(), static_cast<int>(loc.line()), loc.function_name()};
    }

public:
    explicit AbstractDebugOStream(QDebug &&os);
    ~AbstractDebugOStream();

public:
    void writeLatin1(std::string_view sv);
    void writeUtf8(std::string_view sv);

public:
    template<typename T>
    friend AbstractDebugOStream &operator<<(AbstractDebugOStream &self,
                                            const TaggedStringUtf8<T> &string)
    {
        self.writeUtf8(string.getRawStdString());
        return self;
    }

public:
    template<typename T>
    AbstractDebugOStream &operator<<(const T &x)
    {
        auto &self = *this;
        self.m_os_utf8 << x;
        return self;
    }

public:
    AbstractDebugOStream &operator<<(const char *const s)
    {
        assert(s != nullptr);
        auto &self = *this;
        if (s != nullptr) {
            self.writeUtf8(s);
        }
        return self;
    }

public:
    AbstractDebugOStream &operator<<(const std::string_view s)
    {
        auto &self = *this;
        self.writeUtf8(s);
        return self;
    }

public:
    AbstractDebugOStream &operator<<(const std::string &s)
    {
        auto &self = *this;
        self.writeUtf8(s);
        return self;
    }

public:
    using OsFn = std::ostream &(*) (std::ostream &);
    // This handles std::endl and std::flush
    DEPRECATED
    AbstractDebugOStream &operator<<(const OsFn &x)
    {
        auto &self = *this;
        self.m_os_utf8 << x;
        return self;
    }
};

struct NODISCARD DebugOstream final : public AbstractDebugOStream
{
public:
    explicit DebugOstream(source_location loc)
        : AbstractDebugOStream(getMessageLogger(loc).debug())
    {}
    ~DebugOstream();
};

struct NODISCARD InfoOstream final : public AbstractDebugOStream
{
public:
    explicit InfoOstream(source_location loc)
        : AbstractDebugOStream(getMessageLogger(loc).info())
    {}
    ~InfoOstream();
};

struct NODISCARD WarningOstream final : public AbstractDebugOStream
{
public:
    explicit WarningOstream(source_location loc)
        : AbstractDebugOStream(getMessageLogger(loc).warning())
    {}
    ~WarningOstream();
};

} // namespace mm

#define MMLOG_DEBUG() (mm::DebugOstream{MM_SOURCE_LOCATION()})
#define MMLOG_INFO() (mm::InfoOstream{MM_SOURCE_LOCATION()})
#define MMLOG_WARNING() (mm::WarningOstream{MM_SOURCE_LOCATION()})
#define MMLOG_ERROR() MMLOG_WARNING()
#define MMLOG() MMLOG_INFO()
