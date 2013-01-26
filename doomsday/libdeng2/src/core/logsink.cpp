/** @file logsink.h  Sink where log entries are flushed from the LogBuffer.
 * @ingroup core
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

#include "de/LogSink"

namespace de {

LogSink::LogSink(LogSink::IFormatter &formatter)
    : _formatter(&formatter), _mode(Enabled)
{}

LogSink::~LogSink()
{}

void LogSink::setMode(Mode mode)
{
    _mode = mode;
}

LogSink::Mode LogSink::mode() const
{
    return _mode;
}

bool LogSink::willAccept(LogEntry const &entry) const
{
    switch(_mode)
    {
    case Enabled:
        return true;

    case Disabled:
        return false;

    case OnlyNormalEntries:
        return entry.level() < LogEntry::WARNING;

    case OnlyWarningEntries:
        return entry.level() >= LogEntry::WARNING;
    }
}

LogSink::IFormatter *LogSink::formatter()
{
    return _formatter;
}

LogSink &LogSink::operator << (LogEntry const &entry)
{
    DENG2_ASSERT(formatter());

    foreach(String line, formatter()->logEntryToTextLines(entry))
    {
        *this << line;
    }
    return *this;
}

} // namespace de
