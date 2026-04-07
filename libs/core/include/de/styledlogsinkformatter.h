/** @file styledlogsinkformatter.h  Rich text log entry formatter.
 *
 * @authors Copyright (c) 2013-2019 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_STYLEDLOGSINKFORMATTER_H
#define LIBCORE_STYLEDLOGSINKFORMATTER_H

#include "logsink.h"
#include "string.h"

namespace de {

/**
 * Formats log entries for styled output.
 */
class DE_PUBLIC StyledLogSinkFormatter : public LogSink::IFormatter
{
public:
    /**
     * Constructs a formatter that uses Config for the format options.
     * - Config.log.showMetadata determines if entry metadata is to be included.
     */
    StyledLogSinkFormatter();

    StyledLogSinkFormatter(const Flags &formatFlags);

    Lines logEntryToTextLines(const LogEntry &entry);

    /**
     * Omits the log entry section information if the entry is marked as
     * a non-developer entry. The default is @c true.
     *
     * @param omit  Omit section.
     */
    void setOmitSectionIfNonDev(bool omit);

    void setShowMetadata(bool show);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_STYLEDLOGSINKFORMATTER_H
