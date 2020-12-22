/** @file sourcelinetable.h  Table for source paths and line numbers.
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_SOURCELINETABLE_H
#define LIBCORE_SOURCELINETABLE_H

#include "de/string.h"

#include <utility>

namespace de {

/**
 * Generates unique identifiers for lines in source files.
 *
 * Every Record that has been parsed from a ScriptedInfo document has a special
 * variable that determines the source code location of the data. To store this
 * information with less overhead, its value is generated using
 * SourceLineTable.
 *
 * @ingroup data
 */
class SourceLineTable
{
public:
    typedef duint32 LineId;
    typedef std::pair<String, duint> PathAndLine;

public:
    SourceLineTable();

    /**
     * Forms a number that uniquely identifies a line in a source file.
     *
     * @param path        Absolute path of the source file.
     * @param lineNumber  Line number.
     *
     * @return Unique source number.
     */
    LineId lineId(const String &path, duint lineNumber);

    String sourceLocation(LineId sourceId) const;

    PathAndLine sourcePathAndLineNumber(LineId sourceId) const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_SOURCELINETABLE_H

