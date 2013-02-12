/** @file monospacelogsinkformatter.h  Fixed-width log entry formatter.
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

#ifndef LIBDENG2_MONOSPACELOGSINKFORMATTER_H
#define LIBDENG2_MONOSPACELOGSINKFORMATTER_H

#include "../LogSink"

namespace de {

/**
 * Log entry formatter with fixed line length and the assumption of fixed-width
 * fonts. This formatter is for plain text output.
 * @ingroup core
 */
class DENG2_PUBLIC MonospaceLogSinkFormatter : public LogSink::IFormatter
{
public:
    MonospaceLogSinkFormatter();

    QList<String> logEntryToTextLines(LogEntry const &entry);

    /**
     * Sets the maximum line length. Entries will be wrapped onto multiple
     * lines if they don't fit on one line.
     *
     * @param maxLength  Maximum line length.
     */
    void setMaxLength(duint maxLength);

    duint maxLength() const;

private:
    duint _maxLength;
    int _minimumIndent;
    String _sectionOfPreviousLine;
    int _sectionDepthOfPreviousLine;
};

} // namespace de

#endif // LIBDENG2_MONOSPACELOGSINKFORMATTER_H
