/** @file textstreamlogsink.h  Log sink that uses a QTextStream for output.
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

#ifndef LIBDENG2_TEXTSTREAMLOGSINK_H
#define LIBDENG2_TEXTSTREAMLOGSINK_H

#include "../LogSink"
#include "../MonospaceLogSinkFormatter"

namespace de {

/**
 * Log sink that outputs to a QTextStream.
 * @ingroup core
 */
class TextStreamLogSink : public LogSink
{
public:
    /**
     * Construct a log sink.
     *
     * @param ts  Text stream. Ownership given to the sink.
     */
    TextStreamLogSink(QTextStream *ts);

    ~TextStreamLogSink();

    LogSink &operator << (LogEntry const &entry);

    LogSink &operator << (String const &plainText);

    void flush();

private:
    QTextStream *_ts;
    MonospaceLogSinkFormatter _format;
};

} // namespace de

#endif // LIBDENG2_TEXTSTREAMLOGSINK_H
