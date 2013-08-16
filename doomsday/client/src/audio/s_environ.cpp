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

#include <de/Log>

#include "de_base.h"
#include "de_audio.h"
#include "de_play.h"
#include "de_resource.h"
#include "de_system.h"

#include "Face"

#include "BspLeaf"

#include "audio/s_environ.h"

using namespace de;

typedef struct {
    char const name[9]; ///< Environment type name.
    int volumeMul;
    int decayMul;
    int dampingMul;
} audioenvinfo_t;

static audioenvinfo_t envInfo[NUM_AUDIO_ENVIRONMENT_CLASSES] = {
    {"Metal",     255,     255,    25},
    {"Rock",      200,     160,    100},
    {"Wood",      80,      50,     200},
    {"Cloth",     5,       5,      255}
};

typedef std::set<Sector *> ReverbUpdateRequested;
ReverbUpdateRequested reverbUpdateRequested;

char const *S_AudioEnvironmentName(AudioEnvironmentClass env)
{
    if(VALID_AUDIO_ENVIRONMENT_CLASS(env))
        return envInfo[env - AEC_FIRST].name;
    return "";
}

AudioEnvironmentClass S_AudioEnvironmentForMaterial(uri_s const *uri)
{
    if(uri)
    {
        ded_tenviron_t *env = defs.textureEnv;
        for(int i = 0; i < defs.count.textureEnv.num; ++i, env++)
        {
            for(int k = 0; k < env->count.num; ++k)
            {
                uri_s *ref = env->materials[k];
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

static void accumReverbForWallSections(HEdge const *hedge,
    float envSpaceAccum[NUM_AUDIO_ENVIRONMENT_CLASSES], float &total)
{
    // Edges with no map line segment implicitly have no surfaces.
    if(!hedge || !hedge->mapElement())
        return;

    Line::Side::Segment *seg = hedge->mapElement()->as<Line::Side::Segment>();
    if(!seg->lineSide().hasSections() || !seg->lineSide().middle().hasMaterial())
        return;

    Material &material = seg->lineSide().middle().material();
    AudioEnvironmentClass env = material.audioEnvironment();
    if(!(env >= 0 && env < NUM_AUDIO_ENVIRONMENT_CLASSES))
        env = AEC_WOOD; // Assume it's wood if unknown.

    total += seg->length();

    envSpaceAccum[env] += seg->length();
}

static boolean calcBspLeafReverb(BspLeaf *bspLeaf)
{
    DENG2_ASSERT(bspLeaf);

    if(!bspLeaf->hasSector() || bspLeaf->isDegenerate() || isDedicated)
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
        (bspLeaf->poly().aaBox().maxX - bspLeaf->poly().aaBox().minX) *
        (bspLeaf->poly().aaBox().maxY - bspLeaf->poly().aaBox().minY);

    float total = 0;

    // The other reverb properties can be found out by taking a look at the
    // materials of all surfaces in the BSP leaf.
    HEdge *base = bspLeaf->poly().hedge();
    HEdge *hedge = base;
    do
    {
        accumReverbForWallSections(hedge, envSpaceAccum, total);
    } while((hedge = &hedge->next()) != base);

    foreach(Mesh *mesh, bspLeaf->extraMeshes())
    foreach(HEdge *hedge, mesh->hedges())
    {
        accumReverbForWallSections(hedge, envSpaceAccum, total);
    }

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
                      bspLeaf->indexInMap(), bspLeaf->reverb[SRD_VOLUME],
                      bspLeaf->reverb[SRD_SPACE], bspLeaf->reverb[SRD_DECAY],
                      bspLeaf->reverb[SRD_DAMPING])); */

    return true;
}

static void calculateSectorReverb(Sector *sec)
{
    if(!sec || !sec->sideCount()) return;

    uint spaceVolume = int((sec->ceiling().height() - sec->floor().height()) * sec->roughArea());

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
