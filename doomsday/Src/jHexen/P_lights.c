
//**************************************************************************
//**
//** p_lights.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

#include "h2def.h"
#include "jHexen/p_local.h"
#include "Common/p_start.h"
#include "Common/dmu_lib.h"

void T_Light(light_t * light)
{
    if(light->count)
    {
        light->count--;
        return;
    }
    switch (light->type)
    {
    case LITE_FADE:
        P_SectorModifyLightx(light->sector, light->value2);

        if(light->tics2 == 1)
        {
            if(P_SectorLight(light->sector) >= light->value1)
            {
                P_SectorSetLight(light->sector, light->value1);
                P_RemoveThinker(&light->thinker);
            }
        }
        else if(P_SectorLight(light->sector) <= light->value1)
        {
            P_SectorSetLight(light->sector, light->value1);
            P_RemoveThinker(&light->thinker);
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
                light->tics2 = -1;  // reverse direction
            }
        }
        else if(P_SectorLight(light->sector) <= light->value2)
        {
            P_SectorSetLight(light->sector, light->value2);
            light->tics1 = -light->tics1;
            light->tics2 = 1;   // reverse direction
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

//============================================================================
//
//  EV_SpawnLight
//
//============================================================================

boolean EV_SpawnLight(line_t *line, byte *arg, lighttype_t type)
{
    light_t *light;
    int     secNum;
    int     arg1, arg2, arg3, arg4;
    boolean think;
    boolean rtn;

    arg1 = arg[1];
    arg2 = arg[2];
    arg3 = arg[3];
    arg4 = arg[4];

    secNum = -1;
    rtn = false;
    think = false;
    while((secNum = P_FindSectorFromTag(arg[0], secNum)) >= 0)
    {
        think = false;
        light = (light_t *) Z_Malloc(sizeof(light_t), PU_LEVSPEC, 0);
        light->type = type;
        light->sector = P_ToPtr(DMU_SECTOR, secNum);
        light->count = 0;
        rtn = true;
        switch (type)
        {
        case LITE_RAISEBYVALUE:
            P_SectorModifyLight(light->sector, arg1);
            break;
        case LITE_LOWERBYVALUE:
            P_SectorModifyLight(light->sector, -arg1);
            break;
        case LITE_CHANGETOVALUE:
            P_SectorSetLight(light->sector, arg1);
            break;
        case LITE_FADE:
            think = true;
            light->value1 = arg1;   // destination lightlevel
            light->value2 = FixedDiv((arg1 - P_SectorLight(light->sector)) << FRACBITS, arg2 << FRACBITS);  // delta lightlevel
            if(P_SectorLight(light->sector) <= arg1)
            {
                light->tics2 = 1;   // get brighter
            }
            else
            {
                light->tics2 = -1;
            }
            break;
        case LITE_GLOW:
            think = true;
            light->value1 = arg1;   // upper lightlevel
            light->value2 = arg2;   // lower lightlevel
            light->tics1 = FixedDiv((arg1 - P_SectorLight(light->sector)) << FRACBITS, arg3 << FRACBITS);   // lightlevel delta
            if(P_SectorLight(light->sector) <= arg1)
            {
                light->tics2 = 1;   // get brighter
            }
            else
            {
                light->tics2 = -1;
            }
            break;
        case LITE_FLICKER:
            think = true;
            light->value1 = arg1;   // upper lightlevel
            light->value2 = arg2;   // lower lightlevel
            P_SectorSetLight(light->sector, light->value1);
            light->count = (P_Random() & 64) + 1;
            break;
        case LITE_STROBE:
            think = true;
            light->value1 = arg1;   // upper lightlevel
            light->value2 = arg2;   // lower lightlevel
            light->tics1 = arg3;    // upper tics
            light->tics2 = arg4;    // lower tics
            light->count = arg3;
            P_SectorSetLight(light->sector, light->value1);
            break;
        default:
            rtn = false;
            break;
        }
        if(think)
        {
            P_AddThinker(&light->thinker);
            light->thinker.function = T_Light;
        }
        else
        {
            Z_Free(light);
        }
    }
    return rtn;
}

//============================================================================
//
//  T_Phase
//
//============================================================================

int     PhaseTable[64] = {
    128, 112, 96, 80, 64, 48, 32, 32,
    16, 16, 16, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 16, 16, 16,
    32, 32, 48, 64, 80, 96, 112, 128
};

void T_Phase(phase_t * phase)
{
    phase->index = (phase->index + 1) & 63;
    P_SectorSetLight(phase->sector, phase->base + PhaseTable[phase->index]);
}

//==========================================================================
//
// P_SpawnPhasedLight
//
//==========================================================================

void P_SpawnPhasedLight(sector_t *sector, int base, int index)
{
    phase_t *phase;

    phase = Z_Malloc(sizeof(*phase), PU_LEVSPEC, 0);
    P_AddThinker(&phase->thinker);
    phase->sector = sector;
    if(index == -1)
    {                           // sector->lightlevel as the index
        phase->index = P_SectorLight(sector) & 63;
    }
    else
    {
        phase->index = index & 63;
    }
    phase->base = base & 255;
    P_SectorSetLight(phase->sector, phase->base + PhaseTable[phase->index]);
    phase->thinker.function = T_Phase;

    P_XSector(sector)->special = 0;
}

//==========================================================================
//
// P_SpawnLightSequence
//
//==========================================================================

void P_SpawnLightSequence(sector_t *sector, int indexStep)
{
    sector_t *sec;
    sector_t *nextSec;
    sector_t *tempSec;
    int     seqSpecial;
    int     i;
    int     count;
    fixed_t index;
    fixed_t indexDelta;
    int     base;

    seqSpecial = LIGHT_SEQUENCE;    // look for Light_Sequence, first
    sec = sector;
    count = 1;
    do
    {
        nextSec = NULL;
        P_XSector(sec)->special = LIGHT_SEQUENCE_START; // make sure that the search doesn't back up.
        for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); i++)
        {
            tempSec = getNextSector(P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i), sec);
            if(!tempSec)
            {
                continue;
            }
            if(P_XSector(tempSec)->special == seqSpecial)
            {
                if(seqSpecial == LIGHT_SEQUENCE)
                {
                    seqSpecial = LIGHT_SEQUENCE_ALT;
                }
                else
                {
                    seqSpecial = LIGHT_SEQUENCE;
                }
                nextSec = tempSec;
                count++;
            }
        }
        sec = nextSec;
    } while(sec);

    sec = sector;
    count *= indexStep;
    index = 0;
    indexDelta = FixedDiv(64 * FRACUNIT, count * FRACUNIT);
    base = P_SectorLight(sector);
    do
    {
        nextSec = NULL;
        if(P_SectorLight(sec))
        {
            base = P_SectorLight(sec);
        }
        P_SpawnPhasedLight(sec, base, index >> FRACBITS);
        index += indexDelta;
        for(i = 0; i < P_GetIntp(sec, DMU_LINE_COUNT); i++)
        {
            tempSec = getNextSector(P_GetPtrp(sec, DMU_LINE_OF_SECTOR | i), sec);
            if(!tempSec)
            {
                continue;
            }
            if(P_XSector(tempSec)->special == LIGHT_SEQUENCE_START)
            {
                nextSec = tempSec;
            }
        }
        sec = nextSec;
    } while(sec);
}
