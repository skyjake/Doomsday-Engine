/** @file dedparser.h  DED v1 parser.
 * @ingroup defs
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDOOMSDAY_DED_V1_PARSER_H
#define LIBDOOMSDAY_DED_V1_PARSER_H

#include <de/libcore.h>
#include "../libdoomsday.h"
#include "ded.h"

LIBDOOMSDAY_PUBLIC void DED_SetXGClassLinks(struct xgclass_s *links);

/**
 * Parser of DED v1 definitions.
 * @ingroup data
 */
class LIBDOOMSDAY_PUBLIC DEDParser
{
public:
    DEDParser(ded_t *ded);

    int parse(const char *buffer, de::String sourceFile, bool sourceIsCustom);

private:
    DE_PRIVATE(d)
};

#endif // LIBDOOMSDAY_DED_V1_PARSER_H
