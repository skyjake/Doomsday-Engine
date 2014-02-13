/** @file textstreamlogsink.cpp  Log sink that uses a QTextStream for output.
 * @ingroup core
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
