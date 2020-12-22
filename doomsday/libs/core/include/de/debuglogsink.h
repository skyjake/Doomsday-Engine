/** @file debuglogsink.h  Log sink that uses QDebug for output.
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
#ifndef LIBCORE_DEBUGLOGSINK_H
#define LIBCORE_DEBUGLOGSINK_H

#include "de/logsink.h"
#include "de/monospacelogsinkformatter.h"

namespace de {

/**
 * Log sink that uses QDebug for output.
 * @ingroup core
 */
class DebugLogSink : public LogSink
{
public:
    DebugLogSink(QtMsgType msgType);
    ~DebugLogSink();

    LogSink &operator << (const String &plainText);
    void flush();

private:
    QtMsgType _msgType;
    MonospaceLogSinkFormatter _format;
};

} // namespace de

#endif // LIBCORE_DEBUGLOGSINK_H
#endif
