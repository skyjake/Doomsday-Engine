/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#if  __DOOM64TC__
#  include "doom64tc.h"
#  include "g_common.h"
#elif __WOLFTC__
#  include "wolftc.h"
#  include "g_common.h"
#elif __JDOOM__
#  include "jdoom.h"
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

// MACROS ------------------------------------------------------------------

#define VIEW_HEIGHT  (cfg.plrViewHeight)

#define MAXBOB  0x100000        // 16 pixels of bob.

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Calculate the walking / running height adjustment.
 */
void P_CalcHeight(player_t *player)
{
    boolean setz = (player == &players[consoleplayer]);
    boolean airborne;
    boolean morphed = false;
    ddplayer_t *dplay = player->plr;
    mobj_t *pmo = dplay->mo;
    float   zOffset, target, step;
    static int aircounter = 0;

    // Regular movement bobbing.
    // (needs to be calculated for gun swing even if not on ground)
    player->bob =
        FixedMul(pmo->mom[MX], pmo->mom[MX]) + FixedMul(pmo->mom[MY], pmo->mom[MY]);
    player->bob >>= 2;
    if(player->bob > MAXBOB)
        player->bob = MAXBOB;

    // When flying, don't bob the view.
    if((pmo->flags2 & MF2_FLY) && pmo->pos[VZ] > FLT2FIX(pmo->floorz))
    {
        player->bob = FRACUNIT / 2;
    }

#if __JHERETIC__ || __JHEXEN__
    if(player->morphTics)
        morphed = true;
#endif

    // During demo playback the view is thought to be airborne
    // if viewheight is zero (Cl_MoveLocalPlayer).
    if(Get(DD_PLAYBACK))
        airborne = !dplay->viewheight;
    else
        airborne = pmo->pos[VZ] > FLT2FIX(pmo->floorz);    // Truly in the air?

    // Should view bobbing be done?
    if(setz)
    {
        if(P_IsCamera(dplay->mo) /*$democam*/ || (dplay->flags & DDPF_CHASECAM)
           || (P_GetPlayerCheats(player) & CF_NOMOMENTUM) || airborne || morphed)    // Morphed players don't bob their view.
        {
            // Reduce the bob offset to zero.
            target = 0;
        }
        else
        {
            angle_t angle = (FINEANGLES / 20 * leveltime) & FINEMASK;
            target = cfg.bobView * FIX2FLT(FixedMul(player->bob / 2, finesine[angle]));
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
    if(!((P_GetPlayerCheats(player) & CF_NOMOMENTUM) || P_IsCamera(pmo) /*$democam*/ ||
         Get(DD_PLAYBACK)))
    {
        // Move viewheight.
        if(player->playerstate == PST_LIVE)
        {
            dplay->viewheight += dplay->deltaviewheight;

            if(dplay->viewheight > VIEW_HEIGHT)
            {
                dplay->viewheight = VIEW_HEIGHT;
                dplay->deltaviewheight = 0;
            }
            if(dplay->viewheight < VIEW_HEIGHT / 2.0f)
            {
                dplay->viewheight = VIEW_HEIGHT / 2.0f;
                if(dplay->deltaviewheight <= 0)
                    dplay->deltaviewheight = 1;
            }
            if(dplay->deltaviewheight)
            {
                dplay->deltaviewheight += 0.25f;
                if(!dplay->deltaviewheight)
                    dplay->deltaviewheight = 1;
            }
        }
    }

    // Set the player's eye-level Z coordinate.
    dplay->viewZ = FIX2FLT(pmo->pos[VZ]) +
                     (P_IsCamera(pmo)? 0 : dplay->viewheight);

    // During demo playback (or camera mode) the viewz will not be
    // modified any further.
    if(!(Get(DD_PLAYBACK) || P_IsCamera(pmo) || (dplay->flags & DDPF_CHASECAM)))
    {
        if(morphed)
        {
            // Chicken or pig.
            dplay->viewZ -= 20;
        }
        // Foot clipping is done for living players.
        if(player->playerstate != PST_DEAD)
        {
            // Foot clipping is done for living players.
            if(pmo->floorclip && FIX2FLT(pmo->pos[VZ]) <= pmo->floorz)
            {
                dplay->viewZ -= pmo->floorclip;
            }
        }
    }
}
