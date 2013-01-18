/** @file s_environ.cpp Environmental sound effects. 
 * @ingroup audio
 *
 * Calculation of the aural properties of sectors.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#include <cctype>
#include <cstring>
#include <set>

#include "de_base.h"
#include "de_audio.h"
#include "de_play.h"
#include "de_resource.h"
#include "de_system.h"

#include <de/Log>

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

static ownernode_t* unusedNodeList;

typedef std::set<Sector *> ReverbUpdateRequested;
ReverbUpdateRequested reverbUpdateRequested;

const char* S_MaterialEnvClassName(material_env_class_t mclass)
{
    if(VALID_MATERIAL_ENV_CLASS(mclass))
        return matInfo[mclass - MEC_FIRST].name;
    return "";
}

material_env_class_t S_MaterialEnvClassForUri(const Uri* uri)
{
    if(uri)
    {
        ded_tenviron_t* env = defs.textureEnv;
        for(int i = 0; i < defs.count.textureEnv.num; ++i, env++)
        {
            for(int k = 0; k < env->count.num; ++k)
            {
                Uri* ref = env->materials[k];
                if(!ref || !Uri_Equality(ref, uri)) continue;

                // See if we recognise the material name.
                for(int l = 0; l < NUM_MATERIAL_ENV_CLASSES; ++l)
                {
                    if(!stricmp(env->id, matInfo[l].name))
                        return material_env_class_t(MEC_FIRST + l);
                }
                return MEC_UNKNOWN;
            }
        }
    }
    return MEC_UNKNOWN;
}

// Free any nodes left in the unused list.
static void clearUnusedNodes()
{
    while(unusedNodeList)
    {
        ownernode_t* next = unusedNodeList->next;
        M_Free(unusedNodeList);
        unusedNodeList = next;
    }
}

static ownernode_t* newOwnerNode(void)
{
    ownernode_t* node;

    if(unusedNodeList)
    {
        // An existing node is available for re-use.
        node = unusedNodeList;
        unusedNodeList = unusedNodeList->next;

        node->next = NULL;
        node->data = NULL;
    }
    else
    {
        // Need to allocate another.
        node = (ownernode_t*) M_Malloc(sizeof(ownernode_t));
    }

    return node;
}

static void setBspLeafSectorOwner(ownerlist_t* ownerList, BspLeaf* bspLeaf)
{
    DENG2_ASSERT(ownerList);
    if(!bspLeaf) return;

    // Add a new owner.
    // NOTE: No need to check for duplicates.
    ownerList->count++;

    ownernode_t* node = newOwnerNode();
    node->data = bspLeaf;
    node->next = ownerList->head;
    ownerList->head = node;
}

static void findBspLeafsAffectingSector(GameMap* map, uint secIDX)
{
    Sector* sec = GameMap_Sector(map, secIDX);
    if(!sec || !sec->lineDefCount) return;

    ownerlist_t bspLeafOwnerList;
    memset(&bspLeafOwnerList, 0, sizeof(bspLeafOwnerList));

    AABoxd aaBox = sec->aaBox;
    aaBox.minX -= 128;
    aaBox.minY -= 128;
    aaBox.maxX += 128;
    aaBox.maxY += 128;

    // LOG_DEBUG("sector %u: min[x:%f, y:%f]  max[x:%f, y:%f]")
    //    << secIDX <<  aaBox.minX << aaBox.minY << aaBox.maxX << aaBox.maxY;

    for(uint i = 0; i < map->numBspLeafs; ++i)
    {
        BspLeaf* bspLeaf = GameMap_BspLeaf(map, i);

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
        sec->reverbBspLeafs = (BspLeaf**)
            Z_Malloc((sec->numReverbBspLeafAttributors + 1) * sizeof(BspLeaf*), PU_MAPSTATIC, 0);

        BspLeaf** ptr = sec->reverbBspLeafs;
        ownernode_t* node = bspLeafOwnerList.head;
        for(uint i = 0; i < sec->numReverbBspLeafAttributors; ++i, ptr++)
        {
            ownernode_t* next = node->next;
            *ptr = (BspLeaf*) node->data;

            if(i < map->sectorCount() - 1)
            {
                // Move this node to the unused list for re-use.
                node->next = unusedNodeList;
                unusedNodeList = node;
            }
            else
            {
                // No further use for the node.
                M_Free(node);
            }
            node = next;
        }
        *ptr = 0; // terminate.
    }
}

void S_DetermineBspLeafsAffectingSectorReverb(GameMap* map)
{
    uint startTime = Timer_RealMilliseconds();

    /// @todo optimize: Make use of the BSP leaf blockmap.
    uint numSectors = GameMap_SectorCount(map);
    for(uint i = 0; i < numSectors; ++i)
    {
        findBspLeafsAffectingSector(map, i);
    }

    clearUnusedNodes();

    // How much time did we spend?
    LOG_VERBOSE("S_DetermineBspLeafsAffectingSectorReverb: Done in %.2f seconds.")
        << (Timer_RealMilliseconds() - startTime) / 1000.0f;
}

static boolean calcBspLeafReverb(BspLeaf* bspLeaf)
{
    DENG2_ASSERT(bspLeaf);

    if(!bspLeaf->sector || isDedicated)
    {
        bspLeaf->reverb[SRD_SPACE] = bspLeaf->reverb[SRD_VOLUME] =
            bspLeaf->reverb[SRD_DECAY] = bspLeaf->reverb[SRD_DAMPING] = 0;
        return false;
    }

    float materials[NUM_MATERIAL_ENV_CLASSES];
    memset(&materials, 0, sizeof(materials));

    // Space is the rough volume of the BSP leaf (bounding box).
    bspLeaf->reverb[SRD_SPACE] =
        (int) (bspLeaf->sector->SP_ceilheight - bspLeaf->sector->SP_floorheight) *
        (bspLeaf->aaBox.maxX - bspLeaf->aaBox.minX) *
        (bspLeaf->aaBox.maxY - bspLeaf->aaBox.minY);

    float total = 0;
    // The other reverb properties can be found out by taking a look at the
    // materials of all surfaces in the BSP leaf.
    HEdge* hedge = bspLeaf->hedge;
    do
    {
        if(hedge->lineDef && HEDGE_SIDEDEF(hedge) && HEDGE_SIDEDEF(hedge)->SW_middlematerial)
        {
            material_t* mat = HEDGE_SIDEDEF(hedge)->SW_middlematerial;
            material_env_class_t mclass = Material_EnvironmentClass(mat);
            if(!(mclass >= 0 && mclass < NUM_MATERIAL_ENV_CLASSES))
                mclass = MEC_WOOD; // Assume it's wood if unknown.

            total += hedge->length;

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
    uint i, v;
    for(i = 0; i < NUM_MATERIAL_ENV_CLASSES; ++i)
    {
        materials[i] /= total;
    }

    // Volume.
    for(i = 0, v = 0; i < NUM_MATERIAL_ENV_CLASSES; ++i)
    {
        v += materials[i] * matInfo[i].volumeMul;
    }
    if(v > 255) v = 255;
    bspLeaf->reverb[SRD_VOLUME] = v;

    // Decay time.
    for(i = 0, v = 0; i < NUM_MATERIAL_ENV_CLASSES; ++i)
    {
        v += materials[i] * matInfo[i].decayMul;
    }
    if(v > 255) v = 255;
    bspLeaf->reverb[SRD_DECAY] = v;

    // High frequency damping.
    for(i = 0, v = 0; i < NUM_MATERIAL_ENV_CLASSES; ++i)
    {
        v += materials[i] * matInfo[i].dampingMul;
    }
    if(v > 255) v = 255;
    bspLeaf->reverb[SRD_DAMPING] = v;

    /* DEBUG_Message(("bspLeaf %04i: vol:%3i sp:%3i dec:%3i dam:%3i\n",
                      GET_BSPLEAF_IDX(bspLeaf), bspLeaf->reverb[SRD_VOLUME],
                      bspLeaf->reverb[SRD_SPACE], bspLeaf->reverb[SRD_DECAY],
                      bspLeaf->reverb[SRD_DAMPING])); */

    return true;
}

static void Sector_CalculateReverb(Sector* sec)
{
    if(!sec || !sec->lineDefCount) return;

    /// @todo fixme: This 3D volume rough estimate may be massively off.
    ///       Consider the case of a single sector used over an entire map
    ///       with multiple disjoint groups of small geometries.
    ///       In general a sector should never be considered as playing any
    ///       part in the definition of a map's geometry. -ds
    uint spaceVolume = (int) (sec->SP_ceilheight - sec->SP_floorheight) *
        (sec->aaBox.maxX - sec->aaBox.minX) *
        (sec->aaBox.maxY - sec->aaBox.minY);

    sec->reverb[SRD_SPACE] = sec->reverb[SRD_VOLUME] =
        sec->reverb[SRD_DECAY] = sec->reverb[SRD_DAMPING] = 0;

    for(uint i = 0; i < sec->numReverbBspLeafAttributors; ++i)
    {
        BspLeaf* bspLeaf = sec->reverbBspLeafs[i];

        if(calcBspLeafReverb(bspLeaf))
        {
            sec->reverb[SRD_SPACE]   += bspLeaf->reverb[SRD_SPACE];

            sec->reverb[SRD_VOLUME]  += bspLeaf->reverb[SRD_VOLUME]  / 255.0f * bspLeaf->reverb[SRD_SPACE];
            sec->reverb[SRD_DECAY]   += bspLeaf->reverb[SRD_DECAY]   / 255.0f * bspLeaf->reverb[SRD_SPACE];
            sec->reverb[SRD_DAMPING] += bspLeaf->reverb[SRD_DAMPING] / 255.0f * bspLeaf->reverb[SRD_SPACE];
        }
    }

    float spaceScatter;
    if(sec->reverb[SRD_SPACE])
    {
        spaceScatter = spaceVolume / sec->reverb[SRD_SPACE];
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
    sec->reverb[SRD_SPACE] /= (spaceScatter > .8 ? 10 : spaceScatter > .6 ? 4 : 1);

    // Normalize the reverb space [0..1]
    //   0= very small
    // .99= very large
    // 1.0= only for open areas (special case).
    sec->reverb[SRD_SPACE] /= 120e6;
    if(sec->reverb[SRD_SPACE] > .99)
        sec->reverb[SRD_SPACE] = .99f;

    if(Surface_IsSkyMasked(&sec->SP_ceilsurface) || Surface_IsSkyMasked(&sec->SP_floorsurface))
    {
        // An "open" sector.
        // It can still be small, in which case; reverb is diminished a bit.
        if(sec->reverb[SRD_SPACE] > .5)
            sec->reverb[SRD_VOLUME] = 1; // Full volume.
        else
            sec->reverb[SRD_VOLUME] = .5f; // Small, but still open.

        sec->reverb[SRD_SPACE] = 1;
    }
    else
    {
        // A "closed" sector.
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

void S_UpdateReverbForSector(Sector* sec)
{
    if(reverbUpdateRequested.empty()) return;

    // If update has been requested for this sector, calculate it now.
    if(reverbUpdateRequested.find(sec) != reverbUpdateRequested.end())
    {
        Sector_CalculateReverb(sec);
        reverbUpdateRequested.erase(sec);
    }
}

void S_MarkSectorReverbDirty(Sector* sec)
{
    reverbUpdateRequested.insert(sec);
}
