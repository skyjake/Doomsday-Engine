/** @file p_mapspec.cpp  World map utilities.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#include "common.h"
#include "p_mapspec.h"

#include "dmu_lib.h"
#include "g_game.h"
#include "gamesession.h"
#include "p_mapsetup.h"
#include "acs/system.h"

#include <doomsday/world/lineopening.h>

using namespace de;

iterlist_t *spechit;  ///< For crossed line specials.

struct spreadsoundtoneighbors_params_t
{
    Sector *baseSec;
    int soundBlocks;
    mobj_t *soundTarget;
};

/// @return  0= continue iteration; otherwise stop.
static int spreadSoundToNeighbors(void *ptr, void *context)
{
    Line *li = (Line *) ptr;
    spreadsoundtoneighbors_params_t &parm = *(spreadsoundtoneighbors_params_t *) context;

    xline_t *xline = P_ToXLine(li);
    DE_ASSERT(xline);

    if(!(xline->flags & ML_TWOSIDED)) return false;

    Sector *frontSec = (Sector *) P_GetPtrp(li, DMU_FRONT_SECTOR);
    if(!frontSec) return false;

    Sector *backSec  = (Sector *) P_GetPtrp(li, DMU_BACK_SECTOR);
    if(!backSec) return false;

    LineOpening opening;
    Line_Opening(li, &opening);
    if(opening.range <= 0) return false;

    Sector *other = (frontSec == parm.baseSec? backSec : frontSec);

    if(xline->flags & ML_SOUNDBLOCK)
    {
        if(!parm.soundBlocks)
            P_RecursiveSound(parm.soundTarget, other, 1);
    }
    else
    {
        P_RecursiveSound(parm.soundTarget, other, parm.soundBlocks);
    }

    return false; // Continue iteration.
}

void P_RecursiveSound(mobj_t *soundTarget, Sector *sec, int soundBlocks)
{
    xsector_t *xsec = P_ToXSector(sec);
    DE_ASSERT(xsec);

    if(P_GetIntp(sec, DMU_VALID_COUNT) == VALIDCOUNT)
    {
        // Already flooded?
        if(xsec->soundTraversed <= soundBlocks + 1)
            return;
    }

    // Wake up all monsters in this sector.

    P_SetIntp(sec, DMU_VALID_COUNT, VALIDCOUNT);
    xsec->soundTraversed = soundBlocks + 1;
    xsec->soundTarget    = soundTarget;

    spreadsoundtoneighbors_params_t parm; de::zap(parm);
    parm.baseSec     = sec;
    parm.soundBlocks = soundBlocks;
    parm.soundTarget = soundTarget;
    P_Iteratep(sec, DMU_LINE, spreadSoundToNeighbors, &parm);
}

#if 0
dd_bool P_SectorTagIsBusy(int tag)
{
    /// @note The sector tag lists cannot be used here as an iteration at a higher
    /// level may already be in progress.
    for(int i = 0; i < numsectors; ++i)
    {
        xsector_t *xsec = P_ToXSector((Sector *) P_ToPtr(DMU_SECTOR, i));
        if(xsec->tag == tag && xsec->specialData)
        {
            return true;
        }
    }
    return false;
}
#endif

void P_NotifySectorFinished(int tag)
{
#if __JHEXEN__
    gfw_Session()->acsSystem().forAllScripts([&tag] (acs::Script &script)
    {
        script.sectorFinished(tag);
        return LoopContinue;
    });
#else
    DE_UNUSED(tag);
#endif
}

void P_NotifyPolyobjFinished(int tag)
{
#if __JHEXEN__
    gfw_Session()->acsSystem().forAllScripts([&tag] (acs::Script &script)
    {
        script.polyobjFinished(tag);
        return LoopContinue;
    });
#else
    DE_UNUSED(tag);
#endif
}
