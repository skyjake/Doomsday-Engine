/** @file debuglogsink.h  Log sink that uses QDebug for output.
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

#include "de/DebugLogSink"

namespace de {

DebugLogSink::DebugLogSink(QtMsgType msgType) : LogSink(_format)
{
    _qs = new QDebug(msgType);
}

DebugLogSink::~DebugLogSink()
{
    delete _qs;
}

LogSink &DebugLogSink::operator << (LogEntry const &entry)
{
    foreach(String line, _format.logEntryToTextLines(entry))
    {
        *_qs << line;
    }
    return *this;
}

LogSink &DebugLogSink::operator << (String const &plainText)
{
    *_qs << plainText.toUtf8().constData();
    return *this;
}

} // namespace de
