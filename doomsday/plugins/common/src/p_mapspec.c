/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * Line Tag handling. Line and Sector groups. Specialized iteration
 * routines, respective utility functions...
 */

// HEADER FILES ------------------------------------------------------------

#include <assert.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

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

iterlist_t* spechit = 0; /// For crossed line specials.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

int spreadSoundToNeighbors(void* ptr, void* context)
{
    spreadsoundtoneighborsparams_t* params = (spreadsoundtoneighborsparams_t*) context;
    linedef_t* li = (linedef_t*) ptr;
    sector_t* frontSec = P_GetPtrp(li, DMU_FRONT_SECTOR);
    sector_t* backSec = P_GetPtrp(li, DMU_BACK_SECTOR);

    if(!(frontSec && backSec))
        return 1; // Continue iteration.

    P_LineOpening(li);
    if(!(OPENRANGE > 0))
        return 1; // Continue iteration.
    {
    sector_t* other = (frontSec == params->baseSec? backSec : frontSec);
    xline_t* xline = P_ToXLine(li);
    assert(xline);
    if(xline->flags & ML_SOUNDBLOCK)
    {
        if(!params->soundBlocks)
            P_RecursiveSound(params->soundTarget, other, 1);
    }
    else
    {
        P_RecursiveSound(params->soundTarget, other, params->soundBlocks);
    }
    }
    return 1; // Continue iteration.
}

void P_RecursiveSound(struct mobj_s* soundTarget, sector_t* sec, int soundBlocks)
{
    spreadsoundtoneighborsparams_t params;
    xsector_t* xsec = P_ToXSector(sec);

    // Wake up all monsters in this sector.
    if(P_GetIntp(sec, DMU_VALID_COUNT) == VALIDCOUNT && xsec->soundTraversed <= soundBlocks + 1)
        return; // Already flooded.

    P_SetIntp(sec, DMU_VALID_COUNT, VALIDCOUNT);
    xsec->soundTraversed = soundBlocks + 1;
    xsec->soundTarget = soundTarget;

    params.baseSec = sec;
    params.soundBlocks = soundBlocks;
    params.soundTarget = soundTarget;
    P_Iteratep(sec, DMU_LINEDEF, &params, spreadSoundToNeighbors);
}
