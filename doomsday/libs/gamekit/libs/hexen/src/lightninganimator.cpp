/** @file lightninganimator.cpp  Animator for map-wide lightning effects.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#include "jhexen.h"
#include "lightninganimator.h"
#include "g_common.h"

#include <de/list.h>
#include "dmu_lib.h"
#include "gamesession.h"

using namespace de;

#define LIGHTNING_SPECIAL       198
#define LIGHTNING_SPECIAL2      199

static bool isLightningSector(Sector *sec)
{
    xsector_t *xsec = P_ToXSector(sec);

    if(xsec->special == LIGHTNING_SPECIAL || xsec->special == LIGHTNING_SPECIAL2)
        return true;

    if(P_GetIntp(P_GetPtrp(sec, DMU_CEILING_MATERIAL),
                 DMU_FLAGS) & MATF_SKYMASK)
        return true;

    if(P_GetIntp(P_GetPtrp(sec, DMU_FLOOR_MATERIAL),
                 DMU_FLAGS) & MATF_SKYMASK)
        return true;

    return false;
}

DE_PIMPL_NOREF(LightningAnimator)
{
    int flash = 0;
    int nextFlash = 0;
    typedef List<float> AmbientLightLevels;
    AmbientLightLevels sectorLightLevels;  ///< For each sector (if enabled).
};

LightningAnimator::LightningAnimator() : d(new Impl)
{}

bool LightningAnimator::enabled() const
{
    return !d->sectorLightLevels.isEmpty();
}

void LightningAnimator::triggerFlash()
{
    if(!enabled()) return;
    d->nextFlash = 0;
}

void LightningAnimator::advanceTime()
{
    if(!enabled()) return;

    // Is it time for a lightning state change?
    if(!(!d->nextFlash || d->flash))
    {
        d->nextFlash--;
        return;
    }

    if(d->flash)
    {
        d->flash--;

        if(d->flash)
        {
            int lightLevelsIdx = 0;
            for(int i = 0; i < numsectors; ++i)
            {
                Sector *sec = (Sector *)P_ToPtr(DMU_SECTOR, i);
                if(!isLightningSector(sec)) continue;

                const float ll = P_GetFloat(DMU_SECTOR, i, DMU_LIGHT_LEVEL);
                if(d->sectorLightLevels.at(lightLevelsIdx++) < ll - (4.f / 255))
                {
                    P_SetFloat(DMU_SECTOR, i, DMU_LIGHT_LEVEL, ll - (1.f / 255) * 4);
                }
            }
        }
        else
        {
            // Remove the alternate lightning flash special.
            int lightLevelsIdx = 0;
            for(int i = 0; i < numsectors; ++i)
            {
                Sector *sec = (Sector *)P_ToPtr(DMU_SECTOR, i);
                if(!isLightningSector(sec)) continue;

                P_SetFloatp(sec, DMU_LIGHT_LEVEL, d->sectorLightLevels.at(lightLevelsIdx++));
            }

            int skyFlags = P_GetInt(DMU_SKY, 0, DMU_FLAGS);
            skyFlags |= SKYF_LAYER0_ENABLED;
            skyFlags &= ~SKYF_LAYER1_ENABLED;
            P_SetInt(DMU_SKY, 0, DMU_FLAGS, skyFlags);
        }

        return;
    }

    d->flash = (P_Random() & 7) + 8;

    const float flashLight = (float) (200 + (P_Random() & 31)) / 255.0f;
    int lightLevelsIdx = 0;
    bool foundSec = false;
    for(int i = 0; i < numsectors; ++i)
    {
        Sector *sec = (Sector *)P_ToPtr(DMU_SECTOR, i);
        if(!isLightningSector(sec)) continue;

        xsector_t *xsec = P_ToXSector(sec);
        float newLevel  = P_GetFloatp(sec, DMU_LIGHT_LEVEL);

        d->sectorLightLevels[lightLevelsIdx] = newLevel;

        if(xsec->special == LIGHTNING_SPECIAL)
        {
            newLevel += .25f;
            if(newLevel > flashLight)
                newLevel = flashLight;
        }
        else if(xsec->special == LIGHTNING_SPECIAL2)
        {
            newLevel += .125f;
            if(newLevel > flashLight)
                newLevel = flashLight;
        }
        else
        {
            newLevel = flashLight;
        }

        if(newLevel < d->sectorLightLevels[lightLevelsIdx])
            newLevel = d->sectorLightLevels[lightLevelsIdx];

        P_SetFloatp(sec, DMU_LIGHT_LEVEL, newLevel);
        lightLevelsIdx++;
        foundSec = true;
    }

    if(foundSec)
    {
        mobj_t *plrmo = ::players[DISPLAYPLAYER].plr->mo;
        mobj_t *clapSource = 0;

        // Set the alternate (lightning) sky.
        int skyFlags = P_GetInt(DMU_SKY, 0, DMU_FLAGS);
        skyFlags &= ~SKYF_LAYER0_ENABLED;
        skyFlags |= SKYF_LAYER1_ENABLED;
        P_SetInt(DMU_SKY, 0, DMU_FLAGS, skyFlags);

        // If 3D sounds are active, position the clap somewhere above the player.
        if(Con_GetInteger("sound-3d") && plrmo && !IS_NETGAME)
        {
            coord_t clapOrigin[] = {
                plrmo->origin[VX] + (16 * (M_Random() - 127)),
                plrmo->origin[VY] + (16 * (M_Random() - 127)),
                plrmo->origin[VZ] + 4000
            };
            clapSource = P_SpawnMobj(MT_CAMERA, clapOrigin, 0, 0);
            if(clapSource)
            {
                clapSource->tics = 5 * TICSPERSEC; // Five seconds will do.
            }
        }

        // Make it loud!
        S_StartSound(SFX_THUNDER_CRASH | DDSF_NO_ATTENUATION, clapSource);
    }

    // Calculate the next lighting flash.
    if(!d->nextFlash)
    {
        if(P_Random() < 50) // Immediate, quick flash.
        {
            d->nextFlash = (P_Random() & 15) + 16;
        }
        else if(P_Random() < 128 && !(mapTime & 32))
        {
            d->nextFlash = ((P_Random() & 7) + 2) * TICSPERSEC;
        }
        else
        {
            d->nextFlash = ((P_Random() & 15) + 5) * TICSPERSEC;
        }
    }
}

bool LightningAnimator::initForMap()
{
    d->flash     = 0;
    d->nextFlash = 0;
    d->sectorLightLevels.clear();

    if (gfw_MapInfoFlags() & MIF_LIGHTNING)
    {
        int numLightningSectors = 0;
        for (int i = 0; i < numsectors; ++i)
        {
            if (isLightningSector(static_cast<Sector *>(P_ToPtr(DMU_SECTOR, i))))
            {
                numLightningSectors++;
            }
        }
        if (numLightningSectors > 0)
        {
            d->sectorLightLevels.resize(numLightningSectors);

            // Don't flash immediately on entering the map.
            d->nextFlash = ((P_Random() & 15) + 5) * TICSPERSEC;
        }
    }

    return enabled();
}
