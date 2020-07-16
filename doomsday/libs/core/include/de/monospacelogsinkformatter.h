/** @file monospacelogsinkformatter.h  Fixed-width log entry formatter.
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

#ifndef LIBCORE_MONOSPACELOGSINKFORMATTER_H
#define LIBCORE_MONOSPACELOGSINKFORMATTER_H

#include "de/logsink.h"

namespace de {

/**
 * Log entry formatter with fixed line length and the assumption of fixed-width
 * fonts. This formatter is for plain text output.
 * @ingroup core
 */
class DE_PUBLIC MonospaceLogSinkFormatter : public LogSink::IFormatter
{
public:
    MonospaceLogSinkFormatter();

    StringList logEntryToTextLines(const LogEntry &entry);

    /**
     * Sets the maximum line length. Entries will be wrapped onto multiple
     * lines if they don't fit on one line.
     *
     * @param maxLength  Maximum line length.
     */
    void setMaxLength(duint maxLength);

    duint maxLength() const;

private:
    duint  _maxLength;
    int    _minimumIndent;
    String _sectionOfPreviousLine;
    int    _sectionDepthOfPreviousLine;
};

} // namespace de

#endif // LIBCORE_MONOSPACELOGSINKFORMATTER_H
