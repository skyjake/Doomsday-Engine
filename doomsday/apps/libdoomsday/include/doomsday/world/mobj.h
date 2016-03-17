/** @file mobj.h  Base for world map objects.
 *
 * @authors Copyright © 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDOOMSDAY_MOBJ_H
#define LIBDOOMSDAY_MOBJ_H

#include "dd_share.h" /// @todo dd_share.h is not part of libdoomsday.
#include "../players.h"

// This macro can be used to calculate a mobj-specific 'random' number.
#define MOBJ_TO_ID(mo)          ( (long)(mo)->thinker.id * 48 + (PTR2INT(mo)/1000) )

// Game plugins define their own mobj_s/t.
/// @todo Plugin mobjs should be derived from a class in libdoomsday, and
/// the DD_BASE_MOBJ_ELEMENTS macros should be removed. -jk
#ifndef LIBDOOMSDAY_CUSTOM_MOBJ

// We'll use the base mobj template directly as our mobj.
typedef struct mobj_s {
    DD_BASE_MOBJ_ELEMENTS()
#ifdef __cplusplus
    mobj_s(thinker_s::InitBehavior b) : thinker(b) {}
#endif
} mobj_t;

#endif // LIBDOOMSDAY_CUSTOM_MOBJ

#endif // LIBDOOMSDAY_MOBJ_H
