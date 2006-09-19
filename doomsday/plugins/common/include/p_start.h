/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * p_start.h: Common playsim code relating to (re)spawn of map objects
 *            and map setup.
 */

#ifndef __COMMON_PLAYSTART_H__
#define __COMMON_PLAYSTART_H__

extern thing_t *playerstarts;
extern int      numPlayerStarts;

void            P_Init(void);
int             P_RegisterPlayerStart(thing_t * mthing);
void            P_FreePlayerStarts(void);
boolean         P_CheckSpot(int playernum, thing_t * mthing,
                            boolean doTeleSpark);
boolean         P_FuzzySpawn(thing_t * spot, int playernum,
                             boolean doTeleSpark);
thing_t        *P_GetPlayerStart(int group, int pnum);
void            P_DealPlayerStarts(int group);
void            P_SpawnPlayers(void);

void            P_GetMapLumpName(int episode, int map, char *lumpName);

#endif
