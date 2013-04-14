/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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
 * p_lights.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "dmu_lib.h"
#include "p_mapspec.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static float phaseTable[64] = {
    .5, .4375, .375, .3125, .25, .1875, .125, .125,
    .0625, .0625, .0625, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, .0625, .0625, .0625,
    .125, .125, .1875, .25, .3125, .375, .4375, .5
};

// CODE --------------------------------------------------------------------

void T_Light(light_t *light)
{
    if(light->count)
    {
        light->count--;
        return;
    }

    switch(light->type)
    {
    case LITE_FADE:
        P_SectorModifyLight(light->sector, light->value2);

        if(light->tics2 == 1)
        {
            if(P_SectorLight(light->sector) >= light->value1)
            {
                P_SectorSetLight(light->sector, light->value1);
                Thinker_Remove(&light->thinker);
            }
        }
        else if(P_SectorLight(light->sector) <= light->value1)
        {
            P_SectorSetLight(light->sector, light->value1);
            Thinker_Remove(&light->thinker);
        }
        break;

    case LITE_GLOW:
        P_SectorModifyLightx(light->sector, light->tics1);
        if(light->tics2 == 1)
        {
            if(P_SectorLight(light->sector) >= light->value1)
            {
                P_SectorSetLight(light->sector, light->value1);
                light->tics1 = -light->tics1;
                light->tics2 = -1;  // Reverse direction.
            }
        }
        else if(P_SectorLight(light->sector) <= light->value2)
        {
            P_SectorSetLight(light->sector, light->value2);
            light->tics1 = -light->tics1;
            light->tics2 = 1; // Reverse direction.
        }
        break;

    case LITE_FLICKER:
        if(P_SectorLight(light->sector) == light->value1)
        {
            P_SectorSetLight(light->sector, light->value2);
            light->count = (P_Random() & 7) + 1;
        }
        else
        {
            P_SectorSetLight(light->sector, light->value1);
            light->count = (P_Random() & 31) + 1;
        }
        break;

    case LITE_STROBE:
        if(P_SectorLight(light->sector) == light->value1)
        {
            P_SectorSetLight(light->sector, light->value2);
            light->count = light->tics2;
        }
        else
        {
            P_SectorSetLight(light->sector, light->value1);
            light->count = light->tics1;
        }
        break;

    default:
        break;
    }
}

boolean EV_SpawnLight(Line *line, byte *arg, lighttype_t type)
{
    int         arg1, arg2, arg3, arg4;
    boolean     think = false;
    boolean     rtn = false;
    light_t    *light;
    Sector     *sec = NULL;
    iterlist_t *list;

    arg1 = (int) arg[1];
    arg2 = (int) arg[2];
    arg3 = (int) arg[3];
    arg4 = (int) arg[4];

    list = P_GetSectorIterListForTag((int) arg[0], false);
    if(!list)
        return rtn;

    IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
    IterList_RewindIterator(list);
    while((sec = IterList_MoveIterator(list)) != NULL)
    {
        think = false;
        rtn = true;

        light = Z_Calloc(sizeof(*light), PU_MAP, 0);
        light->type = type;
        light->sector = sec;
        light->count = 0;

        switch(type)
        {
        case LITE_RAISEBYVALUE:
            P_SectorModifyLight(light->sector, (float) arg1 / 255.0f);
            break;

        case LITE_LOWERBYVALUE:
            P_SectorModifyLight(light->sector, -((float) arg1 / 255.0f));
            break;

        case LITE_CHANGETOVALUE:
            P_SectorSetLight(light->sector, (float) arg1 / 255.0f);
            break;

        case LITE_FADE:
            think = true;
            light->value1 = (float) arg1 / 255.0f; // Destination lightlevel.
            light->value2 =
                FIX2FLT(FixedDiv((arg1 - (int) (255.0f * P_SectorLight(light->sector))) << FRACBITS,
                         arg2 << FRACBITS)) / 255.0f; // Delta lightlevel.
            if(P_SectorLight(light->sector) <= (float) arg1 / 255.0f)
            {
                light->tics2 = 1; // Get brighter.
            }
            else
            {
                light->tics2 = -1;
            }
            break;

        case LITE_GLOW:
            think = true;
            light->value1 = (float) arg1 / 255.0f; // Upper lightlevel.
            light->value2 = (float) arg2 / 255.0f; // Lower lightlevel.
            light->tics1 =
                FixedDiv((arg1 - (int) (255.0f * P_SectorLight(light->sector))) << FRACBITS,
                         arg3 << FRACBITS); // Lightlevel delta.
            if(P_SectorLight(light->sector) <= (float) arg1 / 255.0f)
            {
                light->tics2 = 1; // Get brighter.
            }
            else
            {
                light->tics2 = -1;
            }
            break;

        case LITE_FLICKER:
            think = true;
            light->value1 = (float) arg1 / 255.0f; // Upper lightlevel.
            light->value2 = (float) arg2 / 255.0f; // Lower lightlevel.
            P_SectorSetLight(light->sector, light->value1);
            light->count = (P_Random() & 64) + 1;
            break;

        case LITE_STROBE:
            think = true;
            light->value1 = (float) arg1 / 255.0f; // Upper lightlevel.
            light->value2 = (float) arg2 / 255.0f; // Lower lightlevel.
            light->tics1 = arg3; // Upper tics.
            light->tics2 = arg4; // Lower tics.
            light->count = arg3;
            P_SectorSetLight(light->sector, light->value1);
            break;

        default:
            rtn = false;
            break;
        }

        if(think)
        {
            light->thinker.function = (thinkfunc_t) T_Light;
            Thinker_Add(&light->thinker);
        }
        else
        {
            Z_Free(light);
        }
    }

    return rtn;
}

void T_Phase(phase_t *phase)
{
    phase->index = (phase->index + 1) & 63;
    P_SectorSetLight(phase->sector,
                     phase->baseValue + phaseTable[phase->index]);
}

void P_SpawnPhasedLight(Sector* sector, float base, int index)
{
    phase_t*            phase;

    phase = Z_Calloc(sizeof(*phase), PU_MAP, 0);
    phase->thinker.function = (thinkfunc_t) T_Phase;
    Thinker_Add(&phase->thinker);

    phase->sector = sector;
    if(index == -1)
    {   // Sector->lightLevel as the index.
        phase->index = (int) (255.0f * P_SectorLight(sector)) & 63;
    }
    else
    {
        phase->index = index & 63;
    }

    phase->baseValue = base;
    P_SectorSetLight(phase->sector,
                     phase->baseValue + phaseTable[phase->index]);

    P_ToXSector(sector)->special = 0;
}

typedef struct {
    int                 seqSpecial, count;
    Sector*             sec, *nextSec;
} findlightsequencesectorparams_t;

static int findLightSequenceSector(void* p, void* context)
{
    Line*            li = (Line*) p;
    findlightsequencesectorparams_t* params =
        (findlightsequencesectorparams_t*) context;
    Sector*             tempSec = P_GetNextSector(li, params->sec);

    if(tempSec)
    {
        if(P_ToXSector(tempSec)->special == params->seqSpecial)
        {
            if(params->seqSpecial == LIGHT_SEQUENCE)
                params->seqSpecial = LIGHT_SEQUENCE_ALT;
            else
                params->seqSpecial = LIGHT_SEQUENCE;

            params->nextSec = tempSec;
            params->count++;
        }
    }

    return false; // Continue iteration.
}

typedef struct {
    Sector*             sec, *nextSec;
} findlightsequencestartsectorparams_t;

static int findLightSequenceStartSector(void* p, void* context)
{
    Line*           li = (Line*) p;
    findlightsequencestartsectorparams_t* params =
        (findlightsequencestartsectorparams_t*) context;
    Sector*             tempSec = P_GetNextSector(li, params->sec);

    if(tempSec)
    {
        if(P_ToXSector(tempSec)->special == LIGHT_SEQUENCE_START)
        {
            params->nextSec = tempSec;
        }
    }

    return false; // Continue iteration.
}

void P_SpawnLightSequence(Sector* sector, int indexStep)
{
    int                 count;

    {
    findlightsequencesectorparams_t params;

    params.seqSpecial = LIGHT_SEQUENCE; // Look for Light_Sequence, first.
    params.count = 1;
    params.sec = sector;
    do
    {
        // Make sure that the search doesn't back up.
        P_ToXSector(params.sec)->special = LIGHT_SEQUENCE_START;

        params.nextSec = NULL;
        P_Iteratep(params.sec, DMU_LINE, &params,
                   findLightSequenceSector);
        params.sec = params.nextSec;
    } while(params.sec);

    count = params.count;
    }

    {
    findlightsequencestartsectorparams_t params;
    float               base;
    fixed_t             index, indexDelta;

    params.sec = sector;
    count *= indexStep;
    index = 0;
    indexDelta = FixedDiv(64 * FRACUNIT, count * FRACUNIT);
    base = P_SectorLight(sector);
    do
    {
        if(P_SectorLight(params.sec))
        {
            base = P_SectorLight(params.sec);
        }
        P_SpawnPhasedLight(params.sec, base, index >> FRACBITS);
        index += indexDelta;

        params.nextSec = NULL;
        P_Iteratep(params.sec, DMU_LINE, &params,
                   findLightSequenceStartSector);
        params.sec = params.nextSec;
    } while(params.sec);
    }
}
