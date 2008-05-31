/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
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
 */

/**
 * p_waggle.c:
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "dmu_lib.h"
#include "p_map.h"
#include "p_mapspec.h"
#include "p_iterlist.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void T_FloorWaggle(waggle_t* waggle)
{
    float               fh;

    switch(waggle->state)
    {
    default:
    case WS_STABLE:
        if(waggle->ticker != -1)
        {
            if(!--waggle->ticker)
            {
                waggle->state = WS_REDUCE;
            }
        }
        break;

    case WS_EXPAND:
        if((waggle->scale += waggle->scaleDelta) >= waggle->targetScale)
        {
            waggle->scale = waggle->targetScale;
            waggle->state = WS_STABLE;
        }
        break;

    case WS_REDUCE:
        if((waggle->scale -= waggle->scaleDelta) <= 0)
        {   // Remove.
            P_SetFloatp(waggle->sector, DMU_FLOOR_HEIGHT,
                        waggle->originalHeight);
            P_ChangeSector(waggle->sector, true);
            P_ToXSector(waggle->sector)->specialData = NULL;
            P_TagFinished(P_ToXSector(waggle->sector)->tag);
            P_ThinkerRemove(&waggle->thinker);
            return;
        }
        break;
    }

    waggle->accumulator += waggle->accDelta;
    fh = waggle->originalHeight +
        FLOATBOBOFFSET(((int) waggle->accumulator) & 63) * waggle->scale;
    P_SetFloatp(waggle->sector, DMU_FLOOR_HEIGHT, fh);
    P_SetFloatp(waggle->sector, DMU_FLOOR_TARGET_HEIGHT, fh);
    P_SetFloatp(waggle->sector, DMU_FLOOR_SPEED, 0);
    P_ChangeSector(waggle->sector, true);
}

boolean EV_StartFloorWaggle(int tag, int height, int speed, int offset,
                            int timer)
{
    boolean             retCode = false;
    sector_t*           sec = NULL;
    waggle_t*      waggle;
    iterlist_t*         list;

    list = P_GetSectorIterListForTag(tag, false);
    if(!list)
        return retCode;

    P_IterListResetIterator(list, true);
    while((sec = P_IterListIterator(list)) != NULL)
    {
        if(P_ToXSector(sec)->specialData)
            continue; // Already moving, so keep going...

        retCode = true;
        waggle = Z_Calloc(sizeof(*waggle), PU_LEVSPEC, 0);
        P_ToXSector(sec)->specialData = waggle;
        waggle->thinker.function = T_FloorWaggle;
        waggle->sector = sec;
        waggle->originalHeight = P_GetFloatp(sec, DMU_FLOOR_HEIGHT);
        waggle->accumulator = offset;
        waggle->accDelta = FIX2FLT(speed << 10);
        waggle->scale = 0;
        waggle->targetScale = FIX2FLT(height << 10);
        waggle->scaleDelta =
            FIX2FLT(FLT2FIX(waggle->targetScale) / (TICSPERSEC + ((3 * TICSPERSEC) * height) / 255));
        waggle->ticker = timer ? timer * 35 : -1;
        waggle->state = WS_EXPAND;
        P_ThinkerAdd(&waggle->thinker);
    }

    return retCode;
}
