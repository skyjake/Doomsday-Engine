/** @file s_environ.cpp Environmental audio effects.
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
} audioenvinfo_t;

static audioenvinfo_t envInfo[NUM_AUDIO_ENVIRONMENT_CLASSES] = {
    {"Metal",     255,     255,    25},
    {"Rock",      200,     160,    100},
    {"Wood",      80,      50,     200},
    {"Cloth",     5,       5,      255}
};

static ownernode_t* unusedNodeList;

typedef std::set<Sector *> ReverbUpdateRequested;
ReverbUpdateRequested reverbUpdateRequested;

const char* S_AudioEnvironmentName(AudioEnvironmentClass env)
{
    if(VALID_AUDIO_ENVIRONMENT_CLASS(env))
        return envInfo[env - AEC_FIRST].name;
    return "";
}

AudioEnvironmentClass S_AudioEnvironmentForMaterial(const Uri* uri)
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
                for(int l = 0; l < NUM_AUDIO_ENVIRONMENT_CLASSES; ++l)
                {
                    if(!stricmp(env->id, envInfo[l].name))
                        return AudioEnvironmentClass(AEC_FIRST + l);
                }
                return AEC_UNKNOWN;
            }
        }
    }
    return AEC_UNKNOWN;
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

static void findBspLeafsAffectingSector(GameMap *map, uint secIDX)
{
    Sector *sec = GameMap_Sector(map, secIDX);
    if(!sec || !sec->lineCount()) return;

    ownerlist_t bspLeafOwnerList;
    std::memset(&bspLeafOwnerList, 0, sizeof(bspLeafOwnerList));

    AABoxd aaBox = sec->aaBox();
    aaBox.minX -= 128;
    aaBox.minY -= 128;
    aaBox.maxX += 128;
    aaBox.maxY += 128;

    // LOG_DEBUG("sector %u: min[x:%f, y:%f]  max[x:%f, y:%f]")
    //    << secIDX <<  aaBox.minX << aaBox.minY << aaBox.maxX << aaBox.maxY;

    for(uint i = 0; i < map->numBspLeafs; ++i)
    {
        BspLeaf *bspLeaf = GameMap_BspLeaf(map, i);

        // Is this BSP leaf close enough?
        if(bspLeaf->sectorPtr() == sec || // leaf is IN this sector
           (bspLeaf->center()[VX] > aaBox.minX &&
            bspLeaf->center()[VY] > aaBox.minY &&
            bspLeaf->center()[VX] < aaBox.maxX &&
            bspLeaf->center()[VY] < aaBox.maxY))
        {
            // It will contribute to the reverb settings of this sector.
            setBspLeafSectorOwner(&bspLeafOwnerList, bspLeaf);
        }
    }

    sec->_reverbBspLeafs.clear();

    if(!bspLeafOwnerList.count) return;

    // Build the final list.
#ifdef DENG2_QT_4_7_OR_NEWER
    sec->_reverbBspLeafs.reserve(bspLeafOwnerList.count);
#endif

    ownernode_t *node = bspLeafOwnerList.head;
    for(uint i = 0; i < bspLeafOwnerList.count; ++i)
    {
        ownernode_t *next = node->next;

        sec->_reverbBspLeafs.append(static_cast<BspLeaf *>(node->data));

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

static boolean calcBspLeafReverb(BspLeaf *bspLeaf)
{
    DENG2_ASSERT(bspLeaf);

    if(!bspLeaf->hasSector() || isDedicated)
    {
        bspLeaf->_reverb[SRD_SPACE] = bspLeaf->_reverb[SRD_VOLUME] =
            bspLeaf->_reverb[SRD_DECAY] = bspLeaf->_reverb[SRD_DAMPING] = 0;
        return false;
    }

    float envSpaceAccum[NUM_AUDIO_ENVIRONMENT_CLASSES];
    std::memset(&envSpaceAccum, 0, sizeof(envSpaceAccum));

    // Space is the rough volume of the BSP leaf (bounding box).
    bspLeaf->_reverb[SRD_SPACE] =
        (int) (bspLeaf->sector().ceiling().height() - bspLeaf->sector().floor().height()) *
        (bspLeaf->aaBox().maxX - bspLeaf->aaBox().minX) *
        (bspLeaf->aaBox().maxY - bspLeaf->aaBox().minY);

    float total = 0;
    // The other reverb properties can be found out by taking a look at the
    // materials of all surfaces in the BSP leaf.
    HEdge *base = bspLeaf->firstHEdge();
    HEdge *hedge = base;
    do
    {
        if(hedge->hasLineSideDef() && hedge->lineSideDef().middle().hasMaterial())
        {
            Material &material = hedge->lineSideDef().middle().material();
            AudioEnvironmentClass env = material.audioEnvironment();
            if(!(env >= 0 && env < NUM_AUDIO_ENVIRONMENT_CLASSES))
                env = AEC_WOOD; // Assume it's wood if unknown.

            total += hedge->length();

            envSpaceAccum[env] += hedge->length();
        }
    } while((hedge = &hedge->next()) != base);

    if(!total)
    {
        // Huh?
        bspLeaf->_reverb[SRD_VOLUME] = bspLeaf->_reverb[SRD_DECAY] =
            bspLeaf->_reverb[SRD_DAMPING] = 0;
        return false;
    }

    // Average the results.
    uint i, v;
    for(i = 0; i < NUM_AUDIO_ENVIRONMENT_CLASSES; ++i)
    {
        envSpaceAccum[i] /= total;
    }

    // Volume.
    for(i = 0, v = 0; i < NUM_AUDIO_ENVIRONMENT_CLASSES; ++i)
    {
        v += envSpaceAccum[i] * envInfo[i].volumeMul;
    }
    if(v > 255) v = 255;
    bspLeaf->_reverb[SRD_VOLUME] = v;

    // Decay time.
    for(i = 0, v = 0; i < NUM_AUDIO_ENVIRONMENT_CLASSES; ++i)
    {
        v += envSpaceAccum[i] * envInfo[i].decayMul;
    }
    if(v > 255) v = 255;
    bspLeaf->_reverb[SRD_DECAY] = v;

    // High frequency damping.
    for(i = 0, v = 0; i < NUM_AUDIO_ENVIRONMENT_CLASSES; ++i)
    {
        v += envSpaceAccum[i] * envInfo[i].dampingMul;
    }
    if(v > 255) v = 255;
    bspLeaf->_reverb[SRD_DAMPING] = v;

    /* DEBUG_Message(("bspLeaf %04i: vol:%3i sp:%3i dec:%3i dam:%3i\n",
                      GET_BSPLEAF_IDX(bspLeaf), bspLeaf->reverb[SRD_VOLUME],
                      bspLeaf->reverb[SRD_SPACE], bspLeaf->reverb[SRD_DECAY],
                      bspLeaf->reverb[SRD_DAMPING])); */

    return true;
}

static void calculateSectorReverb(Sector *sec)
{
    if(!sec || !sec->lineCount()) return;

    /// @todo fixme: This 3D volume rough estimate may be massively off.
    ///       Consider the case of a single sector used over an entire map
    ///       with multiple disjoint groups of small geometries.
    ///       In general a sector should never be considered as playing any
    ///       part in the definition of a map's geometry. -ds
    uint spaceVolume = (int) (sec->ceiling().height() - sec->floor().height()) *
        (sec->aaBox().maxX - sec->aaBox().minX) *
        (sec->aaBox().maxY - sec->aaBox().minY);

    sec->_reverb[SRD_SPACE] = sec->_reverb[SRD_VOLUME] =
        sec->_reverb[SRD_DECAY] = sec->_reverb[SRD_DAMPING] = 0;

    foreach(BspLeaf *bspLeaf, sec->reverbBspLeafs())
    {
        if(calcBspLeafReverb(bspLeaf))
        {
            sec->_reverb[SRD_SPACE]   += bspLeaf->_reverb[SRD_SPACE];

            sec->_reverb[SRD_VOLUME]  += bspLeaf->_reverb[SRD_VOLUME]  / 255.0f * bspLeaf->_reverb[SRD_SPACE];
            sec->_reverb[SRD_DECAY]   += bspLeaf->_reverb[SRD_DECAY]   / 255.0f * bspLeaf->_reverb[SRD_SPACE];
            sec->_reverb[SRD_DAMPING] += bspLeaf->_reverb[SRD_DAMPING] / 255.0f * bspLeaf->_reverb[SRD_SPACE];
        }
    }

    float spaceScatter;
    if(sec->_reverb[SRD_SPACE])
    {
        spaceScatter = spaceVolume / sec->_reverb[SRD_SPACE];
        // These three are weighted by the space.
        sec->_reverb[SRD_VOLUME]  /= sec->_reverb[SRD_SPACE];
        sec->_reverb[SRD_DECAY]   /= sec->_reverb[SRD_SPACE];
        sec->_reverb[SRD_DAMPING] /= sec->_reverb[SRD_SPACE];
    }
    else
    {
        spaceScatter = 0;
        sec->_reverb[SRD_VOLUME]  = .2f;
        sec->_reverb[SRD_DECAY]   = .4f;
        sec->_reverb[SRD_DAMPING] = 1;
    }

    // If the space is scattered, the reverb effect lessens.
    sec->_reverb[SRD_SPACE] /= (spaceScatter > .8 ? 10 : spaceScatter > .6 ? 4 : 1);

    // Normalize the reverb space [0..1]
    //   0= very small
    // .99= very large
    // 1.0= only for open areas (special case).
    sec->_reverb[SRD_SPACE] /= 120e6;
    if(sec->_reverb[SRD_SPACE] > .99)
        sec->_reverb[SRD_SPACE] = .99f;

    if(sec->ceilingSurface().hasSkyMaskedMaterial() || sec->floorSurface().hasSkyMaskedMaterial())
    {
        // An "open" sector.
        // It can still be small, in which case; reverb is diminished a bit.
        if(sec->_reverb[SRD_SPACE] > .5)
            sec->_reverb[SRD_VOLUME] = 1; // Full volume.
        else
            sec->_reverb[SRD_VOLUME] = .5f; // Small, but still open.

        sec->_reverb[SRD_SPACE] = 1;
    }
    else
    {
        // A "closed" sector.
        // Large spaces have automatically a bit more audible reverb.
        sec->_reverb[SRD_VOLUME] += sec->_reverb[SRD_SPACE] / 4;
    }

    if(sec->_reverb[SRD_VOLUME] > 1)
        sec->_reverb[SRD_VOLUME] = 1;
}

void S_ResetReverb()
{
    reverbUpdateRequested.clear();
}

void S_UpdateReverbForSector(Sector *sec)
{
    if(reverbUpdateRequested.empty()) return;

    // If update has been requested for this sector, calculate it now.
    if(reverbUpdateRequested.find(sec) != reverbUpdateRequested.end())
    {
        calculateSectorReverb(sec);
        reverbUpdateRequested.erase(sec);
    }
}

void S_MarkSectorReverbDirty(Sector* sec)
{
    reverbUpdateRequested.insert(sec);
}
