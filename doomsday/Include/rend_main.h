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
 */

/*
 * rend_main.h: Rendering Subsystem
 */

#ifndef __DOOMSDAY_REND_MAIN_H__
#define __DOOMSDAY_REND_MAIN_H__

#include <math.h>
#include "rend_list.h"
#include "r_things.h"

// Parts of a wall segment.
#define SEG_MIDDLE	0x1
#define SEG_TOP		0x2
#define SEG_BOTTOM	0x4

extern float	vx, vy, vz, vang, vpitch, fieldOfView, yfov;
extern boolean	smoothTexAnim;
extern float	viewsidex, viewsidey;	
extern int		missileBlend, litSprites;
extern boolean	useFog;
extern byte		fogColor[4];
extern int		r_ambient;

void	Rend_Init(void);
void	Rend_Reset(void);

void	Rend_RenderMap(void);
void	Rend_ModelViewMatrix(boolean use_angles);

#define Rend_PointDist2D(c) (fabs((vz-c[VY])*viewsidex - (vx-c[VX])*viewsidey))

float	Rend_PointDist3D(float c[3]);
float	Rend_SignedPointDist2D(float c[2]);
int		Rend_SectorLight(sector_t *sec);
int		Rend_SegFacingDir(float v1[2], float v2[2]);
int		Rend_MidTexturePos(float *top, float *bottom, float *texoffy, 
						   float tcyoff, boolean lower_unpeg);

#endif
