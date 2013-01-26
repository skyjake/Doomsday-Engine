/** @file logsink.h  Sink where log entries are flushed from the LogBuffer.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef LIBDENG2_LOGSINK_H
#define LIBDENG2_LOGSINK_H

#include "../String"
#include "../Log"
#include <QList>

namespace de {

/**
 * Sink where log entries are flushed from the LogBuffer.
 * @ingroup core
 */
class DENG2_PUBLIC LogSink
{
public:
    enum Mode
    {
        Disabled,
        Enabled,
        OnlyNormalEntries,  ///< Info or lower.
        OnlyWarningEntries  ///< Warning or higher.
    };

public:
    /**
     * Formatters are responsible for converting LogEntry instances to a
     * human-presentable, print-ready format suitable for the sink. It may,
     * for instance, apply indenting and omit repeating parts.
     */
    class DENG2_PUBLIC IFormatter
    {
    public:
        virtual ~IFormatter() {}

        virtual QList<String> logEntryToTextLines(LogEntry const &entry) = 0;
    };

public:
    LogSink() : _formatter(0) {}

    /**
     * Construct a sink.
     *
     * @param formatter  Formatter for the flushed entries.
     */
    LogSink(IFormatter &formatter);

    virtual ~LogSink();

    void setMode(Mode mode);

    Mode mode() const;

    bool willAccept(LogEntry const &entry) const;

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
     */
    virtual LogSink &operator << (LogEntry const &entry);

    virtual LogSink &operator << (String const &plainText) = 0;

    /**
     * Flushes buffered output.
     */
    virtual void flush() = 0;

private:
    IFormatter *_formatter;
    Mode _mode;
};

} // namespace de

#endif // LIBDENG2_LOGSINK_H
