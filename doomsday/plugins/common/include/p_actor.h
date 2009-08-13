/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * p_actor.h: Common actor (mobj) routines.
 */

#ifndef __DD_COMMON_ACTOR_H__
#define __DD_COMMON_ACTOR_H__

void            P_MobjRemove(mobj_s* mo, boolean noRespawn);

void            P_MobjUnsetPosition(mobj_t* mo);
void            P_MobjSetPosition(mobj_t* mo);
void            P_MobjSetSRVO(mobj_t* mo, float stepx, float stepy);
void            P_MobjSetSRVOZ(mobj_t* mo, float stepz);
void            P_MobjClearSRVO(mobj_t* mo);
void            P_MobjAngleSRVOTicker(mobj_t* mo);

boolean         P_MobjIsCamera(const mobj_s* mo);

void            P_UpdateHealthBits(mobj_t* mo);
statenum_t      P_GetState(int /*mobjtype_t*/ mobjType, statename_t name);
#endif
