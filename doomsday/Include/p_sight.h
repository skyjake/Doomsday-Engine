/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 *
 * Based on Hexen by Raven Software.
 */

/*
 * de_play.h: Line of Sight Testing
 */

#ifndef __DOOMSDAY_PLAY_SIGHT_H__
#define __DOOMSDAY_PLAY_SIGHT_H__

extern fixed_t topslope;
extern fixed_t bottomslope;		

boolean P_CheckReject(sector_t *sec1, sector_t *sec2);
boolean P_CheckFrustum(int plrNum, mobj_t *mo);
boolean P_CheckSight(mobj_t *t1, mobj_t *t2);

#endif 
