/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006-2007 Daniel Swanson <danij@dengine.net>
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
#include <string.h>

#include "de_base.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_audio.h"

#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef struct {
    const char  name[9];    // Material type name.
    int         volumeMul;
    int         decayMul;
    int         dampingMul;
} materialinfo_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static materialinfo_t matInfo[NUM_MATERIAL_TYPES] = {
    {"Metal",     255,     255,    25},
    {"Rock",      200,     160,    100},
    {"Wood",      80,      50,     200},
    {"Cloth",     5,       5,      255}
};

static ownernode_t *unusedNodeList = NULL;

// CODE --------------------------------------------------------------------

/**
 * Given a texture/flat name, look up the associated material type.
 *
 * @param name          Name of the texture/flat to look up.
 * @param isFlat        <code>true</code> = look for name among flats.
 *
 * @return              If found; material type associated to the texture,
 *                      else <code>MATTYPE_UNKNOWN</code>.
 */
materialtype_t S_MaterialTypeForName(const char *name, boolean isFlat)
{
    int         i, j, count;
    materialtype_t k;
    ded_tenviron_t *env;
    ded_str_t  *list;

    for(i = 0, env = defs.tenviron; i < defs.count.tenviron.num; ++i, env++)
    {
        if(isFlat)
        {
            list = env->textures;
            count = env->texCount.num;
        }
        else
        {
            list = env->flats;
            count = env->flatCount.num;
        }

        for(j = 0; j < count; ++j)
            if(!stricmp(list[j].str, name))
            {   // A match!
                // See if we recognise the material name.
                for(k = 0; k < NUM_MATERIAL_TYPES; ++k)
                    if(!stricmp(env->id, matInfo[k].name))
                        return k;

                return MATTYPE_UNKNOWN;
            }
    }

    return MATTYPE_UNKNOWN;
}

static ownernode_t *newOwnerNode(void)
{
    ownernode_t *node;

    if(unusedNodeList)
    {   // An existing node is available for re-use.
        node = unusedNodeList;
        unusedNodeList = unusedNodeList->next;

        node->next = NULL;
        node->data = NULL;
    }
    else
    {   // Need to allocate another.
        node = M_Malloc(sizeof(ownernode_t));
    }

    return node;
}

static void setSubSecSectorOwner(ownerlist_t *ownerList, subsector_t *ssec)
{
    ownernode_t *node;

    if(!ssec)
        return;

    // Add a new owner.
    // NOTE: No need to check for duplicates.
    ownerList->count++;

    node = newOwnerNode();
    node->data = ssec;
    node->next = ownerList->head;
    ownerList->head = node;
}

/**
 * Called during level init to determine which subsectors affect the reverb
 * properties of all sectors. Given that subsectors do not change shape (in
 * two dimensions at least), they do not move and are not created/destroyed
 * once the map has been loaded; this step can be pre-processed.
 */
void S_DetermineSubSecsAffectingSectorReverb(void)
{
    uint        startTime = Sys_GetRealTime();

    uint        i, k;
    subsector_t *sub;
    sector_t   *sec;
    ownernode_t *node, *p;

    for(i = 0; i < numsectors; ++i)
    {
        float       bbox[4];
        ownerlist_t subSecOwnerList;

        sec = SECTOR_PTR(i);
        memset(&subSecOwnerList, 0, sizeof(subSecOwnerList));

        memcpy(bbox, sec->bounds, sizeof(bbox));
        bbox[BLEFT]   -= 128;
        bbox[BRIGHT]  += 128;
        bbox[BTOP]    -= 128;
        bbox[BBOTTOM] += 128;
/*
#if _DEBUG
Con_Message("sector %i: (%f,%f) - (%f,%f)\n", c,
            bbox[BLEFT], bbox[BTOP], bbox[BRIGHT], bbox[BBOTTOM]);
#endif
*/
        for(k = 0; k < numsubsectors; ++k)
        {
            sub = SUBSECTOR_PTR(k);

            // Is this subsector close enough?
            if(sub->sector == sec || // subsector is IN this sector
               (sub->midpoint.pos[VX] > bbox[BLEFT] &&
                sub->midpoint.pos[VX] < bbox[BRIGHT] &&
                sub->midpoint.pos[VY] > bbox[BTOP] &&
                sub->midpoint.pos[VY] < bbox[BBOTTOM]))
            {
                // It will contribute to the reverb settings of this sector.
                setSubSecSectorOwner(&subSecOwnerList, sub);
            }
        }

        // Now harden the list.
        sec->numReverbSSecAttributors = subSecOwnerList.count;
        if(sec->numReverbSSecAttributors)
        {
            subsector_t **ptr;

            sec->reverbSSecs =
                Z_Malloc(sec->numReverbSSecAttributors * sizeof(subsector_t*),
                         PU_LEVELSTATIC, 0);

            for(k = 0, ptr = sec->reverbSSecs, node = subSecOwnerList.head;
                k < sec->numReverbSSecAttributors; ++k, ptr++)
            {
                p = node->next;
                *ptr = (subsector_t*) node->data;

                if(i < numsectors - 1)
                {   // Move this node to the unused list for re-use.
                    node->next = unusedNodeList;
                    unusedNodeList = node;
                }
                else
                {   // No further use for the nodes.
                    M_Free(node);
                }
                node = p;
            }
        }
    }

    // Free any nodes left in the unused list.
    node = unusedNodeList;
    while(node)
    {
        p = node->next;
        M_Free(node);
        node = p;
    }
    unusedNodeList = NULL;

    // How much time did we spend?
    VERBOSE(Con_Message
            ("S_DetermineSubSecsAffectingSectorReverb: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}

static boolean calcSSecReverb(subsector_t *ssec)
{
    uint        i, v;
    seg_t      *seg;
    float       total = 0;
    materialtype_t type;
    float       materials[NUM_MATERIAL_TYPES];

    memset(&materials, 0, sizeof(materials));

    // Space is the rough volume of the subsector (bounding box).
    ssec->reverb[SRD_SPACE] =
        (int) (ssec->sector->SP_ceilheight - ssec->sector->SP_floorheight) *
        (ssec->bbox[1].pos[VX] - ssec->bbox[0].pos[VX]) *
        (ssec->bbox[1].pos[VY] - ssec->bbox[0].pos[VY]);

    // The other reverb properties can be found out by taking a look at the
    // materials of all surfaces in the subsector.
    for(i = 0, seg = ssec->firstseg; i < ssec->segcount; ++i, seg++)
    {
        if(!seg->linedef || !seg->sidedef ||
           !seg->sidedef->SW_middletexture)
            continue;

        // The texture of the seg determines its type.
        if(seg->sidedef->SW_middletexture >= 0)
        {
            if(seg->sidedef->SW_middleisflat)
                type = flats[seg->sidedef->SW_middletexture]->materialType;
            else
                type = textures[seg->sidedef->SW_middletexture]->materialType;
        }
        else
        {
            type = MATTYPE_UNKNOWN;
        }

        total += seg->length;
        if(!(type >= 0 && type < NUM_MATERIAL_TYPES))
            type = MATTYPE_WOOD; // Assume it's wood if unknown.
        materials[type] += seg->length;
    }

    if(!total)
    {   // Huh?
        ssec->reverb[SRD_VOLUME] = ssec->reverb[SRD_DECAY] =
            ssec->reverb[SRD_DAMPING] = 0;
        return false;
    }

    // Average the results.
    for(i = 0; i < NUM_MATERIAL_TYPES; ++i)
        materials[i] /= total;

    // Volume.
    for(i = 0, v = 0; i < NUM_MATERIAL_TYPES; ++i)
        v += materials[i] * matInfo[i].volumeMul;
    if(v < 0)
        v = 0;
    else if(v > 255)
        v = 255;
    ssec->reverb[SRD_VOLUME] = v;

    // Decay time.
    for(i = 0, v = 0; i < NUM_MATERIAL_TYPES; ++i)
        v += materials[i] * matInfo[i].decayMul;
    if(v < 0)
        v = 0;
    else if(v > 255)
        v = 255;
    ssec->reverb[SRD_DECAY] = v;

    // High frequency damping.
    for(i = 0, v = 0; i < NUM_MATERIAL_TYPES; ++i)
        v += materials[i] * matInfo[i].dampingMul;
    if(v < 0)
        v = 0;
    else if(v > 255)
        v = 255;
    ssec->reverb[SRD_DAMPING] = v;

/*
#if _DEBUG
Con_Message("ssec %04i: vol:%3i sp:%3i dec:%3i dam:%3i\n",
            GET_SUBSECTOR_IDX(ssec), ssec->reverb[SRD_VOLUME],
            ssec->reverb[SRD_SPACE], ssec->reverb[SRD_DECAY],
            ssec->reverb[SRD_DAMPING]);
#endif
*/
    return true;
}

/**
 * Re-calculate the reverb properties of the given sector. Should be called
 * whenever any of the properties governing reverb properties have changed
 * (i.e. seg/plane texture or plane height changes).
 *
 * PRE: Subsector attributors must have been determined first.
 *
 * @param sec           Ptr to the sector to calculate reverb properties of.
 */
void S_CalcSectorReverb(sector_t *sec)
{
    uint        i;
    subsector_t *sub;
    float       spaceScatter;
    uint        sectorSpace;

    if(!sec)
        return; // Wha?

    sectorSpace = (int) (sec->SP_ceilheight - sec->SP_floorheight) *
        (sec->bounds[BRIGHT] - sec->bounds[BLEFT]) *
        (sec->bounds[BBOTTOM] - sec->bounds[BTOP]);    
/*
#if _DEBUG
Con_Message("sector %i: secsp:%i\n", c, sectorSpace);
#endif
*/
    for(i = 0; i < sec->numReverbSSecAttributors; ++i)
    {
        sub = sec->reverbSSecs[i];

        if(calcSSecReverb(sub))
        {
            sec->reverb[SRD_SPACE]   += sub->reverb[SRD_SPACE];

            sec->reverb[SRD_VOLUME]  +=
                sub->reverb[SRD_VOLUME]  / 255.0f * sub->reverb[SRD_SPACE];
            sec->reverb[SRD_DECAY]   +=
                sub->reverb[SRD_DECAY]   / 255.0f * sub->reverb[SRD_SPACE];
            sec->reverb[SRD_DAMPING] +=
                sub->reverb[SRD_DAMPING] / 255.0f * sub->reverb[SRD_SPACE];
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
    sec->reverb[SRD_SPACE] /=
        (spaceScatter > .8 ? 10 : spaceScatter > .6 ? 4 : 1);

    // Normalize the reverb space [0...1].
    // 0= very small
    // .99= very large
    // 1.0= only for open areas (special case).
    sec->reverb[SRD_SPACE] /= 120e6;
    if(sec->reverb[SRD_SPACE] > .99)
        sec->reverb[SRD_SPACE] = .99f;

    if(R_IsSkySurface(&sec->SP_ceilsurface) ||
       R_IsSkySurface(&sec->SP_floorsurface))
    {   // An "open" sector.
        // It can still be small, in which case; reverb is diminished a bit.
        if(sec->reverb[SRD_SPACE] > .5)
            sec->reverb[SRD_VOLUME] = 1; // Full volume.
        else
            sec->reverb[SRD_VOLUME] = .5f; // Small, but still open.

        sec->reverb[SRD_SPACE] = 1;
    }
    else
    {   // A "closed" sector.
        // Large spaces have automatically a bit more audible reverb.
        sec->reverb[SRD_VOLUME] += sec->reverb[SRD_SPACE] / 4;
    }

    if(sec->reverb[SRD_VOLUME] > 1)
        sec->reverb[SRD_VOLUME] = 1;
}
