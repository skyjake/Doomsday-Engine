// Calculation of the aural properties of sectors.

#include "DoomDef.h"
#include "R_local.h"
#include <ctype.h>

enum
{
	BLEFT, BTOP, BRIGHT, BBOTTOM
};

enum // Texture types
{
	TEXTYPE_UNKNOWN,
	TEXTYPE_METAL,
	TEXTYPE_ROCK,
	TEXTYPE_WOOD,
	TEXTYPE_CLOTH
};

// This could hold much more detailed information...
typedef struct
{
	char	name[9];	// Name of the texture.
	int		type;		// Which type?
} textype_t;

void R_LoadTextureTypes();
void R_FreeTextureTypes();
int R_TextureTypeForName(char *name);

static textype_t	*texTypes = 0;
static int			numTexTypes = 0;


// (0,1) = top left; (2,3) = bottom right
// Assumes sectors are always closed.
void P_SectorBoundingBox(sector_t *sec, float *bbox)
{
	float	x, y;
	int		i;
	line_t	*li;
	
	bbox[BLEFT] = bbox[BRIGHT] = sec->Lines[0]->v1->x >> FRACBITS;
	bbox[BTOP] = bbox[BBOTTOM] = sec->Lines[0]->v1->y >> FRACBITS;
	for(i=1; i<sec->linecount; i++)
	{
		li = sec->Lines[i];
		x = li->v1->x >> FRACBITS;
		y = li->v1->y >> FRACBITS;
		if(x < bbox[BLEFT]) bbox[BLEFT] = x;
		if(x > bbox[BRIGHT]) bbox[BRIGHT] = x;
		if(y < bbox[BTOP]) bbox[BTOP] = y;
		if(y > bbox[BBOTTOM]) bbox[BBOTTOM] = y;
	}
}

// Calculate the reverb settings for each sector.
void P_CalcSectorReverbs()
{
	int			i, c, type, k;
	subsector_t	*sub;
	sector_t	*sec;
	seg_t		*seg;
//	float		volume;
	float		total, metal, rock, wood, cloth;

	//gi.Message( "P_CalcSectorReverbs: begin at %i\n", gi.GetTime());

	// First determine each subsectors' individual characteristics.
	for(c=0, sub=subsectors; c<numsubsectors; c++, sub++)
	{
		// Space is the rough volume of the subsector (bounding box).
		sub->reverb[SSRD_SPACE] = ((sub->sector->ceilingheight - sub->sector->floorheight) >> FRACBITS)
			* (sub->bbox[1].x - sub->bbox[0].x) * (sub->bbox[1].y - sub->bbox[0].y);
		//gi.Message( "sub %i: volume %f Mu\n", c, volume/1e6);
/*	i = (int) (volume/1e4);
		if(i < 5) i = 5;
		if(i > 255) i = 255;
		sub->reverb[SSRD_SPACE] = i;*/
		
		// The other reverb properties can be found out by taking a look at the
		// walls surrounding the subsector.
		total = metal = rock = wood = cloth = 0;
		for(i=0, seg=segs+sub->firstline; i<sub->numLines; i++, seg++)
		{
			if(!seg->linedef || !seg->sidedef->midtexture) continue;
			total += seg->length;
			// The texture of the seg determines its type.
			type = R_TextureTypeForName(gi.R_TextureNameForNum(seg->sidedef->midtexture));
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
		sub->reverb[SSRD_VOLUME] = i;

		// Decay time.
		i = (int) ( metal*255 + rock*160 + wood*50 + cloth*5 );
		if(i < 0) i = 0;
		if(i > 255) i = 255;
		sub->reverb[SSRD_DECAY] = i;

		// High frequency damping.
		i = (int) ( metal*25 + rock*100 + wood*200 + cloth*255 );
		if(i < 0) i = 0;
		if(i > 255) i = 255;
		sub->reverb[SSRD_DAMPING] = i;

		// The floor and sky also have an effect, especially is there is 
		// sky involved.
/*		if(sub->sector->ceilingpic == skyflatnum)
		{
			sub->reverb[SSRD_VOLUME] /= 1.5;
			sub->reverb[SSRD_DECAY] /= 2;
		}*/

/*		gi.Message( "sub %04i: vol:%3i sp:%3i dec:%3i dam:%3i\n", c, sub->reverb[SSRD_VOLUME],
			sub->reverb[SSRD_SPACE], sub->reverb[SSRD_DECAY], sub->reverb[SSRD_DAMPING]);*/
	}
	
	for(c=0, sec=sectors; c<numsectors; c++, sec++)
	{
		float bbox[4], spaceScatter;
		unsigned int sectorSpace;
		P_SectorBoundingBox(sec, bbox);
		//gi.Message( "sector %i: (%f,%f) - (%f,%f)\n", c, bbox[BLEFT], bbox[BTOP], bbox[BRIGHT], bbox[BBOTTOM]);

		sectorSpace = ((sec->ceilingheight - sec->floorheight) >> FRACBITS)
			* (bbox[BRIGHT] - bbox[BLEFT]) * (bbox[BBOTTOM] - bbox[BTOP]);

		bbox[BLEFT] -= 128;
		bbox[BRIGHT] += 128;
		bbox[BTOP] -= 128;
		bbox[BBOTTOM] += 128;

		for(i=0, k=0, sub=subsectors; i<numsubsectors; i++, sub++)
		{
			// Is this subsector close enough?
			if(sub->midpoint.x > bbox[BLEFT] && sub->midpoint.x < bbox[BRIGHT] 
				&& sub->midpoint.y > bbox[BTOP] && sub->midpoint.y < bbox[BBOTTOM])
			{
				//gi.Message( "- sub %i within, own:%i\n", i, sub->sector == sec);
				k++; 
				total = sub->reverb[SSRD_SPACE]/* / 255.0f*/;
				sec->reverbSpace += total;
				sec->reverbVolume += sub->reverb[SSRD_VOLUME] / 255.0f * total;
				sec->reverbDecay += sub->reverb[SSRD_DECAY] / 255.0f * total;
				sec->reverbDamping += sub->reverb[SSRD_DAMPING] / 255.0f * total;
			}
		}
		if(sec->reverbSpace)
		{
			spaceScatter = sectorSpace / (float) sec->reverbSpace;
			// These three are weighted by the space.
			sec->reverbVolume /= sec->reverbSpace;
			sec->reverbDecay /= sec->reverbSpace;
			sec->reverbDamping /= sec->reverbSpace;
		}
		else
		{
			spaceScatter = 0;
			sec->reverbVolume = .2f;
			sec->reverbDecay = .4f;
			sec->reverbDamping = 1;
		}
		//gi.Message( "sector %i: secSp:%fM revSp:%fM scatter: %f\n", c, sectorSpace/1e6f, sec->reverbSpace/1e6f, spaceScatter);

		// If the space is scattered, the reverb effect lessens.
		sec->reverbSpace /= spaceScatter > .8? 10 : spaceScatter > .6? 4 : 1;

		// Scale the reverb space to a reasonable range, so that 0 is very small and
		// .99 is very large. 1.0 is only for open areas.
		sec->reverbSpace /= 120e6;
		if(sec->reverbSpace > .99) sec->reverbSpace = .99f;
		
		if(sec->ceilingpic == skyflatnum) // An open sector?
		{
			// An open sector can still be small. In that case the reverb
			// is diminished a bit.
			if(sec->reverbSpace > .5) 
				sec->reverbVolume = 1; // Full volume.
			else 
				sec->reverbVolume = .5f; // Small sector, but still open.
			sec->reverbSpace = 1;
		}
		else // A closed sector.
		{
			// Large spaces have automatically a bit more audible reverb.
			sec->reverbVolume += sec->reverbSpace / 4;
		}
		if(sec->reverbVolume > 1) sec->reverbVolume = 1;
/*		sec->reverbDecay /= k/2.0f;
		sec->reverbDamping /= k;*/
	}

	//gi.Message( "P_CalcSectorReverbs: end at %i\n", gi.GetTime());
}


//----------------------------------------------------------------------
// Texture types
//----------------------------------------------------------------------

static void readline(char *buffer, int len, FILE *file)
{
	int		p;

	fgets(buffer, len, file);
	p = strlen(buffer)-1;
	if(buffer[p] == '\n') buffer[p] = 0;
}

static char *firstchar(char *buffer)
{
	int		i = 0;

	while(isspace(buffer[i]) && buffer[i]) i++;
	return buffer + i;
}

void R_LoadTextureTypes()
{
	FILE	*file;
	char	buff[256], *ptr;
	int		curtype = TEXTYPE_UNKNOWN;
	textype_t *tt;

	if((file=fopen("textypes.txt", "rt")) == NULL)
		return;

	for(readline(buff, 255, file); !feof(file); readline(buff, 255, file))
	{
		ptr = firstchar(buff);
		if(*ptr == '#') continue; // A comment.
		if(*ptr == '*')
		{
			ptr = firstchar(ptr+1);
			if(!stricmp(ptr, "metal"))
				curtype = TEXTYPE_METAL;
			else if(!stricmp(ptr, "rock"))
				curtype = TEXTYPE_ROCK;
			else if(!stricmp(ptr, "wood"))
				curtype = TEXTYPE_WOOD;
			else if(!stricmp(ptr, "cloth"))
				curtype = TEXTYPE_CLOTH;
			continue;
		}
		// Allocate a new textype entry.
		texTypes = realloc(texTypes, sizeof(textype_t) * ++numTexTypes);
		tt = texTypes + numTexTypes-1;
		memset(tt, 0, sizeof(*tt));
		strncpy(tt->name, ptr, 8);
		tt->type = curtype;
	}
	gi.Message( "%d texture types loaded.\n", numTexTypes);
	fclose(file);
}

void R_FreeTextureTypes()
{
	free(texTypes);
	texTypes = 0;
	numTexTypes = 0;
}

int R_TextureTypeForName(char *name)
{
	int		i;

	for(i=0; i<numTexTypes; i++)
		if(!strnicmp(name, texTypes[i].name, 8))
			return texTypes[i].type;
	return TEXTYPE_UNKNOWN;
}
