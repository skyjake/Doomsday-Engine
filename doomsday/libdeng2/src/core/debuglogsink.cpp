/** @file debuglogsink.cpp  Log sink that uses QDebug for output.
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

DebugLogSink::DebugLogSink(QtMsgType msgType) : LogSink(_format), _msgType(msgType)
{}

DebugLogSink::~DebugLogSink()
{}

LogSink &DebugLogSink::operator << (String const &plainText)
{
    QByteArray utf8 = plainText.toUtf8();
    if(_msgType == QtWarningMsg)
        qWarning() << utf8.constData();
    else
        qDebug() << utf8.constData();
    return *this;
}

void DebugLogSink::flush()
{}

} // namespace de
