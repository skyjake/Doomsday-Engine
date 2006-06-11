
//**************************************************************************
//**
//** p_ceilng.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

#include "h2def.h"
#include "jHexen/p_local.h"
#include "jHexen/soundst.h"
#include "Common/p_start.h"
#include "Common/dmu_lib.h"

//==================================================================
//==================================================================
//
//                                                      CEILINGS
//
//==================================================================
//==================================================================

ceiling_t *activeceilings[MAXCEILINGS];

//==================================================================
//
//      T_MoveCeiling
//
//==================================================================
void T_MoveCeiling(ceiling_t * ceiling)
{
    result_e res;

    switch (ceiling->direction)
    {
        //      case 0:         // IN STASIS
        //          break;
    case 1:                 // UP
        res = T_MovePlane(ceiling->sector, ceiling->speed, ceiling->topheight,
                          false, 1, ceiling->direction);
        if(res == RES_PASTDEST)
        {
            SN_StopSequence(P_SectorSoundOrigin(ceiling->sector));
            switch (ceiling->type)
            {
            case CLEV_CRUSHANDRAISE:
                ceiling->direction = -1;
                ceiling->speed = ceiling->speed * 2;
                break;
            default:
                P_RemoveActiveCeiling(ceiling);
                break;
            }
        }
        break;
    case -1:                    // DOWN
        res =
            T_MovePlane(ceiling->sector, ceiling->speed, ceiling->bottomheight,
                        ceiling->crush, 1, ceiling->direction);
        if(res == RES_PASTDEST)
        {
            SN_StopSequence(P_SectorSoundOrigin(ceiling->sector));
            switch (ceiling->type)
            {
            case CLEV_CRUSHANDRAISE:
            case CLEV_CRUSHRAISEANDSTAY:
                ceiling->direction = 1;
                ceiling->speed = ceiling->speed / 2;
                break;
            default:
                P_RemoveActiveCeiling(ceiling);
                break;
            }
        }
        else if(res == RES_CRUSHED)
        {
            switch (ceiling->type)
            {
            case CLEV_CRUSHANDRAISE:
            case CLEV_LOWERANDCRUSH:
            case CLEV_CRUSHRAISEANDSTAY:
                //ceiling->speed = ceiling->speed/4;
                break;
            default:
                break;
            }
        }
        break;
    }
}

//==================================================================
//
//              EV_DoCeiling
//              Move a ceiling up/down and all around!
//
//==================================================================
int EV_DoCeiling(line_t *line, byte *arg, ceiling_e type)
{
    int     secnum, rtn;
    sector_t *sec;
    ceiling_t *ceiling;

    secnum = -1;
    rtn = 0;

    /* Old Ceiling stasis code
       //
       //      Reactivate in-stasis ceilings...for certain types.
       //
       switch(type)
       {
       case CLEV_CRUSHANDRAISE:
       P_ActivateInStasisCeiling(line);
       default:
       break;
       }
     */
    while((secnum = P_FindSectorFromTag(arg[0], secnum)) >= 0)
    {
        sec = P_ToPtr(DMU_SECTOR, secnum);
        if(P_XSector(sec)->specialdata)
            continue;

        //
        // new door thinker
        //
        rtn = 1;
        ceiling = Z_Malloc(sizeof(*ceiling), PU_LEVSPEC, 0);
        P_AddThinker(&ceiling->thinker);
        P_XSector(sec)->specialdata = ceiling;
        ceiling->thinker.function = T_MoveCeiling;
        ceiling->sector = sec;
        ceiling->crush = 0;
        ceiling->speed = arg[1] * (FRACUNIT / 8);
        switch (type)
        {
        case CLEV_CRUSHRAISEANDSTAY:
            ceiling->crush = arg[2];    // arg[2] = crushing value
            ceiling->topheight = P_GetFixedp(sec, DMU_CEILING_HEIGHT);
            ceiling->bottomheight = P_GetFixedp(sec, DMU_FLOOR_HEIGHT) +
                (8 * FRACUNIT);
            ceiling->direction = -1;
            break;
        case CLEV_CRUSHANDRAISE:
            ceiling->topheight = P_GetFixedp(sec, DMU_CEILING_HEIGHT);
        case CLEV_LOWERANDCRUSH:
            ceiling->crush = arg[2];    // arg[2] = crushing value
        case CLEV_LOWERTOFLOOR:
            ceiling->bottomheight = P_GetFixedp(sec, DMU_FLOOR_HEIGHT);
            if(type != CLEV_LOWERTOFLOOR)
            {
                ceiling->bottomheight += 8 * FRACUNIT;
            }
            ceiling->direction = -1;
            break;
        case CLEV_RAISETOHIGHEST:
            ceiling->topheight = P_FindHighestCeilingSurrounding(sec);
            ceiling->direction = 1;
            break;
        case CLEV_LOWERBYVALUE:
            ceiling->bottomheight = P_GetFixedp(sec, DMU_CEILING_HEIGHT) -
                arg[2] * FRACUNIT;
            ceiling->direction = -1;
            break;
        case CLEV_RAISEBYVALUE:
            ceiling->topheight = P_GetFixedp(sec, DMU_CEILING_HEIGHT) +
                arg[2] * FRACUNIT;
            ceiling->direction = 1;
            break;
        case CLEV_MOVETOVALUETIMES8:
            {
                int     destHeight = arg[2] * FRACUNIT * 8;

                if(arg[3])
                {
                    destHeight = -destHeight;
                }
                if(P_GetFixedp(sec, DMU_CEILING_HEIGHT) <= destHeight)
                {
                    ceiling->direction = 1;
                    ceiling->topheight = destHeight;
                    if(P_GetFixedp(sec, DMU_CEILING_HEIGHT) == destHeight)
                    {
                        rtn = 0;
                    }
                }
                else if(P_GetFixedp(sec, DMU_CEILING_HEIGHT) > destHeight)
                {
                    ceiling->direction = -1;
                    ceiling->bottomheight = destHeight;
                }
                break;
            }
        default:
            rtn = 0;
            break;
        }
        ceiling->tag = P_XSector(sec)->tag;
        ceiling->type = type;
        P_AddActiveCeiling(ceiling);
        if(rtn)
        {
            SN_StartSequence(P_SectorSoundOrigin(ceiling->sector),
                             SEQ_PLATFORM + P_XSector(ceiling->sector)->seqType);
        }
    }
    return rtn;
}

//==================================================================
//
//              Add an active ceiling
//
//==================================================================
void P_AddActiveCeiling(ceiling_t * c)
{
    int     i;

    for(i = 0; i < MAXCEILINGS; i++)
        if(activeceilings[i] == NULL)
        {
            activeceilings[i] = c;
            return;
        }
}

//==================================================================
//
//              Remove a ceiling's thinker
//
//==================================================================
void P_RemoveActiveCeiling(ceiling_t * c)
{
    int     i;

    for(i = 0; i < MAXCEILINGS; i++)
        if(activeceilings[i] == c)
        {
            P_XSector(activeceilings[i]->sector)->specialdata = NULL;
            P_RemoveThinker(&activeceilings[i]->thinker);
            P_TagFinished(P_XSector(activeceilings[i]->sector)->tag);
            activeceilings[i] = NULL;
            break;
        }
}

//==================================================================
//
//              EV_CeilingCrushStop
//              Stop a ceiling from crushing!
//
//==================================================================

int EV_CeilingCrushStop(line_t *line, byte *args)
{
    int     i;
    int     rtn;

    rtn = 0;
    for(i = 0; i < MAXCEILINGS; i++)
    {
        if(activeceilings[i] && activeceilings[i]->tag == args[0])
        {
            rtn = 1;
            SN_StopSequence(P_SectorSoundOrigin(activeceilings[i]->sector));
            P_XSector(activeceilings[i]->sector)->specialdata = NULL;
            P_RemoveThinker(&activeceilings[i]->thinker);
            P_TagFinished(P_XSector(activeceilings[i]->sector)->tag);
            activeceilings[i] = NULL;
            break;
        }
    }
    return rtn;
}
