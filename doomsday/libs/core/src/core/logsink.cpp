/** @file logsink.cpp  Sink where log entries are flushed from the LogBuffer.
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

#include "de/logsink.h"

namespace de {

LogSink::LogSink()
    : _formatter(0),
      _mode(Enabled)
{}

LogSink::LogSink(LogSink::IFormatter &formatter)
    : _formatter(&formatter),
      _mode(Enabled)
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

bool LogSink::willAccept(const LogEntry &entry) const
{
    switch (_mode)
    {
    case Enabled:
        return true;

    case Disabled:
        return false;

    case OnlyNormalEntries:
        return entry.level() < LogEntry::Warning;

    case OnlyWarningEntries:
        return entry.level() >= LogEntry::Warning;
    }
    return false;
}

LogSink::IFormatter *LogSink::formatter()
{
    return _formatter;
}

LogSink &LogSink::operator<<(const LogEntry &entry)
{
    DE_ASSERT(formatter());
    for (const String &line : formatter()->logEntryToTextLines(entry))
    {
        *this << line;
    }
    return *this;
}

} // namespace de
