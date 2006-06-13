
//**************************************************************************
//**
//** p_plats.c : Heretic 2 : Raven Software, Corp.
//**
//** $RCSfile$
//** $Revision$
//** $Date$
//** $Author$
//**
//**************************************************************************

#include "jhexen.h"

plat_t *activeplats[MAXPLATS];

static void StartSequence(sector_t* sector, int seqBase)
{
    SN_StartSequence(P_GetPtrp(sector, DMU_SOUND_ORIGIN),
                     seqBase + P_XSector(sector)->seqType);
}

static void StopSequence(sector_t* sector)
{
    SN_StopSequence(P_GetPtrp(sector, DMU_SOUND_ORIGIN));
}

//==================================================================
//
//      Move a plat up and down
//
//==================================================================
void T_PlatRaise(plat_t * plat)
{
    result_e res;

    switch (plat->status)
    {
    case PLAT_UP:
        res =
            T_MovePlane(plat->sector, plat->speed, plat->high, plat->crush, 0,
                        1);
        if(res == RES_CRUSHED && (!plat->crush))
        {
            plat->count = plat->wait;
            plat->status = PLAT_DOWN;
            StartSequence(plat->sector, SEQ_PLATFORM);
        }
        else if(res == RES_PASTDEST)
        {
            plat->count = plat->wait;
            plat->status = PLAT_WAITING;
            StopSequence(plat->sector);
            switch (plat->type)
            {
            case PLAT_DOWNWAITUPSTAY:
            case PLAT_DOWNBYVALUEWAITUPSTAY:
                P_RemoveActivePlat(plat);
                break;
            default:
                break;
            }
        }
        break;
    case PLAT_DOWN:
        res = T_MovePlane(plat->sector, plat->speed, plat->low, false, 0, -1);
        if(res == RES_PASTDEST)
        {
            plat->count = plat->wait;
            plat->status = PLAT_WAITING;
            switch (plat->type)
            {
            case PLAT_UPWAITDOWNSTAY:
            case PLAT_UPBYVALUEWAITDOWNSTAY:
                P_RemoveActivePlat(plat);
                break;
            default:
                break;
            }
            StopSequence(plat->sector);
        }
        break;
    case PLAT_WAITING:
        if(!--plat->count)
        {
            if(P_GetFixedp(plat->sector, DMU_FLOOR_HEIGHT) == plat->low)
                plat->status = PLAT_UP;
            else
                plat->status = PLAT_DOWN;
            StartSequence(plat->sector, SEQ_PLATFORM);
        }
        //      case PLAT_IN_STASIS:
        //          break;
    }
}

//==================================================================
//
//      Do Platforms
//      "amount" is only used for SOME platforms.
//
//==================================================================
int EV_DoPlat(line_t *line, byte *args, plattype_e type, int amount)
{
    plat_t *plat;
    int     secnum;
    int     rtn;
    sector_t *sec;
    fixed_t floorheight;

    secnum = -1;
    rtn = 0;

    /*
       //
       //      Activate all <type> plats that are in_stasis
       //
       switch(type)
       {
       case PLAT_PERPETUALRAISE:
       P_ActivateInStasis(args[0]);
       break;
       default:
       break;
       }
     */

    while((secnum = P_FindSectorFromTag(args[0], secnum)) >= 0)
    {
        sec = P_ToPtr(DMU_SECTOR, secnum);
        if(P_XSector(sec)->specialdata)
            continue;

        //
        // Find lowest & highest floors around sector
        //
        rtn = 1;
        plat = Z_Malloc(sizeof(*plat), PU_LEVSPEC, 0);
        P_AddThinker(&plat->thinker);

        plat->type = type;
        plat->sector = sec;
        P_XSector(plat->sector)->specialdata = plat;
        plat->thinker.function = T_PlatRaise;
        plat->crush = false;
        plat->tag = args[0];
        plat->speed = args[1] * (FRACUNIT / 8);
        floorheight = P_GetFixedp(sec, DMU_FLOOR_HEIGHT);
        switch (type)
        {
        case PLAT_DOWNWAITUPSTAY:
            plat->low = P_FindLowestFloorSurrounding(sec) + 8 * FRACUNIT;
            if(plat->low > floorheight)
                plat->low = floorheight;
            plat->high = floorheight;
            plat->wait = args[2];
            plat->status = PLAT_DOWN;
            break;
        case PLAT_DOWNBYVALUEWAITUPSTAY:
            plat->low = floorheight - args[3] * 8 * FRACUNIT;
            if(plat->low > floorheight)
                plat->low = floorheight;
            plat->high = floorheight;
            plat->wait = args[2];
            plat->status = PLAT_DOWN;
            break;
        case PLAT_UPWAITDOWNSTAY:
            plat->high = P_FindHighestFloorSurrounding(sec);
            if(plat->high < floorheight)
                plat->high = floorheight;
            plat->low = floorheight;
            plat->wait = args[2];
            plat->status = PLAT_UP;
            break;
        case PLAT_UPBYVALUEWAITDOWNSTAY:
            plat->high = floorheight + args[3] * 8 * FRACUNIT;
            if(plat->high < floorheight)
                plat->high = floorheight;
            plat->low = floorheight;
            plat->wait = args[2];
            plat->status = PLAT_UP;
            break;
        case PLAT_PERPETUALRAISE:
            plat->low = P_FindLowestFloorSurrounding(sec) + 8 * FRACUNIT;
            if(plat->low > floorheight)
                plat->low = floorheight;
            plat->high = P_FindHighestFloorSurrounding(sec);
            if(plat->high < floorheight)
                plat->high = floorheight;
            plat->wait = args[2];
            plat->status = P_Random() & 1;
            break;
        }
        P_AddActivePlat(plat);
        StartSequence(plat->sector, SEQ_PLATFORM);
    }
    return rtn;
}

void EV_StopPlat(line_t *line, byte *args)
{
    int     i;

    for(i = 0; i < MAXPLATS; i++)
    {
        if((activeplats[i])->tag == args[0])
        {
            P_XSector((activeplats[i])->sector)->specialdata = NULL;
            P_TagFinished(P_XSector((activeplats[i])->sector)->tag);
            P_RemoveThinker(&(activeplats[i])->thinker);
            activeplats[i] = NULL;
            return;
        }
    }
}

void P_AddActivePlat(plat_t * plat)
{
    int     i;

    for(i = 0; i < MAXPLATS; i++)
        if(activeplats[i] == NULL)
        {
            activeplats[i] = plat;
            return;
        }
    Con_Error("P_AddActivePlat: no more plats!");
}

void P_RemoveActivePlat(plat_t * plat)
{
    int     i;

    for(i = 0; i < MAXPLATS; i++)
        if(plat == activeplats[i])
        {
            P_XSector((activeplats[i])->sector)->specialdata = NULL;
            P_TagFinished(P_XSector(plat->sector)->tag);
            P_RemoveThinker(&(activeplats[i])->thinker);
            activeplats[i] = NULL;
            return;
        }
    Con_Error("P_RemoveActivePlat: can't find plat!");
}
