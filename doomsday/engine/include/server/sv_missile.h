/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * sv_missile.h: Delta Pool Missile Record
 */

#ifndef __DOOMSDAY_SERVER_POOL_MISSILE_H__
#define __DOOMSDAY_SERVER_POOL_MISSILE_H__

#include "sv_def.h"
#include "sv_pool.h"

misrecord_t    *Sv_MRFind(pool_t *pool, thid_t id);
void            Sv_MRAdd(pool_t *pool, const mobjdelta_t *delta);
int             Sv_MRCheck(pool_t *pool, const mobjdelta_t *mobj);
void            Sv_MRRemove(pool_t *pool, thid_t id);

#endif
