/* $Id: p_local.h 3439 2006-07-31 23:49:10Z danij $
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
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
void P_RadiusAttack(mobj_t *spot, mobj_t *source, int damage);
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
