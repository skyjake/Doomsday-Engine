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
 * rend_dyn.h: Dynamic Lights
 */

#ifndef __DOOMSDAY_DYNLIGHTS_H__
#define __DOOMSDAY_DYNLIGHTS_H__

#include "p_mobj.h"
#include "rend_list.h"

#define DYN_ASPECT		1.1f			// 1.2f is just too round for Doom.

// Lumobj Flags.
#define LUMF_USED		0x1
#define LUMF_RENDERED	0x2
#define LUMF_CLIPPED	0x4				// Hidden by world geometry.
#define LUMF_NOHALO		0x100

typedef struct lumobj_s					// For dynamic lighting.
{
	struct lumobj_s *next;				// Next in the same DL block, or NULL.
	struct lumobj_s *ssNext;			// Next in the same subsector, or NULL.
	int		flags;
	mobj_t	*thing;
	float	center; 					// Offset to center from mobj Z.
	int		radius, patch, distance;	// Radius: lights are spheres.
	int		flareSize;					// Radius for this light source.
	byte	rgb[3];						// The color.
	float	xOff;
	float	xyScale;					// 1.0 if there's no modeldef.
	DGLuint	tex;						// Lightmap texture.
	DGLuint	floorTex, ceilTex;			// Lightmaps for floor/ceil.
	DGLuint decorMap;					// Decoration lightmap.
	char	flareTex;					// Zero = automatical.
	float	flareMul;					// Flare brightness factor.
} 
lumobj_t;

/*
 * The data of a projected dynamic light is stored in this structure.
 * A list of these is associated with all the lit segs/planes in a frame.
 */
typedef struct dynlight_s
{
	struct dynlight_s *next, *nextUsed;
	int		flags;
	float	s[2], t[2];
	byte	color[3];
	DGLuint texture;
}
dynlight_t;

// Flags for projected dynamic lights.
#define DYNF_PREGEN_DECOR	0x1	// Pregen RGB lightmap for a light decoration.

extern boolean	dlInited;
extern lumobj_t	*luminousList;
extern lumobj_t	**dlSubLinks;
extern int		numLuminous;
extern int		useDynLights;
extern int		maxDynLights, dlBlend, dlMaxRad;
extern float	dlRadFactor, dlFactor;
extern int		useWallGlow, glowHeight;
extern float	glowFogBright;
extern int		rend_info_lums;

extern dynlight_t **floorLightLinks;
extern dynlight_t **ceilingLightLinks;

// Setup.
void DL_InitLinks();
void DL_Clear();	// 'Physically' destroy the tables.

// Action.
void DL_ClearForFrame();
void DL_InitForNewFrame();
int DL_NewLuminous(void);
lumobj_t *DL_GetLuminous(int index);
void DL_ProcessSubsector(subsector_t *ssec);
void DL_ProcessWallSeg(lumobj_t *lum, seg_t *seg, sector_t *frontsector);
dynlight_t *DL_GetSegLightLinks(int seg, int whichpart);

// Helpers.
boolean DL_RadiusIterator(subsector_t *subsector, fixed_t x, fixed_t y, fixed_t radius, boolean (*func)(lumobj_t*,fixed_t));
boolean DL_BoxIterator(fixed_t box[4], void *ptr, boolean (*func)(lumobj_t*,void*));

int Rend_SubsectorClipper(fvertex_t *out, subsector_t *sub, float x, float y, float radius);

#endif
