/** @file logsink.h  Sink where log entries are flushed from the LogBuffer.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBCORE_LOGSINK_H
#define LIBCORE_LOGSINK_H

#include "de/string.h"
#include "de/log.h"
#include "de/list.h"

namespace de {

/**
 * Sink where log entries are flushed from the LogBuffer.
 * @ingroup core
 *
 * LogSinks are flushed only from one thread at a time.
 */
class DE_PUBLIC LogSink
{
public:
    enum Mode {
        Disabled,
        Enabled,
        OnlyNormalEntries, ///< Info or lower.
        OnlyWarningEntries ///< Warning or higher.
    };

public:
    /**
     * Formatters are responsible for converting LogEntry instances to a
     * human-presentable, print-ready format suitable for the sink. It may,
     * for instance, apply indenting and omit repeating parts.
     */
    class DE_PUBLIC IFormatter
    {
    public:
        typedef StringList Lines;

        virtual Lines logEntryToTextLines(const LogEntry &entry) = 0;
        virtual ~IFormatter() = default;
    };

public:
    LogSink();

    /**
     * Construct a sink.
     *
     * @param formatter  Formatter for the flushed entries.
     */
    LogSink(IFormatter &formatter);

    virtual ~LogSink();

    void setMode(Mode mode);

    Mode mode() const;

    bool willAccept(const LogEntry &entry) const;

    IFormatter *formatter();

    /**
     * Output a log entry to the sink. The caller must first verify with
     * isAccepted() whether this is an acceptable entry according to the mode
     * of the sink.
     *
     * The default implementation uses the formatter to convert the entry
     * to one or more lines of text.
     *
     * @param entry  Log entry to output.
     *
     * @return Reference to this sink.
     */
    virtual LogSink &operator << (const LogEntry &entry);

    /**
     * Output a plain text string to the sink. This will be called as a
     * fallback if the formatting of a LogEntry causes an exception.
     *
     * @param plainText  Message.
     *
     * @return Reference to this sink.
     */
    virtual LogSink &operator << (const String &plainText) = 0;

    /**
     * Flushes buffered output.
     */
    virtual void flush() = 0;

private:
    IFormatter *_formatter;
    Mode _mode;
};

} // namespace de

#endif // LIBCORE_LOGSINK_H
