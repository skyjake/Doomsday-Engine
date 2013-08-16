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

#include <set>

#include "de_base.h"
#include "de_audio.h"
#include "de_play.h"
#include "de_resource.h"

#include "BspLeaf"
#include "Sector"

#include "audio/s_environ.h"

using namespace de;

static AudioEnvironment envInfo[1 + NUM_AUDIO_ENVIRONMENTS] = {
    {"",          0,       0,      0},
    {"Metal",     255,     255,    25},
    {"Rock",      200,     160,    100},
    {"Wood",      80,      50,     200},
    {"Cloth",     5,       5,      255}
};

typedef std::set<Sector *> ReverbUpdateRequested;
ReverbUpdateRequested reverbUpdateRequested;

char const *S_AudioEnvironmentName(AudioEnvironmentId id)
{
    DENG_ASSERT(id >= AE_NONE && id < NUM_AUDIO_ENVIRONMENTS);
    return envInfo[1 + int(id)].name;
}

AudioEnvironment const &S_AudioEnvironment(AudioEnvironmentId id)
{
    DENG_ASSERT(id >= AE_NONE && id < NUM_AUDIO_ENVIRONMENTS);
    return envInfo[1 + int(id)];
}

AudioEnvironmentId S_AudioEnvironmentId(uri_s const *uri)
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

                // Is this a known environment?
                for(int m = 0; m < NUM_AUDIO_ENVIRONMENTS; ++m)
                {
                    AudioEnvironment const &envInfo = S_AudioEnvironment(AudioEnvironmentId(m));
                    if(!stricmp(env->id, envInfo.name))
                        return AudioEnvironmentId(m);
                }
                return AE_NONE;
            }
        }
    }
    return AE_NONE;
}

static void calculateSectorReverb(Sector *sec)
{
    if(!sec || !sec->sideCount()) return;

    uint spaceVolume = int((sec->ceiling().height() - sec->floor().height()) * sec->roughArea());

    sec->_reverb[SRD_SPACE] = sec->_reverb[SRD_VOLUME] =
        sec->_reverb[SRD_DECAY] = sec->_reverb[SRD_DAMPING] = 0;

    foreach(BspLeaf *bspLeaf, sec->reverbBspLeafs())
    {
        if(bspLeaf->updateReverb())
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
