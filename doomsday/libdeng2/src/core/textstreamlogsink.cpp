/** @file textstreamlogsink.cpp  Log sink that uses a QTextStream for output.
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

#include "de/TextStreamLogSink"

namespace de {

TextStreamLogSink::TextStreamLogSink(QTextStream *ts)
    : LogSink(_format), _ts(ts)
{
    _ts->setCodec("UTF-8");
}

TextStreamLogSink::~TextStreamLogSink()
{
    delete _ts;
}

LogSink &TextStreamLogSink::operator << (LogEntry const &entry)
{
    foreach(String line, _format.logEntryToTextLines(entry))
    {
        *this << line;
    }
    return *this;
}

LogSink &TextStreamLogSink::operator << (String const &plainText)
{
    *_ts << plainText + "\n";
    return *this;
}

void TextStreamLogSink::flush()
{
    _ts->flush();
}

} // namespace de
