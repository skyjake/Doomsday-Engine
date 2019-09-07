/** @file sv_missile.h Delta Pool Missile Record.
 * @ingroup server
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef __DOOMSDAY_SERVER_POOL_MISSILE_H__
#define __DOOMSDAY_SERVER_POOL_MISSILE_H__

#include "sv_def.h"

struct pool_s;
struct mobjdelta_s;

typedef struct misrecord_s {
    struct misrecord_s* next, *prev;
    thid_t          id;
    //fixed_t momx, momy, momz;
} misrecord_t;

typedef struct mislink_s {
    misrecord_t*    first, *last;
} mislink_t;

misrecord_t    *Sv_MRFind(struct pool_s *pool, thid_t id);
void            Sv_MRAdd(struct pool_s *pool, const struct mobjdelta_s *delta);
int             Sv_MRCheck(struct pool_s *pool, const struct mobjdelta_s *mobj);
void            Sv_MRRemove(struct pool_s *pool, thid_t id);

#endif
