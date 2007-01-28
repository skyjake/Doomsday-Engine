/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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

/*
 * p_map.h : Common play/maputil functions.
 */

#ifndef __COMMON_P_LOCAL__
#define __COMMON_P_LOCAL__

// If "floatok" true, move would be ok
// if within "tmfloorz - tmceilingz".
extern boolean  floatok;
extern fixed_t  tmfloorz;
extern fixed_t  tmceilingz;
extern int      tmfloorpic;

extern line_t  *ceilingline;

#if __JHEXEN__
mobj_t *PuffSpawned;
mobj_t *BlockingMobj;
#endif

boolean         P_CheckSides(mobj_t* actor, int x, int y);

boolean         P_CheckPosition(mobj_t *thing, fixed_t x, fixed_t y);
boolean         P_CheckPosition2(mobj_t *thing, fixed_t x, fixed_t y,
                                 fixed_t z);
#if __JHEXEN__
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage, int distance,
                    boolean damageSource);
#else
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage, int distance);
#endif

#if !__JHEXEN__
boolean         P_TryMove(mobj_t *thing, fixed_t x, fixed_t y,
                          boolean dropoff, boolean slide);
#else
boolean         P_TryMove(mobj_t *thing, fixed_t x, fixed_t y);
#endif
boolean         P_TeleportMove(mobj_t *thing, fixed_t x, fixed_t y, boolean alwaysstomp);
void            P_SlideMove(mobj_t *mo);

void            P_UseLines(player_t *player);

boolean         P_ChangeSector(sector_t *sector, boolean crunch);

extern mobj_t  *linetarget;        // who got hit (or NULL)

fixed_t         P_AimLineAttack(mobj_t *t1, angle_t angle, fixed_t distance);

void            P_LineAttack(mobj_t *t1, angle_t angle, fixed_t distance,
                             fixed_t slope, int damage);

#endif
