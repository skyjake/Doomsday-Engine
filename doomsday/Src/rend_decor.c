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
 * rend_decor.c: Decorations
 *
 * Surface decorations (dynamic lights).
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// Quite a bit of lights, there!
#define MAX_SOURCES		16384

// TYPES -------------------------------------------------------------------

typedef struct decorsource_s {
	mobj_t thing;
	struct decorsource_s *next;
} decorsource_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean		useDecorations = true;
float		decorWallMaxDist = 1500; // No decorations are visible beyond this.
float		decorPlaneMaxDist = 1500;
float		decorWallFactor = 1;
float		decorPlaneFactor = 1;
float		decorFadeAngle = .1f;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int	numSources;
static decorsource_t *sourceFirst, *sourceLast, *sourceCursor;

// Lights near surfaces get dimmer if the angle is too small.
static float surfaceNormal[3];

// CODE --------------------------------------------------------------------

/*
 * Returns a pointer to the surface decoration, if any.
 */
ded_decor_t *Rend_GetTextureDecoration(int texture)
{
	if(!texture) return NULL;
	return textures[ texturetranslation[texture].current ]->decoration;
}

/*
 * Returns a pointer to the surface decoration, if any.
 */
ded_decor_t *Rend_GetFlatDecoration(int index)
{
	flat_t *flat = R_GetFlat(index);

	// Get the translated one?
	if(flat->translation.current != index) 
	{
		flat = R_GetFlat(flat->translation.current);
	}
	return flat->decoration;
}

/*
 * Clears the list of decoration dummies.
 */
void Rend_ClearDecorations(void)
{
	numSources = 0;
	sourceCursor = sourceFirst;
}

/*
 * Project all the non-clipped decorations. They become regular vissprites.
 * This is needed for rendering halos.
 */
void Rend_ProjectDecorations(void)
{
	decorsource_t *src;

	// No need for this if no halos are rendered.
	if(!haloMode) return;

	//for(i = 0; i < numSources; i++)
	for(src = sourceFirst; src != sourceCursor; src = src->next)
	{
		lumobj_t *lum = DL_GetLuminous(src->thing.light);

		// Clipped sources don't get halos.
		if(lum->flags & LUMF_CLIPPED || lum->flareSize <= 0)
			continue;

		R_ProjectDecoration(&src->thing);
	}
}

/*
 * Create a new source for a light decoration.
 */
decorsource_t *Rend_NewLightDecorationSource(void)
{
	decorsource_t *src;

	if(numSources > MAX_SOURCES) return NULL;

	numSources++;

	// If the cursor is NULL, new sources must be allocated.
	if(!sourceCursor)
	{
		// Allocate a new entry.
		src = Z_Calloc(sizeof(decorsource_t), PU_STATIC, NULL);

		if(sourceLast) sourceLast->next = src;
		sourceLast = src;

		if(!sourceFirst) sourceFirst = src;
	}
	else
	{
		// There are old sources to use.
		src = sourceCursor;
		memset(&src->thing, 0, sizeof(src->thing));

		// Advance the cursor.
		sourceCursor = sourceCursor->next;
	}

	return src;		
}

/*
 * A light decoration is created in the specified coordinates.
 * Does largely the same thing as DL_AddLuminous().
 */
void Rend_AddLightDecoration
	(float pos[3], ded_decorlight_t *def, float brightness, boolean isWall,
	 DGLuint decorMap)
{
	decorsource_t *source;
	lumobj_t *lum;
	float distance = Rend_PointDist3D(pos);
	float fadeMul = 1, flareMul = 1;
	float maxDist = (isWall? decorWallMaxDist : decorPlaneMaxDist);
	int i;

	// Is the point in range?
	if(distance > maxDist) return;

	// Close enough to the maximum distance, the lights fade out.
	if(distance > .67f * maxDist)
	{
		fadeMul = (maxDist - distance) / (.33f * maxDist);
	}

	// Apply the brightness factor (was calculated using sector lightlevel).
	fadeMul *= brightness * (isWall? decorWallFactor : decorPlaneFactor);

	// Brightness drops as the angle gets too big.
	if(def->elevation < 2 && decorFadeAngle > 0) // Close the surface?
	{
		float vector[3] = { pos[VX] - vx, pos[VZ] - vy, pos[VY] - vz };
		float dot;
		M_Normalize(vector);
		dot = 
			-(surfaceNormal[VX] * vector[VX]
			+ surfaceNormal[VY] * vector[VY]
			+ surfaceNormal[VZ] * vector[VZ]);
		if(dot < decorFadeAngle/2)
		{
			flareMul = 0;
		}
		else if(dot < 3 * decorFadeAngle)
		{
			flareMul *= (dot - decorFadeAngle/2) / (2.5f * decorFadeAngle);
		}
	}

	if(fadeMul <= 0) return;

	if(!(source = Rend_NewLightDecorationSource())) 
		return; // Out of sources!

	// Initialize the essentials in the dummy mobj.
	source->thing.x = pos[VX] * FRACUNIT;
	source->thing.y = pos[VY] * FRACUNIT;
	source->thing.z = pos[VZ] * FRACUNIT;
	source->thing.frame = FF_FULLBRIGHT;
	source->thing.halofactor = 0xff; // Assumed visible.
	source->thing.subsector 
		= R_PointInSubsector(source->thing.x, source->thing.y);		

	// Fill in the data for a new luminous object.
	source->thing.light = DL_NewLuminous();
	lum = DL_GetLuminous(source->thing.light);
	lum->thing = &source->thing;
	lum->center = 0;
	lum->flags = LUMF_CLIPPED;
	lum->tex = def->sides.tex;
	lum->ceilTex = def->up.tex;
	lum->floorTex = def->down.tex;

	// These are the same rules as in DL_ThingRadius().
	lum->radius = def->radius * 40 * dlRadFactor;
	
	// Don't make a too small or too large light.
	if(lum->radius > dlMaxRad) lum->radius = dlMaxRad;

	if(def->halo_radius > 0)
	{
		lum->flareSize = def->halo_radius * 60 * (50 + haloSize)/100.0f;
		if(lum->flareSize < 1) lum->flareSize = 1;
	}
	else
	{
		lum->flareSize = 0;
	}

	// This light source is associated with a decoration map, if one is 
	// available.
	lum->decorMap = decorMap;

	// Zero = Texture chosen automatically.
	lum->flareTex = def->flare_texture;
	lum->flareMul = flareMul;

	for(i = 0; i < 3; i++) 
		lum->rgb[i] = (byte) (255 * def->color[i] * fadeMul);

	// Approximate the distance.
	lum->distance = P_ApproxDistance3(source->thing.x - viewx, 
		source->thing.y - viewy, source->thing.z - viewz);
}

/*
 * Returns true if the view point is close enough to the bounding box
 * so that there could be visible decorations inside.
 */
boolean Rend_CheckDecorationBounds(fixed_t bounds[6], float fMaxDist)
{
	fixed_t maxDist = FRACUNIT * fMaxDist;

	return viewx > bounds[BLEFT]    - maxDist
		&& viewx < bounds[BRIGHT]   + maxDist
		&& viewy > bounds[BBOTTOM]  - maxDist
		&& viewy < bounds[BTOP]     + maxDist
		&& viewz > bounds[BFLOOR]   - maxDist
		&& viewz < bounds[BCEILING] + maxDist;
}

/*
 * Returns > 0 if the sector lightlevel passes the limit condition.
 */
float Rend_CheckSectorLight(sector_t *sector, ded_decorlight_t *lightDef)
{
	int lightlevel = sector->lightlevel;
	float factor;

	// Has a limit been set?
	if(lightDef->light_levels[0] == lightDef->light_levels[1]) 
		return 1;

	if(lightlevel < r_ambient) lightlevel = r_ambient;

	factor = (lightlevel - lightDef->light_levels[0]) 
		/ (float) (lightDef->light_levels[1] - lightDef->light_levels[0]);
	if(factor < 0) return 0;
	if(factor > 1) return 1;
	return factor;
}

/*
 * Determine proper skip values.
 */
void Rend_DecorationPatternSkip(ded_decorlight_t *lightDef, int *skip)
{
	int k;

	for(k = 0; k < 2; k++)
	{
		// Skip must be at least one.
		skip[k] = lightDef->pattern_skip[k] + 1;
		if(skip[k] < 1) skip[k] = 1;
	}
}

/*
 * Generate decorations for the specified section of a line.
 */
void Rend_DecorateLineSection
	(line_t *line, side_t *side, int texture, float top, float bottom,
	 float texOffY)
{
	lineinfo_t *linfo = &lineinfo[GET_LINE_IDX(line)];
	ded_decor_t *def;
	ded_decorlight_t *lightDef;
	vertex_t *v1, *v2;
	float lh, s, t; // Horizontal and vertical offset.
	float posBase[2], delta[2], pos[3], brightMul;
	float surfTexW, surfTexH, patternW, patternH;
	int i, skip[2];

	// Is this a valid section?
	if(bottom > top || linfo->length == 0) return;

	// Should this be decorated at all?
	if(!(def = Rend_GetTextureDecoration(texture))) return;

	v1 = line->v1;
	v2 = line->v2;

	// Let's see which sidedef is present.
	if(line->sidenum[1] >= 0 && SIDE_PTR(line->sidenum[1]) == side)
	{
		// Flip vertices, this is the backside.
		v1 = line->v2;
		v2 = line->v1;
	}

	delta[VX] = FIX2FLT(v2->x - v1->x);
	delta[VY] = FIX2FLT(v2->y - v1->y);
	surfaceNormal[VX] = delta[VY] / linfo->length;
	surfaceNormal[VZ] = -delta[VX] / linfo->length;
	surfaceNormal[VY] = 0;

	// Height of the section.
	lh = top - bottom;

	// Setup the global texture info variables.
	GL_GetTextureInfo(texture);
	surfTexW = texw;
	surfTexH = texh;

	// Generate a number of lights.
	for(i = 0; i < DED_DECOR_NUM_LIGHTS; i++)
	{
		lightDef = def->lights + i;

		// No more?
		if(!R_IsValidLightDecoration(lightDef)) break;

		// Does it pass the sectorlight limitation?
		if((brightMul = Rend_CheckSectorLight(side->sector, lightDef)) <= 0)
			continue;

		// Skip must be at least one.
		Rend_DecorationPatternSkip(lightDef, skip);

		posBase[VX] = FIX2FLT(v1->x) + lightDef->elevation * surfaceNormal[VX];
		posBase[VY] = FIX2FLT(v1->y) + lightDef->elevation * surfaceNormal[VZ];
		
		patternW = surfTexW * skip[VX];
		patternH = surfTexH * skip[VY];

		// Let's see where the top left light is.
		s = M_CycleIntoRange(lightDef->pos[VX] - FIX2FLT(side->textureoffset)
			- surfTexW * lightDef->pattern_offset[VX], patternW);

		for(; s < linfo->length; s += patternW)
		{
			t = M_CycleIntoRange(lightDef->pos[VY] - FIX2FLT(side->rowoffset)
				- surfTexH * lightDef->pattern_offset[VY] + texOffY, 
				patternH);
			
			for(; t < lh; t += patternH)
			{
				// Let there be light.
				pos[VX] = posBase[VX] + delta[VX] * s / linfo->length;
				pos[VY] = posBase[VY] + delta[VY] * s / linfo->length;
				pos[VZ] = top - t;
				Rend_AddLightDecoration(pos, lightDef, brightMul, true,
					def->pregen_lightmap);
			}
		}
	}
}

/*
 * Returns the side that faces the sector (if any).
 */
side_t *R_GetSectorSide(line_t *line, sector_t *sector)
{
	side_t *side = SIDE_PTR(line->sidenum[0]);

	// Swap if that wasn't the right one.
	if(side->sector != sector)
		return SIDE_PTR(line->sidenum[1]);

	return side;
}

/*
 * Return true if the line is within the visible decoration 'box'.
 */
boolean Rend_LineDecorationBounds(line_t *line)
{
	fixed_t bounds[6];
	sector_t *sector;

	bounds[BLEFT]   = line->bbox[BOXLEFT];
	bounds[BRIGHT]  = line->bbox[BOXRIGHT];
	bounds[BTOP]    = line->bbox[BOXTOP];
	bounds[BBOTTOM] = line->bbox[BOXBOTTOM];
	
	// Figure out the highest and lowest Z height.
	sector           = line->frontsector;
	bounds[BFLOOR]   = sector->floorheight;
	bounds[BCEILING] = sector->ceilingheight;

	// Is the other sector higher/lower?
	if((sector = line->backsector) != NULL)
	{
		if(sector->floorheight < bounds[BFLOOR])
			bounds[BFLOOR] = sector->floorheight;

		if(sector->ceilingheight > bounds[BCEILING])
			bounds[BCEILING] = sector->ceilingheight;
	}

	return Rend_CheckDecorationBounds(bounds, decorWallMaxDist);
}

/*
 * Return true if the sector is within the visible decoration 'box'.
 */
boolean Rend_SectorDecorationBounds(sector_t *sector, sectorinfo_t *sin)
{
	fixed_t bounds[6];

	bounds[BLEFT]    = FRACUNIT * sin->bounds[BLEFT];
	bounds[BRIGHT]   = FRACUNIT * sin->bounds[BRIGHT];
	// Sectorinfo has top and bottom the other way around.
	bounds[BBOTTOM]  = FRACUNIT * sin->bounds[BTOP];
	bounds[BTOP]     = FRACUNIT * sin->bounds[BBOTTOM];
	bounds[BFLOOR]   = FRACUNIT * sin->visfloor;
	bounds[BCEILING] = FRACUNIT * sin->visceil;

	return Rend_CheckDecorationBounds(bounds, decorPlaneMaxDist);
}

/*
 * Generate decorations for upper, middle and bottom parts of the line,
 * on both sides.
 */
void Rend_DecorateLine(int index)
{
	line_t *line = LINE_PTR(index);
	side_t *side;
	sector_t *highSector, *lowSector;
/*	int frontSectorIndex = -1, backSectorIndex = -1; */
	float frontCeil, frontFloor, backCeil, backFloor;

	// Only the lines within the decoration visibility bounding box
	// are processed.
	if(!Rend_LineDecorationBounds(line)) return;

	frontCeil  = SECT_CEIL(line->frontsector);
	frontFloor = SECT_FLOOR(line->frontsector);

	// Do we have a double-sided line?
	if(line->backsector)
	{
		backCeil  = SECT_CEIL(line->backsector);
		backFloor = SECT_FLOOR(line->backsector);

		// Is there a top section visible on either side?
		if(backCeil != frontCeil
			&& (line->backsector->ceilingpic != skyflatnum
				|| line->frontsector->ceilingpic != skyflatnum))
		{
			if(frontCeil > backCeil)
			{
				highSector = line->frontsector;
				lowSector = line->backsector;
			}
			else
			{
				lowSector = line->frontsector;
				highSector = line->backsector;
			}

			// Figure out the right side.
			side = R_GetSectorSide(line, highSector);

			GL_GetTextureInfo(side->toptexture);
			Rend_DecorateLineSection(line, side, side->toptexture,
				SECT_CEIL(highSector), SECT_CEIL(lowSector), 
				line->flags & ML_DONTPEGTOP? 0 : -texh + 
				(SECT_CEIL(highSector) - SECT_CEIL(lowSector)));
		}

		// Is there a bottom section visible?
		if(backFloor != frontFloor
			&& (line->backsector->floorpic != skyflatnum
				|| line->frontsector->floorpic != skyflatnum))
		{
			if(frontFloor > backFloor)
			{
				highSector = line->frontsector;
				lowSector = line->backsector;
			}
			else
			{
				lowSector = line->frontsector;
				highSector = line->backsector;
			}

			// Figure out the right side.
			side = R_GetSectorSide(line, lowSector);

			Rend_DecorateLineSection(line, side, side->bottomtexture,
				SECT_FLOOR(highSector), SECT_FLOOR(lowSector), 
				line->flags & ML_DONTPEGBOTTOM? SECT_FLOOR(highSector) 
				- SECT_CEIL(lowSector): 0);
		}

		// 2-sided middle texture?
		// FIXME: Since halos aren't usually clipped by 2-sided middle 
		// textures, this looks a bit silly.
/*		if(line->sidenum[0] >= 0
			&& (side = SIDE_PTR(line->sidenum[0]))->midtexture)
		{
			rendpoly_t quad;
			
			// If there is an opening, process it.
			GL_GetTextureInfo(side->midtexture);
			quad.top = MIN_OF(frontCeil, backCeil);
			quad.bottom = MAX_OF(frontFloor, backFloor);
			if(Rend_MidTexturePos(&quad, FIX2FLT(side->rowoffset),
				(line->flags & ML_DONTPEGBOTTOM) != 0))
			{
				Rend_DecorateLineSection(line, side, side->midtexture,
					quad.top, quad.bottom, quad.texoffy);
			}
		}*/
	}
	else
	{
		// This is a single-sided line. We only need to worry about the
		// middle texture.
		side = SIDE_PTR(line->sidenum[0] >= 0? line->sidenum[0] 
			: line->sidenum[1]);

		GL_GetTextureInfo(side->midtexture);
		Rend_DecorateLineSection(line, side, side->midtexture, frontCeil,
			frontFloor, line->flags & ML_DONTPEGBOTTOM? -texh + (frontCeil 
			- frontFloor) : 0);
	}
}

/*
 * Generate decorations for a plane.
 */
void Rend_DecoratePlane
	(int sectorIndex, float z, float elevateDir, float offX, float offY,
	 ded_decor_t *def)
{
	sector_t *sector = SECTOR_PTR(sectorIndex);
	sectorinfo_t *sin = secinfo + sectorIndex;
	ded_decorlight_t *lightDef;
	float pos[3], tileSize = 64, brightMul;
	int i, skip[2];

	surfaceNormal[VX] = 0;
	surfaceNormal[VY] = elevateDir;
	surfaceNormal[VZ] = 0;

	// Generate a number of lights.
	for(i = 0; i < DED_DECOR_NUM_LIGHTS; i++)
	{
		lightDef = def->lights + i;

		// No more?
		if(!R_IsValidLightDecoration(lightDef)) break;

		// Does it pass the sectorlight limitation?
		if((brightMul = Rend_CheckSectorLight(sector, lightDef)) <= 0)
			continue;

		// Skip must be at least one.
		Rend_DecorationPatternSkip(lightDef, skip);

		pos[VY] = (int)(sin->bounds[BTOP] / tileSize) * tileSize - offY
			- lightDef->pos[VY]
			- lightDef->pattern_offset[VY] * tileSize;
		while(pos[VY] > sin->bounds[BTOP]) 
			pos[VY] -= tileSize * skip[VY];

		for(; pos[VY] < sin->bounds[BBOTTOM]; pos[VY] += tileSize * skip[VY])
		{
			if(pos[VY] < sin->bounds[BTOP]) continue;
			pos[VX] = (int)(sin->bounds[BLEFT] / tileSize) * tileSize - offX
				+ lightDef->pos[VX]
				- lightDef->pattern_offset[VX] * tileSize;
			while(pos[VX] > sin->bounds[BLEFT]) 
				pos[VX] -= tileSize * skip[VX];

			for(; pos[VX] < sin->bounds[BRIGHT]; 
				pos[VX] += tileSize * skip[VX])
			{
				if(pos[VX] < sin->bounds[BLEFT]) continue;

				// The point must be inside the correct sector.
				if(!R_IsPointInSector(pos[VX] * FRACUNIT, pos[VY] * FRACUNIT, 
					sector)) continue;

				pos[VZ] = z + lightDef->elevation * elevateDir;
				Rend_AddLightDecoration(pos, lightDef, brightMul, false,
					def->pregen_lightmap);
			}
		}
	}
}

/*
 * Generate decorations for the planes of the sector.
 */
void Rend_DecorateSector(int index)
{
	sector_t *sector = SECTOR_PTR(index);
	ded_decor_t *def;

	// The sector must have height if it wants decorations.
	if(sector->ceilingheight <= sector->floorheight) 
		return;

	// Is this sector close enough for the decorations to be visible?
	if(!Rend_SectorDecorationBounds(sector, &secinfo[index]))
		return;

	if((def = Rend_GetFlatDecoration(sector->floorpic)) != NULL)
	{
		// The floor is decorated.
		Rend_DecoratePlane(index, SECT_FLOOR(sector), 1,
			sector->flooroffx, sector->flooroffy, def);
	}

	if((def = Rend_GetFlatDecoration(sector->ceilingpic)) != NULL)
	{
		// The ceiling is decorated.
		Rend_DecoratePlane(index, SECT_CEIL(sector), -1,
			sector->ceiloffx, sector->ceiloffy, def);
	}
}

/*
 * Decorations are generated for each frame.
 */
void Rend_InitDecorationsForFrame(void)
{
	int i;

	Rend_ClearDecorations();

	// This only needs to be done if decorations have been enabled.
	if(!useDecorations) return;

	// Process all lines. This could also be done during sectors,
	// but validcount would need to be used to prevent duplicate 
	// processing.
	for(i = 0; i < numlines; i++)
		Rend_DecorateLine(i);

	// Process all planes.
	for(i = 0; i < numsectors; i++)
		Rend_DecorateSector(i);
}

