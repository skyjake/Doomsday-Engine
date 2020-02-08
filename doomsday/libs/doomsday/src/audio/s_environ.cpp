/** @file s_environ.cpp  Environmental audio effects.
 *
 * Calculation of the aural properties of sectors.
 *
 * @authors Copyright © 2003-2020 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "doomsday/audio/s_environ.h"
#include "doomsday/defs/ded.h"

using namespace de;

static AudioEnvironment envInfo[1 + NUM_AUDIO_ENVIRONMENTS] = {
    { "",          0,       0,      0   },
    { "Metal",     255,     255,    25  },
    { "Rock",      200,     160,    100 },
    { "Wood",      80,      50,     200 },
    { "Cloth",     5,       5,      255 }
};

const char *S_AudioEnvironmentName(AudioEnvironmentId id)
{
    DE_ASSERT(id >= AE_NONE && id < NUM_AUDIO_ENVIRONMENTS);
    return ::envInfo[1 + dint( id )].name;
}

const AudioEnvironment &S_AudioEnvironment(AudioEnvironmentId id)
{
    DE_ASSERT(id >= AE_NONE && id < NUM_AUDIO_ENVIRONMENTS);
    return ::envInfo[1 + dint( id )];
}

AudioEnvironmentId S_AudioEnvironmentId(const res::Uri *uri)
{
    if (uri)
    {
        for (dint i = 0; i < DED_Definitions()->textureEnv.size(); ++i)
        {
            const ded_tenviron_t *env = &DED_Definitions()->textureEnv[i];
            for (dint k = 0; k < env->materials.size(); ++k)
            {
                res::Uri *ref = env->materials[k].uri;
                if (!ref || *ref != *uri) continue;

                // Is this a known environment?
                for (dint m = 0; m < NUM_AUDIO_ENVIRONMENTS; ++m)
                {
                    const auto &envInfo = S_AudioEnvironment(AudioEnvironmentId(m));
                    if (!iCmpStrCase(env->id, envInfo.name)) return AudioEnvironmentId(m);
                }
                return AE_NONE;
            }
        }
    }
    return AE_NONE;
}
