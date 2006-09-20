/**\file
 *\section Copyright and License Summary
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 *
 *\attention  2006-09-10 - We should be able to use jDoom version of this as
 * a base for replacement. - Yagisan
 */

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

/**
 * @param amount        Only used for SOME platforms.
 */
int EV_DoPlat(line_t *line, byte *args, plattype_e type, int amount)
{
    int         rtn = 0;
    fixed_t     floorheight;
    sector_t   *sec = NULL;
    plat_t     *plat;

    while((sec = P_FindSectorFromTag(args[0], sec)) != NULL)
    {
        if(P_XSector(sec)->specialdata)
            continue;

        // Find lowest & highest floors around sector
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
