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
 * rend_list.h: Rendering Lists
 */

#ifndef __DOOMSDAY_REND_LIST_H__
#define __DOOMSDAY_REND_LIST_H__

#include "r_data.h"

// Multiplicative blending for dynamic lights?
#define IS_MUL	(!dlBlend && !useFog)

// PrepareFlat directions.
#define RLPF_NORMAL		0
#define RLPF_REVERSE	1

extern int		renderTextures;
extern int		renderWireframe;
extern int		useMultiTexLights;
extern int		useMultiTexDetails;

extern float	rend_light_wall_angle;
extern float	detailFactor, detailScale;

void RL_Init();
void RL_ClearLists();
void RL_DeleteLists();
void RL_AddPoly(rendpoly_t *poly);
void RL_PrepareFlat(planeinfo_t *plane, rendpoly_t *poly, subsector_t *subsector);
void RL_VertexColors(rendpoly_t *poly, int lightlevel, const byte *rgb);
void RL_RenderAllLists();

void RL_SelectTexUnits(int count);
void RL_Bind(DGLuint texture);
void RL_BindTo(int unit, DGLuint texture);
void RL_FloatRGB(byte *rgb, float *dest);

#endif
