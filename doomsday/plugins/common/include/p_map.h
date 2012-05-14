/**\file p_map.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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

/**
 * Common play/maputil functions.
 */

#ifndef LIBCOMMON_P_MAP_H
#define LIBCOMMON_P_MAP_H

extern coord_t attackRange;

// If "floatOk" true, move would be ok if within "tmFloorZ - tmCeilingZ".
extern boolean floatOk;
extern coord_t tmFloorZ;
extern coord_t tmCeilingZ;
extern material_t* tmFloorMaterial;

extern LineDef* ceilingLine, *floorLine;
extern LineDef* blockLine;
extern mobj_t* lineTarget; // Who got hit (or NULL).
extern mobj_t* tmThing;

#if __JHEXEN__
extern mobj_t* puffSpawned;
extern mobj_t* blockingMobj;
#endif

extern AABoxd tmBox;
extern boolean fellDown;

boolean P_CheckSight(const mobj_t* from, const mobj_t* to);

boolean P_CheckPositionXY(mobj_t* thing, coord_t x, coord_t y);
boolean P_CheckPositionXYZ(mobj_t* thing, coord_t x, coord_t y, coord_t z);
boolean P_CheckPosition(mobj_t* thing, coord_t const pos[3]);

#if __JHEXEN__
void P_RadiusAttack(mobj_t* spot, mobj_t* source, int damage, int distance, boolean canDamageSource);
#else
void P_RadiusAttack(mobj_t* spot, mobj_t* source, int damage, int distance);
#endif

boolean P_TryMoveXYZ(mobj_t* thing, coord_t x, coord_t y, coord_t z);

#if !__JHEXEN__
boolean P_TryMoveXY(mobj_t* thing, coord_t x, coord_t y, boolean dropoff, boolean slide);
#else
boolean P_TryMoveXY(mobj_t* thing, coord_t x, coord_t y);
#endif

boolean P_TeleportMove(mobj_t* thing, coord_t x, coord_t y, boolean alwaysStomp);

void P_SlideMove(mobj_t* mo);

void P_UseLines(player_t* player);

boolean P_ChangeSector(Sector* sector, boolean crunch);
void P_HandleSectorHeightChange(int sectorIdx);

float P_AimLineAttack(mobj_t* t1, angle_t angle, coord_t distance);
void P_LineAttack(mobj_t* t1, angle_t angle, coord_t distance, coord_t slope, int damage);

coord_t P_GetGravity(void);

boolean P_CheckSides(mobj_t* actor, coord_t x, coord_t y);

#if __JHEXEN__
boolean P_TestMobjLocation(mobj_t* mobj);

void PIT_ThrustSpike(mobj_t* actor);

boolean P_UsePuzzleItem(player_t* player, int itemType);
#endif

#endif /// LIBCOMMON_P_MAP_H
