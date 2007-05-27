/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 * rend_main.h: Rendering Subsystem
 */

#ifndef __DOOMSDAY_REND_MAIN_H__
#define __DOOMSDAY_REND_MAIN_H__

#include <math.h>
#include "rend_list.h"
#include "r_things.h"

// Light mod matrix range
#define MOD_RANGE 100

extern float    vx, vy, vz, vang, vpitch, fieldOfView, yfov;
extern byte     smoothTexAnim;
extern float    viewsidex, viewsidey;
extern int      missileBlend, litSprites;
extern boolean  usingFog;
extern byte     fogColor[4];
extern int      r_ambient;
extern byte     devNoLinkedSurfaces;

extern float    lightRangeModMatrix[MOD_RANGE][255];

void            Rend_Register(void);
void            Rend_Init(void);
void            Rend_Reset(void);

void            Rend_RenderMap(void);
void            Rend_ModelViewMatrix(boolean use_angles);

#define Rend_PointDist2D(c) (fabs((vz-c[VY])*viewsidex - (vx-c[VX])*viewsidey))

float           Rend_PointDist3D(float c[3]);
//float           Rend_SignedPointDist2D(float c[2]);
float           Rend_SectorLight(sector_t *sec);
int             Rend_MidTexturePos(float *bottomleft, float *bottomright,
                                   float *topleft, float *topright,
                                   float *texoffy, float tcyoff, float texHeight,
                                   boolean lower_unpeg);
boolean         Rend_IsWallSectionPVisible(line_t *line, segsection_t section,
                                           boolean backside);
boolean         Rend_DoesMidTextureFillGap(line_t *line, int backside);

void            Rend_ApplyLightAdaptation(float *lightvalue);
float           Rend_GetLightAdaptVal(float lightvalue);

void            Rend_CalcLightRangeModMatrix(struct cvar_s* unused);
void            Rend_InitPlayerLightRanges(void);
#endif
