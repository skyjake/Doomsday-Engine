/**
 * @file s_environ.cpp
 * Environmental sound effects. @ingroup audio
 *
 * Calculation of the aural properties of sectors.
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <ctype.h>
#include <string.h>
#include <set>

#include "de_base.h"
#include "de_console.h"
#include "de_play.h"
#include "de_refresh.h"
#include "de_audio.h"
#include "de_misc.h"

#include "materialvariant.h"

typedef struct {
    const char  name[9];    // Material type name.
    int         volumeMul;
    int         decayMul;
    int         dampingMul;
} materialenvinfo_t;

static materialenvinfo_t matInfo[NUM_MATERIAL_ENV_CLASSES] = {
    {"Metal",     255,     255,    25},
    {"Rock",      200,     160,    100},
    {"Wood",      80,      50,     200},
    {"Cloth",     5,       5,      255}
};

static ownernode_t *unusedNodeList = NULL;

typedef std::set<Sector*> ReverbUpdateRequested;
ReverbUpdateRequested reverbUpdateRequested;

const char* S_MaterialEnvClassName(material_env_class_t mclass)
{
    if(VALID_MATERIAL_ENV_CLASS(mclass))
        return matInfo[mclass - MEC_FIRST].name;
    return "";
}

material_env_class_t S_MaterialEnvClassForUri(const Uri* uri)
{
    ded_tenviron_t* env;
    int i;
    for(i = 0, env = defs.textureEnv; i < defs.count.textureEnv.num; ++i, env++)
    {
        int j;
        for(j = 0; j < env->count.num; ++j)
        {
            Uri* ref = env->materials[j];

            if(!ref) continue;

            if(Uri_Equality(ref, uri))
            {   // A match!
                // See if we recognise the material name.
                int k;
                for(k = 0; k < NUM_MATERIAL_ENV_CLASSES; ++k)
                {
                    if(!stricmp(env->id, matInfo[k].name))
                        return material_env_class_t(MEC_FIRST + k);
                }
                return MEC_UNKNOWN;
            }
        }
    }
    return MEC_UNKNOWN;
}

static ownernode_t* newOwnerNode(void)
{
    ownernode_t* node;

    if(unusedNodeList)
    {   // An existing node is available for re-use.
        node = unusedNodeList;
        unusedNodeList = unusedNodeList->next;

        node->next = NULL;
        node->data = NULL;
    }
    else
    {   // Need to allocate another.
        node = (ownernode_t*) M_Malloc(sizeof(ownernode_t));
    }

    return node;
}

static void setBspLeafSectorOwner(ownerlist_t* ownerList, BspLeaf* bspLeaf)
{
    ownernode_t* node;

    if(!bspLeaf) return;

    // Add a new owner.
    // NOTE: No need to check for duplicates.
    ownerList->count++;

    node = newOwnerNode();
    node->data = bspLeaf;
    node->next = ownerList->head;
    ownerList->head = node;
}

static void findBspLeafsAffectingSector(GameMap* map, uint secIDX)
{
    assert(map && secIDX < map->numSectors);
    {
    Sector* sec = &map->sectors[secIDX];
    ownerlist_t bspLeafOwnerList;
    ownernode_t* node, *p;
    BspLeaf* bspLeaf;
    AABoxf aaBox;
    uint i;

    if(0 == sec->lineDefCount)
        return;

    memset(&bspLeafOwnerList, 0, sizeof(bspLeafOwnerList));

    memcpy(&aaBox, &sec->aaBox, sizeof(aaBox));
    aaBox.minX -= 128;
    aaBox.minY -= 128;
    aaBox.maxX += 128;
    aaBox.maxY += 128;

    /*DEBUG_Message(("sector %i: (%f,%f) - (%f,%f)\n", c,
                     bbox[BOXLEFT], bbox[BOXTOP], bbox[BOXRIGHT], bbox[BOXBOTTOM]));*/

    for(i = 0; i < map->numBspLeafs; ++i)
    {
        bspLeaf = GameMap_BspLeaf(map, i);

        // Is this BSP leaf close enough?
        if(bspLeaf->sector == sec || // leaf is IN this sector
           (bspLeaf->midPoint[VX] > aaBox.minX &&
            bspLeaf->midPoint[VY] > aaBox.minY &&
            bspLeaf->midPoint[VX] < aaBox.maxX &&
            bspLeaf->midPoint[VY] < aaBox.maxY))
        {
            // It will contribute to the reverb settings of this sector.
            setBspLeafSectorOwner(&bspLeafOwnerList, bspLeaf);
        }
    }

    // Now harden the list.
    sec->numReverbBspLeafAttributors = bspLeafOwnerList.count;
    if(sec->numReverbBspLeafAttributors)
    {
        BspLeaf **ptr;

        sec->reverbBspLeafs = (BspLeaf**)
            Z_Malloc((sec->numReverbBspLeafAttributors + 1) * sizeof(BspLeaf*), PU_MAPSTATIC, 0);

        for(i = 0, ptr = sec->reverbBspLeafs, node = bspLeafOwnerList.head;
            i < sec->numReverbBspLeafAttributors; ++i, ptr++)
        {
            p = node->next;
            *ptr = (BspLeaf*) node->data;

            if(i < map->numSectors - 1)
            {
                // Move this node to the unused list for re-use.
                node->next = unusedNodeList;
                unusedNodeList = node;
            }
            else
            {   // No further use for the nodes.
                M_Free(node);
            }
            node = p;
        }
        *ptr = NULL; // terminate.
    }
    }
}

void S_DetermineBspLeafsAffectingSectorReverb(GameMap* map)
{
    uint i, startTime;
    ownernode_t* node, *p;
    assert(map);

    startTime = Sys_GetRealTime();

    /// @optimize Make use of the BSP leaf blockmap.
    for(i = 0; i < map->numSectors; ++i)
    {
        findBspLeafsAffectingSector(map, i);
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
            ("S_DetermineBspLeafsAffectingSectorReverb: Done in %.2f seconds.\n",
             (Sys_GetRealTime() - startTime) / 1000.0f));
}

static boolean calcBspLeafReverb(BspLeaf* bspLeaf)
{
    float materials[NUM_MATERIAL_ENV_CLASSES];
    material_env_class_t mclass;
    HEdge* hedge;
    float total = 0;
    uint i, v;

    if(!bspLeaf->sector || isDedicated)
    {
        bspLeaf->reverb[SRD_SPACE] = bspLeaf->reverb[SRD_VOLUME] =
            bspLeaf->reverb[SRD_DECAY] = bspLeaf->reverb[SRD_DAMPING] = 0;
        return false;
    }

    memset(&materials, 0, sizeof(materials));

    // Space is the rough volume of the BSP leaf (bounding box).
    bspLeaf->reverb[SRD_SPACE] =
        (int) (bspLeaf->sector->SP_ceilheight - bspLeaf->sector->SP_floorheight) *
        (bspLeaf->aaBox.maxX - bspLeaf->aaBox.minX) *
        (bspLeaf->aaBox.maxY - bspLeaf->aaBox.minY);

    // The other reverb properties can be found out by taking a look at the
    // materials of all surfaces in the BSP leaf.
    hedge = bspLeaf->hedge;
    do
    {
        if(hedge->lineDef && HEDGE_SIDEDEF(hedge) && HEDGE_SIDEDEF(hedge)->SW_middlematerial)
        {
            material_t* mat = HEDGE_SIDEDEF(hedge)->SW_middlematerial;

            mclass = Material_EnvironmentClass(mat);
            total += hedge->length;
            if(!(mclass >= 0 && mclass < NUM_MATERIAL_ENV_CLASSES))
                mclass = MEC_WOOD; // Assume it's wood if unknown.
            materials[mclass] += hedge->length;
        }
    } while((hedge = hedge->next) != bspLeaf->hedge);

    if(!total)
    {
        // Huh?
        bspLeaf->reverb[SRD_VOLUME] = bspLeaf->reverb[SRD_DECAY] =
            bspLeaf->reverb[SRD_DAMPING] = 0;
        return false;
    }

    // Average the results.
    for(i = 0; i < NUM_MATERIAL_ENV_CLASSES; ++i)
        materials[i] /= total;

    // Volume.
    for(i = 0, v = 0; i < NUM_MATERIAL_ENV_CLASSES; ++i)
        v += materials[i] * matInfo[i].volumeMul;
    if(v > 255)
        v = 255;
    bspLeaf->reverb[SRD_VOLUME] = v;

    // Decay time.
    for(i = 0, v = 0; i < NUM_MATERIAL_ENV_CLASSES; ++i)
        v += materials[i] * matInfo[i].decayMul;
    if(v > 255)
        v = 255;
    bspLeaf->reverb[SRD_DECAY] = v;

    // High frequency damping.
    for(i = 0, v = 0; i < NUM_MATERIAL_ENV_CLASSES; ++i)
        v += materials[i] * matInfo[i].dampingMul;
    if(v > 255)
        v = 255;
    bspLeaf->reverb[SRD_DAMPING] = v;

    /* DEBUG_Message(("bspLeaf %04i: vol:%3i sp:%3i dec:%3i dam:%3i\n",
                      GET_BSPLEAF_IDX(bspLeaf), bspLeaf->reverb[SRD_VOLUME],
                      bspLeaf->reverb[SRD_SPACE], bspLeaf->reverb[SRD_DECAY],
                      bspLeaf->reverb[SRD_DAMPING])); */

    return true;
}

static void Sector_CalculateReverb(Sector* sec)
{
    BspLeaf* sub;
    float spaceScatter;
    uint sectorSpace;

    if(!sec || !sec->lineDefCount) return;

    sectorSpace = (int) (sec->SP_ceilheight - sec->SP_floorheight) *
        (sec->aaBox.maxX - sec->aaBox.minX) *
        (sec->aaBox.maxY - sec->aaBox.minY);

    // DEBUG_Message(("sector %i: secsp:%i\n", c, sectorSpace));

    sec->reverb[SRD_SPACE] = sec->reverb[SRD_VOLUME] =
        sec->reverb[SRD_DECAY] = sec->reverb[SRD_DAMPING] = 0;

    { uint i;
    for(i = 0; i < sec->numReverbBspLeafAttributors; ++i)
    {
        sub = sec->reverbBspLeafs[i];

        if(calcBspLeafReverb(sub))
        {
            sec->reverb[SRD_SPACE]   += sub->reverb[SRD_SPACE];

            sec->reverb[SRD_VOLUME]  +=
                sub->reverb[SRD_VOLUME]  / 255.0f * sub->reverb[SRD_SPACE];
            sec->reverb[SRD_DECAY]   +=
                sub->reverb[SRD_DECAY]   / 255.0f * sub->reverb[SRD_SPACE];
            sec->reverb[SRD_DAMPING] +=
                sub->reverb[SRD_DAMPING] / 255.0f * sub->reverb[SRD_SPACE];
        }
    }}

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

    if(Surface_IsSkyMasked(&sec->SP_ceilsurface) ||
       Surface_IsSkyMasked(&sec->SP_floorsurface))
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

void S_ResetReverb(void)
{
    reverbUpdateRequested.clear();
}

void S_UpdateReverb(void)
{
    if(sfx3D)
    {
        if(reverbUpdateRequested.empty()) return;

        for(ReverbUpdateRequested::iterator i = reverbUpdateRequested.begin(); i != reverbUpdateRequested.end(); ++i)
        {
            Sector_CalculateReverb(*i);
        }
    }
    reverbUpdateRequested.clear();
}

void S_CalcSectorReverb(Sector* sec)
{
    if(!sfx3D) return;

    reverbUpdateRequested.insert(sec);
}
