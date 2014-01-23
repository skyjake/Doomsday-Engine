/** @file p_anim.h  Parser for Hexen ANIMDEFS.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#ifndef LIBHEXEN_ANIMDEFSPARSER_H
#define LIBHEXEN_ANIMDEFSPARSER_H

#include "doomsday.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Attempt to parse the script on the identified @a path as "animation definition" data.
 */
void AnimDefsParser(Str const *path);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBHEXEN_ANIMDEFSPARSER_H
