/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
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

/**
 * p_lights.c: Handle Sector base lighting effects.
 */

// HEADER FILES ------------------------------------------------------------

#include "wolftc.h"

#include "dmu_lib.h"
#include "p_mapsetup.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void T_FireFlicker(fireflicker_t *flick)
{
    float       lightlevel = P_GetFloatp(flick->sector, DMU_LIGHT_LEVEL);
    float       amount;

    if(--flick->count)
        return;

    amount = ((P_Random() & 3) * 16) / 255.0f;

    if(lightlevel - amount < flick->minLight)
        P_SetFloatp(flick->sector, DMU_LIGHT_LEVEL, flick->minLight);
    else
        P_SetFloatp(flick->sector, DMU_LIGHT_LEVEL,
                    flick->maxLight - amount);

    flick->count = 4;
}

void P_SpawnFireFlicker(sector_t *sector)
{
    float       lightlevel = P_GetFloatp(sector, DMU_LIGHT_LEVEL);
    fireflicker_t *flick;

    // Note that we are resetting sector attributes.
    // Nothing special about it during gameplay.
    P_ToXSector(sector)->special = 0;

    flick = Z_Malloc(sizeof(*flick), PU_LEVSPEC, 0);

    P_ThinkerAdd(&flick->thinker);

    flick->thinker.function = T_FireFlicker;
    flick->sector = sector;
    flick->maxLight = lightlevel;
    flick->minLight =
        P_FindMinSurroundingLight(sector, lightlevel) + (16.0f/255.0f);
    flick->count = 4;
}

/**
 * Broken light flashing.
 */
void T_LightFlash(lightflash_t *flash)
{
    float       lightlevel = P_GetFloatp(flash->sector, DMU_LIGHT_LEVEL);

    if(--flash->count)
        return;

    if(lightlevel == flash->maxLight)
    {
        P_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->minLight);
        flash->count = (P_Random() & flash->minTime) + 1;
    }
    else
    {
        P_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->maxLight);
        flash->count = (P_Random() & flash->maxTime) + 1;
    }
}

/**
 * After the map has been loaded, scan each sector
 * for specials that spawn thinkers
 */
void P_SpawnLightFlash(sector_t *sector)
{
    float       lightlevel = P_GetFloatp(sector, DMU_LIGHT_LEVEL);
    lightflash_t *flash;

    // nothing special about it during gameplay
    P_ToXSector(sector)->special = 0;

    flash = Z_Malloc(sizeof(*flash), PU_LEVSPEC, 0);

    P_ThinkerAdd(&flash->thinker);

    flash->thinker.function = T_LightFlash;
    flash->sector = sector;
    flash->maxLight = lightlevel;

    flash->minLight = P_FindMinSurroundingLight(sector, lightlevel);
    flash->maxTime = 64;
    flash->minTime = 7;
    flash->count = (P_Random() & flash->maxTime) + 1;
}

/**
 * Strobe light flashing.
 */
void T_StrobeFlash(strobe_t *flash)
{
    float       lightlevel = P_GetFloatp(flash->sector, DMU_LIGHT_LEVEL);

    if(--flash->count)
        return;

    if(lightlevel == flash->minLight)
    {
        P_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->maxLight);
        flash->count = flash->brightTime;
    }
    else
    {
        P_SetFloatp(flash->sector, DMU_LIGHT_LEVEL, flash->minLight);
        flash->count = flash->darkTime;
    }
}

/**
 * After the map has been loaded, scan each sector for specials that spawn
 * thinkers.
 */
void P_SpawnStrobeFlash(sector_t *sector, int fastOrSlow, int inSync)
{
    strobe_t   *flash;
    float       lightlevel = P_GetFloatp(sector, DMU_LIGHT_LEVEL);

    flash = Z_Malloc(sizeof(*flash), PU_LEVSPEC, 0);

    P_ThinkerAdd(&flash->thinker);

    flash->sector = sector;
    flash->darkTime = fastOrSlow;
    flash->brightTime = STROBEBRIGHT;
    flash->thinker.function = T_StrobeFlash;
    flash->maxLight = lightlevel;
    flash->minLight = P_FindMinSurroundingLight(sector, lightlevel);

    if(flash->minLight == flash->maxLight)
        flash->minLight = 0;

    // nothing special about it during gameplay
    P_ToXSector(sector)->special = 0;

    if(!inSync)
        flash->count = (P_Random() & 7) + 1;
    else
        flash->count = 1;
}

/**
 * Start strobing lights (usually from a trigger).
 */
void EV_StartLightStrobing(linedef_t *line)
{
    sector_t   *sec = NULL;
    iterlist_t *list;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list)
        return;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        if(P_ToXSector(sec)->specialData)
            continue;

        P_SpawnStrobeFlash(sec, SLOWDARK, 0);
    }
}

void EV_TurnTagLightsOff(linedef_t *line)
{
    int         j;
    float       min;
    float       lightlevel;
    sector_t   *sec = NULL, *tsec;
    linedef_t     *other;
    iterlist_t *list;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list)
        return;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        min = P_GetFloatp(sec, DMU_LIGHT_LEVEL);
        for(j = 0; j < P_GetIntp(sec, DMU_LINEDEF_COUNT); ++j)
        {
            other = P_GetPtrp(sec, DMU_LINEDEF_OF_SECTOR | j);
            tsec = P_GetNextSector(other, sec);

            if(!tsec)
                continue;

            lightlevel = P_GetFloatp(tsec, DMU_LIGHT_LEVEL);

            if(lightlevel < min)
                min = lightlevel;
        }

        P_SetFloatp(sec, DMU_LIGHT_LEVEL, min);
    }
}

void EV_LightTurnOn(linedef_t *line, float max)
{
    int         j;
    float       lightlevel;
    sector_t   *sec = NULL, *tsec;
    linedef_t     *tline;
    iterlist_t *list;

    list = P_GetSectorIterListForTag(P_ToXLine(line)->tag, false);
    if(!list)
        return;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        // max = 0 means to search
        // for highest light level
        // surrounding sector
        if(!max)
        {
            for(j = 0; j < P_GetIntp(sec, DMU_LINEDEF_COUNT); ++j)
            {
                tline = P_GetPtrp(sec, DMU_LINEDEF_OF_SECTOR | j);
                tsec = P_GetNextSector(tline, sec);

                if(!tsec)
                    continue;

                lightlevel = P_GetFloatp(tsec, DMU_LIGHT_LEVEL);

                if(lightlevel > max)
                    max = lightlevel;
            }
        }
        P_SetFloatp(sec, DMU_LIGHT_LEVEL, max);
    }
}

void T_Glow(glow_t *g)
{
    float       lightlevel = P_GetFloatp(g->sector, DMU_LIGHT_LEVEL);
    float       glowDelta = (1.0f / 255.0f) * (float) GLOWSPEED;

    switch(g->direction)
    {
    case -1:
        // DOWN
        lightlevel -= glowDelta;
        if(lightlevel <= g->minLight)
        {
            lightlevel += glowDelta;
            g->direction = 1;
        }
        break;

    case 1:
        // UP
        lightlevel += glowDelta;
        if(lightlevel >= g->maxLight)
        {
            lightlevel -= glowDelta;
            g->direction = -1;
        }
        break;
    }

    P_SetFloatp(g->sector, DMU_LIGHT_LEVEL, lightlevel);
}

void P_SpawnGlowingLight(sector_t *sector)
{
    float       lightlevel = P_GetFloatp(sector, DMU_LIGHT_LEVEL);
    glow_t     *g;

    g = Z_Malloc(sizeof(*g), PU_LEVSPEC, 0);

    P_ThinkerAdd(&g->thinker);

    g->sector = sector;
    g->minLight = P_FindMinSurroundingLight(sector, lightlevel);
    g->maxLight = lightlevel;
    g->thinker.function = T_Glow;
    g->direction = -1;

    P_ToXSector(sector)->special = 0;
}
