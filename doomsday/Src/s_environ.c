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
 * s_environ.c: Environmental Sound Effects
 *
 * Calculation of the aural properties of sectors.
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>

#include "de_base.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_audio.h"

// MACROS ------------------------------------------------------------------

enum // Texture types
{
	TEXTYPE_UNKNOWN,
	TEXTYPE_METAL,
	TEXTYPE_ROCK,
	TEXTYPE_WOOD,
	TEXTYPE_CLOTH
};

// TYPES -------------------------------------------------------------------

typedef struct
{
	unsigned int data[NUM_REVERB_DATA];
} subreverb_t;

// This could hold much more detailed information...
typedef struct
{
	char	name[9];	// Name of the texture.
	int		type;		// Which type?
} textype_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// S_TextureTypeForName
//===========================================================================
int S_TextureTypeForName(char *name)
{
	int	i, k;
	ded_tenviron_t *env;

	for(i = 0, env = defs.tenviron; i < defs.count.tenviron.num; i++, env++)
	{
		for(k = 0; k < env->count.num; k++)
			if(!stricmp(env->textures[k].str, name))
			{
				// A match!
				if(!stricmp(env->id, "Metal")) return TEXTYPE_METAL;
				if(!stricmp(env->id, "Rock")) return TEXTYPE_ROCK;
				if(!stricmp(env->id, "Wood")) return TEXTYPE_WOOD;
				if(!stricmp(env->id, "Cloth")) return TEXTYPE_CLOTH;
				return TEXTYPE_UNKNOWN;
			}
	}
	return TEXTYPE_UNKNOWN;
}

//===========================================================================
// S_CalcSectorReverbs
//	Calculate the reverb settings for each sector.
//===========================================================================
void S_CalcSectorReverbs(void)
{
	int			i, c, type, k;
	subsector_t	*sub;
	sector_t	*sec;
	seg_t		*seg;
	float		total, metal, rock, wood, cloth;
	subreverb_t *sub_reverb, *rev;

	// Allocate memory for the subsector temporary reverb data.
	sub_reverb = (subreverb_t*) Z_Malloc(sizeof(subreverb_t) * numsubsectors,
		PU_STATIC, 0);
	memset(sub_reverb, 0, sizeof(subreverb_t) * numsubsectors);

	// First determine each subsectors' individual characteristics.
	//ST_Message("%d bytes; sub_reverb: %p\n", sizeof(subreverb_t) * numsubsectors, sub_reverb);
	for(c=0; c<numsubsectors; c++)
	{
		sub = SUBSECTOR_PTR(c);
		rev = &sub_reverb[c];
		// Space is the rough volume of the subsector (bounding box).
		rev->data[SRD_SPACE] = ((sub->sector->ceilingheight - sub->sector->floorheight) >> FRACBITS)
			* (sub->bbox[1].x - sub->bbox[0].x) * (sub->bbox[1].y - sub->bbox[0].y);

		//gi.Message( "sub %i: volume %f Mu\n", c, volume/1e6);
		/*	
		i = (int) (volume/1e4);
		if(i < 5) i = 5;
		if(i > 255) i = 255;
		sub->reverb[SSRD_SPACE] = i;
		*/
		
		// The other reverb properties can be found out by taking a look at the
		// walls surrounding the subsector (floors and ceilings are currently
		// ignored).
		total = metal = rock = wood = cloth = 0;
		for(i = 0; i < sub->linecount; i++)
		{
			seg = SEG_PTR(sub->firstline + i);
			if(!seg->linedef || !seg->sidedef || !seg->sidedef->midtexture) 
				continue;
			total += seg->length;
			// The texture of the seg determines its type.
			type = S_TextureTypeForName(R_TextureNameForNum(seg->sidedef->midtexture));
			switch(type)
			{
			case TEXTYPE_METAL:
				metal += seg->length;
				break;

			case TEXTYPE_ROCK:
				rock += seg->length;
				break;

			case TEXTYPE_WOOD:
				wood += seg->length;
				break;

			case TEXTYPE_CLOTH:
				cloth += seg->length;
				break;

			default:
				// The type of the texture is unknown. Assume it's wood.
				wood += seg->length;
			}
		}
		if(!total) continue; // Huh?
		metal /= total;
		rock /= total;
		wood /= total;
		cloth /= total;

		// Volume.
		i = (int) ( metal*255 + rock*200 + wood*80 + cloth*5 );
		if(i < 0) i = 0;
		if(i > 255) i = 255;
		rev->data[SRD_VOLUME] = i;

		// Decay time.
		i = (int) ( metal*255 + rock*160 + wood*50 + cloth*5 );
		if(i < 0) i = 0;
		if(i > 255) i = 255;
		rev->data[SRD_DECAY] = i;

		// High frequency damping.
		i = (int) ( metal*25 + rock*100 + wood*200 + cloth*255 );
		if(i < 0) i = 0;
		if(i > 255) i = 255;
		rev->data[SRD_DAMPING] = i;

		/*ST_Message( "sub %04i: vol:%3i sp:%3i dec:%3i dam:%3i\n", c, rev->data[SRD_VOLUME],
			rev->data[SRD_SPACE], rev->data[SRD_DECAY], rev->data[SRD_DAMPING]);*/
	}
/*	ST_Message("sub_reverb: %p\n", sub_reverb);
	for(c=0; c<numsubsectors; c++)
	{
		rev = &sub_reverb[c];
		ST_Message( "sub %04i: vol:%3i sp:%3i dec:%3i dam:%3i\n", c, rev->data[SRD_VOLUME],
			rev->data[SRD_SPACE], rev->data[SRD_DECAY], rev->data[SRD_DAMPING]);
	}*/
	
	for(c = 0; c < numsectors; c++)
	{
		float bbox[4], spaceScatter;
		unsigned int sectorSpace;
		sec = SECTOR_PTR(c);
		//DD_SectorBoundingBox(sec, bbox);
		memcpy(bbox, secinfo[c].bounds, sizeof(bbox));
		/*ST_Message( "sector %i: (%f,%f) - (%f,%f)\n", c, 
			bbox[BLEFT], bbox[BTOP], bbox[BRIGHT], bbox[BBOTTOM]);*/

		sectorSpace = ((sec->ceilingheight - sec->floorheight) >> FRACBITS)
			* (bbox[BRIGHT] - bbox[BLEFT]) * (bbox[BBOTTOM] - bbox[BTOP]);

		//ST_Message("sector %i: secsp:%i\n", c, sectorSpace);

		bbox[BLEFT] -= 128;
		bbox[BRIGHT] += 128;
		bbox[BTOP] -= 128;
		bbox[BBOTTOM] += 128;

		for(k=0, i=0; i<numsubsectors; i++)
		{
			sub = SUBSECTOR_PTR(i);
			rev = sub_reverb + i;
			// Is this subsector close enough?
			if(sub->midpoint.x > bbox[BLEFT] && sub->midpoint.x < bbox[BRIGHT] 
				&& sub->midpoint.y > bbox[BTOP] && sub->midpoint.y < bbox[BBOTTOM])
			{
				//ST_Message( "- sub %i within, own:%i\n", i, sub->sector == sec);
				k++; 
				sec->reverb[SRD_SPACE] += rev->data[SRD_SPACE];
				sec->reverb[SRD_VOLUME] += rev->data[SRD_VOLUME] / 255.0f * rev->data[SRD_SPACE];
				sec->reverb[SRD_DECAY] += rev->data[SRD_DECAY] / 255.0f * rev->data[SRD_SPACE];
				sec->reverb[SRD_DAMPING] += rev->data[SRD_DAMPING] / 255.0f * rev->data[SRD_SPACE];
			}
		}
		if(sec->reverb[SRD_SPACE])
		{
			spaceScatter = sectorSpace / sec->reverb[SRD_SPACE];
			// These three are weighted by the space.
			sec->reverb[SRD_VOLUME] /= sec->reverb[SRD_SPACE];
			sec->reverb[SRD_DECAY] /= sec->reverb[SRD_SPACE];
			sec->reverb[SRD_DAMPING] /= sec->reverb[SRD_SPACE];
/*			ST_Message("sector %i: sp:%f vol:%f dec:%f dam:%f\n",
				c,
				sec->reverb[SRD_SPACE], 
				sec->reverb[SRD_VOLUME], 
				sec->reverb[SRD_DECAY], 
				sec->reverb[SRD_DAMPING]); */
		}
		else
		{
			spaceScatter = 0;
			sec->reverb[SRD_VOLUME] = .2f;
			sec->reverb[SRD_DECAY] = .4f;
			sec->reverb[SRD_DAMPING] = 1;
		}
		/*ST_Message( "sector %i: secSp:%fM revSp:%fM scatter: %f\n", 
			c, sectorSpace/1e6f, sec->reverb[SRD_SPACE]/1e6f, spaceScatter);*/

		// If the space is scattered, the reverb effect lessens.
		sec->reverb[SRD_SPACE] /= spaceScatter > .8? 10 : spaceScatter > .6? 4 : 1;

		// Scale the reverb space to a reasonable range, so that 0 is very small and
		// .99 is very large. 1.0 is only for open areas.
		sec->reverb[SRD_SPACE] /= 120e6;
		if(sec->reverb[SRD_SPACE] > .99) sec->reverb[SRD_SPACE] = .99f;
		
		if(sec->ceilingpic == skyflatnum) // An open sector?
		{
			// An open sector can still be small. In that case the reverb
			// is diminished a bit.
			if(sec->reverb[SRD_SPACE] > .5) 
				sec->reverb[SRD_VOLUME] = 1; // Full volume.
			else 
				sec->reverb[SRD_VOLUME] = .5f; // Small sector, but still open.
			sec->reverb[SRD_SPACE] = 1;
		}
		else // A closed sector.
		{
			// Large spaces have automatically a bit more audible reverb.
			sec->reverb[SRD_VOLUME] += sec->reverb[SRD_SPACE] / 4;
		}
		if(sec->reverb[SRD_VOLUME] > 1) sec->reverb[SRD_VOLUME] = 1;
/*		sec->reverbDecay /= k/2.0f;
		sec->reverbDamping /= k;*/
	}

	Z_Free(sub_reverb);
		
	//gi.Message( "P_CalcSectorReverbs: end at %i\n", gi.GetTime());
}


