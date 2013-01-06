/** @file api_resourceclass.h Resource class enumeration.
 * @ingroup resource
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_API_RESOURCECLASS_H
#define DOOMSDAY_API_RESOURCECLASS_H

/**
 * Resource Class Identifier.
 *
 * @ingroup base
 *
 * @todo Refactor away. These identifiers are no longer needed.
 */
typedef enum resourceclassid_e {
    RC_NULL = -2,           ///< Not a real class.
    RC_UNKNOWN = -1,        ///< Attempt to guess the class through evaluation of the path.
    RESOURCECLASS_FIRST = 0,
    RC_PACKAGE = RESOURCECLASS_FIRST,
    RC_DEFINITION,
    RC_GRAPHIC,
    RC_MODEL,
    RC_SOUND,
    RC_MUSIC,
    RC_FONT,
    RESOURCECLASS_COUNT
} resourceclassid_t;

#define VALID_RESOURCECLASSID(n)   ((n) >= RESOURCECLASS_FIRST && (n) < RESOURCECLASS_COUNT)

#endif // DOOMSDAY_API_RESOURCECLASS_H
