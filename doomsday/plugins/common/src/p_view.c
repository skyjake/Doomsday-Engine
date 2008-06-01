/**\file
 *\section License
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/**
 * p_view.c
 */

// HEADER FILES ------------------------------------------------------------

#if __WOLFTC__
#  include "wolftc.h"
#  include "g_common.h"
#elif __JDOOM__
#  include "jdoom.h"
#  include "g_common.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#  include "g_common.h"
#elif __JHERETIC__
#  include "jheretic.h"
#  include "g_common.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "p_player.h"
#include "p_tick.h"

// MACROS ------------------------------------------------------------------

#define VIEW_HEIGHT         (cfg.plrViewHeight)

#define MAXBOB              (16) // pixels.

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Calculate the walking / running height adjustment.
 */
void P_CalcHeight(player_t *player)
{
    boolean     setz = (player == &players[CONSOLEPLAYER]);
    boolean     airborne;
    boolean     morphed = false;
    ddplayer_t *dplay = player->plr;
    mobj_t     *pmo = dplay->mo;
    float       zOffset, target, step;
    static int aircounter = 0;

    // Regular movement bobbing (needs to be calculated for gun swing even
    // if not on ground).
    player->bob =
        (pmo->mom[MX] * pmo->mom[MX]) + (pmo->mom[MY] * pmo->mom[MY]);
    player->bob /= 2;
    if(player->bob > MAXBOB)
        player->bob = MAXBOB;

    // When flying, don't bob the view.
    if((pmo->flags2 & MF2_FLY) && pmo->pos[VZ] > pmo->floorZ)
    {
        player->bob = 1.0f / 2;
    }

#if __JHERETIC__ || __JHEXEN__
    if(player->morphTics)
        morphed = true;
#endif

    // During demo playback the view is thought to be airborne if viewheight
    // is zero (Cl_MoveLocalPlayer).
    if(Get(DD_PLAYBACK))
        airborne = !dplay->viewHeight;
    else
        airborne = pmo->pos[VZ] > pmo->floorZ; // Truly in the air?

    // Should view bobbing be done?
    if(setz)
    {   // Morphed players don't bob their view.
        if(P_IsCamera(dplay->mo) /*$democam*/ ||
           (dplay->flags & DDPF_CHASECAM) || airborne || morphed ||
           (P_GetPlayerCheats(player) & CF_NOMOMENTUM))
        {
            // Reduce the bob offset to zero.
            target = 0;
        }
        else
        {
            angle_t angle = (FINEANGLES / 20 * levelTime) & FINEMASK;
            target = cfg.bobView * ((player->bob / 2) * FIX2FLT(finesine[angle]));
        }

        // Do the change gradually.
        zOffset = *((float*) DD_GetVariable(DD_VIEWZ_OFFSET));
        if(airborne || aircounter > 0)
            step = 4.0f - (aircounter > 0 ? aircounter * 0.2f : 3.5f);
        else
            step = 4.0f;

        if(zOffset > target)
        {
            if(zOffset - target > step)
                zOffset -= step;
            else
                zOffset = target;
        }
        else if(zOffset < target)
        {
            if(target - zOffset > step)
                zOffset += step;
            else
                zOffset = target;
        }

        DD_SetVariable(DD_VIEWZ_OFFSET, &zOffset);

        // The aircounter will soften the touchdown after a fall.
        aircounter--;
        if(airborne)
            aircounter = TICSPERSEC / 2;
    }

    // Should viewheight be moved? Not if camera or we're in demo.
    if(!((P_GetPlayerCheats(player) & CF_NOMOMENTUM) ||
        P_IsCamera(pmo) /*$democam*/ || Get(DD_PLAYBACK)))
    {
        // Move viewheight.
        if(player->playerState == PST_LIVE)
        {
            dplay->viewHeight += dplay->viewHeightDelta;

            if(dplay->viewHeight > VIEW_HEIGHT)
            {
                dplay->viewHeight = VIEW_HEIGHT;
                dplay->viewHeightDelta = 0;
            }
            else if(dplay->viewHeight < VIEW_HEIGHT / 2.0f)
            {
                dplay->viewHeight = VIEW_HEIGHT / 2.0f;
                if(dplay->viewHeightDelta <= 0)
                    dplay->viewHeightDelta = 1;
            }

            if(dplay->viewHeightDelta)
            {
                dplay->viewHeightDelta += 0.25f;
                if(!dplay->viewHeightDelta)
                    dplay->viewHeightDelta = 1;
            }
        }
    }

    // Set the player's eye-level Z coordinate.
    dplay->viewZ = pmo->pos[VZ] +
                     (P_IsCamera(pmo)? 0 : dplay->viewHeight);

    // During demo playback (or camera mode) the viewz will not be modified
    // any further.
    if(!(Get(DD_PLAYBACK) || P_IsCamera(pmo) ||
       (dplay->flags & DDPF_CHASECAM)))
    {
        if(morphed)
        {   // Chicken or pig.
            dplay->viewZ -= 20;
        }

        // Foot clipping is done for living players.
        if(player->playerState != PST_DEAD)
        {
            if(pmo->floorClip && pmo->pos[VZ] <= pmo->floorZ)
            {
                dplay->viewZ -= pmo->floorClip;
            }
        }
    }
}
