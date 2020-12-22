/** @file debuglogsink.cpp  Log sink that uses QDebug for output.
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

#if 0
#include "de/debuglogsink.h"

namespace de {

DebugLogSink::DebugLogSink(QtMsgType msgType) : LogSink(_format), _msgType(msgType)
{}

DebugLogSink::~DebugLogSink()
{}

LogSink &DebugLogSink::operator << (const String &plainText)
{
    QByteArray utf8 = plainText.toUtf8();
    if (_msgType == QtWarningMsg)
        qWarning() << utf8.constData();
    else
        qDebug() << utf8.constData();
    return *this;
}

void DebugLogSink::flush()
{}

} // namespace de
#endif
