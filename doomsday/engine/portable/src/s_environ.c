/**\file
 *\section Copyright and License Summary
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

#include "m_misc.h"

// MACROS ------------------------------------------------------------------

enum                            // Texture types
{
    TEXTYPE_UNKNOWN,
    TEXTYPE_METAL,
    TEXTYPE_ROCK,
    TEXTYPE_WOOD,
    TEXTYPE_CLOTH
};

// TYPES -------------------------------------------------------------------

typedef struct {
    unsigned int data[NUM_REVERB_DATA];
} subreverb_t;

// This could hold much more detailed information...
typedef struct {
    char    name[9];            // Name of the texture.
    int     type;               // Which type?
} textype_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

int S_TextureTypeForName(char *name)
{
    int     i, k;
    ded_tenviron_t *env;

    for(i = 0, env = defs.tenviron; i < defs.count.tenviron.num; ++i, env++)
    {
        for(k = 0; k < env->count.num; ++k)
            if(!stricmp(env->textures[k].str, name))
            {
                // A match!
                if(!stricmp(env->id, "Metal"))
                    return TEXTYPE_METAL;
                if(!stricmp(env->id, "Rock"))
                    return TEXTYPE_ROCK;
                if(!stricmp(env->id, "Wood"))
                    return TEXTYPE_WOOD;
                if(!stricmp(env->id, "Cloth"))
                    return TEXTYPE_CLOTH;
                return TEXTYPE_UNKNOWN;
            }
    }
    return TEXTYPE_UNKNOWN;
}

/*
 * Calculate the reverb settings for each sector.
 */
void S_CalcSectorReverbs(void)
{
    int     i, c, type, k;
    int     j;
    subsector_t *sub;
    sector_t *sec;
    seg_t  *seg;
    float   total, metal, rock, wood, cloth;
    subreverb_t *sub_reverb, *rev;

    // Allocate memory for the subsector temporary reverb data.
    sub_reverb = M_Malloc(sizeof(subreverb_t) * numsubsectors);
    memset(sub_reverb, 0, sizeof(subreverb_t) * numsubsectors);

/*
#if _DEBUG
Con_Message("%d bytes; sub_reverb: %p\n", sizeof(subreverb_t) * numsubsectors, sub_reverb);
#endif
*/
    // First determine each subsectors' individual characteristics.
    for(c = 0; c < numsubsectors; ++c)
    {
        sub = SUBSECTOR_PTR(c);
        rev = &sub_reverb[c];
        // Space is the rough volume of the subsector (bounding box).
        rev->data[SRD_SPACE] =
            ((sub->sector->planes[PLN_CEILING]->height -
              sub->sector->planes[PLN_FLOOR]->height) >> FRACBITS) * (sub->bbox[1].pos[VX] -
                                                        sub->bbox[0].pos[VX]) *
            (sub->bbox[1].pos[VY] - sub->bbox[0].pos[VY]);

        // The other reverb properties can be found out by taking a look at the
        // walls surrounding the subsector (floors and ceilings are currently
        // ignored).
        total = metal = rock = wood = cloth = 0;
        seg = (seg_t *) segs;
        for(j = 0; j < sub->linecount; ++j, seg++)
        {
            if(!seg->linedef || !seg->sidedef || !seg->sidedef->middle.texture)
                continue;
            total += seg->length;
            // The texture of the seg determines its type.
            if(seg->sidedef->middle.texture == -1)
            {
                type = TEXTYPE_WOOD;
            }
            else
                type =
                    S_TextureTypeForName(R_TextureNameForNum
                                         (seg->sidedef->middle.texture));
            switch (type)
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
        if(!total)
            continue;           // Huh?
        metal /= total;
        rock /= total;
        wood /= total;
        cloth /= total;

        // Volume.
        i = (int) (metal * 255 + rock * 200 + wood * 80 + cloth * 5);
        if(i < 0)
            i = 0;
        if(i > 255)
            i = 255;
        rev->data[SRD_VOLUME] = i;

        // Decay time.
        i = (int) (metal * 255 + rock * 160 + wood * 50 + cloth * 5);
        if(i < 0)
            i = 0;
        if(i > 255)
            i = 255;
        rev->data[SRD_DECAY] = i;

        // High frequency damping.
        i = (int) (metal * 25 + rock * 100 + wood * 200 + cloth * 255);
        if(i < 0)
            i = 0;
        if(i > 255)
            i = 255;
        rev->data[SRD_DAMPING] = i;

/*
#if _DEBUG
Con_Message("sub %04i: vol:%3i sp:%3i dec:%3i dam:%3i\n", c, rev->data[SRD_VOLUME],
            rev->data[SRD_SPACE], rev->data[SRD_DECAY], rev->data[SRD_DAMPING]);
#endif
*/
    }

/*
#if _DEBUG
Con_Message("sub_reverb: %p\n", sub_reverb);
for(c=0; c < numsubsectors; ++c)
{
    rev = &sub_reverb[c];
    Con_Message("sub %04i: vol:%3i sp:%3i dec:%3i dam:%3i\n", c, rev->data[SRD_VOLUME],
                rev->data[SRD_SPACE], rev->data[SRD_DECAY], rev->data[SRD_DAMPING]);
}
#endif
*/

    for(c = 0; c < numsectors; ++c)
    {
        float   bbox[4], spaceScatter;
        unsigned int sectorSpace;

        sec = SECTOR_PTR(c);
        memcpy(bbox, sec->info->bounds, sizeof(bbox));
/*
#if _DEBUG
Con_Message("sector %i: (%f,%f) - (%f,%f)\n", c,
            bbox[BLEFT], bbox[BTOP], bbox[BRIGHT], bbox[BBOTTOM]);
#endif
*/

        sectorSpace =
            ((sec->planes[PLN_CEILING]->height -
              sec->planes[PLN_FLOOR]->height) >> FRACBITS) * (bbox[BRIGHT] -
                                                bbox[BLEFT]) * (bbox[BBOTTOM] -
                                                                bbox[BTOP]);
/*
#if _DEBUG
Con_Message("sector %i: secsp:%i\n", c, sectorSpace);
#endif
*/
        bbox[BLEFT]   -= 128;
        bbox[BRIGHT]  += 128;
        bbox[BTOP]    -= 128;
        bbox[BBOTTOM] += 128;

        for(k = 0, i = 0; i < numsubsectors; ++i)
        {
            sub = SUBSECTOR_PTR(i);
            rev = sub_reverb + i;
            // Is this subsector close enough?
            if(sub->sector == sec || // subsector is IN this sector
               (sub->midpoint.pos[VX] > bbox[BLEFT] &&
                sub->midpoint.pos[VX] < bbox[BRIGHT] &&
                sub->midpoint.pos[VY] > bbox[BTOP] &&
                sub->midpoint.pos[VY] < bbox[BBOTTOM]))
            {
/*
#if _DEBUG
Con_Message( "- sub %i within, own:%i\n", i, sub->sector == sec);
#endif
*/
                k++;
                sec->reverb[SRD_SPACE]   += rev->data[SRD_SPACE];

                sec->reverb[SRD_VOLUME]  +=
                    rev->data[SRD_VOLUME]  / 255.0f * rev->data[SRD_SPACE];
                sec->reverb[SRD_DECAY]   +=
                    rev->data[SRD_DECAY]   / 255.0f * rev->data[SRD_SPACE];
                sec->reverb[SRD_DAMPING] +=
                    rev->data[SRD_DAMPING] / 255.0f * rev->data[SRD_SPACE];
            }
        }
        if(sec->reverb[SRD_SPACE])
        {
            spaceScatter = sectorSpace / sec->reverb[SRD_SPACE];
            // These three are weighted by the space.
            sec->reverb[SRD_VOLUME]  /= sec->reverb[SRD_SPACE];
            sec->reverb[SRD_DECAY]   /= sec->reverb[SRD_SPACE];
            sec->reverb[SRD_DAMPING] /= sec->reverb[SRD_SPACE];
        }
        else
        {
            spaceScatter = 0;
            sec->reverb[SRD_VOLUME]  = .2f;
            sec->reverb[SRD_DECAY]   = .4f;
            sec->reverb[SRD_DAMPING] = 1;
        }

        // If the space is scattered, the reverb effect lessens.
        sec->reverb[SRD_SPACE] /= spaceScatter > .8 ? 10 : spaceScatter >
            .6 ? 4 : 1;

        // Scale the reverb space to a reasonable range, so that 0 is very small and
        // .99 is very large. 1.0 is only for open areas.
        sec->reverb[SRD_SPACE] /= 120e6;
        if(sec->reverb[SRD_SPACE] > .99)
            sec->reverb[SRD_SPACE] = .99f;

        if(R_IsSkySurface(&sec->SP_ceilsurface))   // An open sector?
        {
            // An open sector can still be small. In that case the reverb
            // is diminished a bit.
            if(sec->reverb[SRD_SPACE] > .5)
                sec->reverb[SRD_VOLUME] = 1;    // Full volume.
            else
                sec->reverb[SRD_VOLUME] = .5f;  // Small sector, but still open.
            sec->reverb[SRD_SPACE] = 1;
        }
        else                    // A closed sector.
        {
            // Large spaces have automatically a bit more audible reverb.
            sec->reverb[SRD_VOLUME] += sec->reverb[SRD_SPACE] / 4;
        }

        if(sec->reverb[SRD_VOLUME] > 1)
            sec->reverb[SRD_VOLUME] = 1;
    }

    M_Free(sub_reverb);
}
