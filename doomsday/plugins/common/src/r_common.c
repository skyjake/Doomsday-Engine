/**\file r_common.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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
 * Common routines for refresh.
 */

// HEADER FILES ------------------------------------------------------------

#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
# include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "am_map.h"
#include "p_actor.h"
#include "p_player.h"
#include "g_common.h"

#include "r_common.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int gammaLevel;
#ifndef __JHEXEN__
char gammamsg[5][81];
#endif

// CODE --------------------------------------------------------------------

void R_PrecachePSprites(void)
{
    int                 i, k;
    int                 pclass = players[CONSOLEPLAYER].class;

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        for(k = 0; k < NUMWEAPLEVELS; ++k)
        {
            pclass = players[CONSOLEPLAYER].class;

            R_PrecacheSkinsForState(weaponInfo[i][pclass].mode[k].states[WSN_UP]);
            R_PrecacheSkinsForState(weaponInfo[i][pclass].mode[k].states[WSN_DOWN]);
            R_PrecacheSkinsForState(weaponInfo[i][pclass].mode[k].states[WSN_READY]);
            R_PrecacheSkinsForState(weaponInfo[i][pclass].mode[k].states[WSN_ATTACK]);
            R_PrecacheSkinsForState(weaponInfo[i][pclass].mode[k].states[WSN_FLASH]);
#if __JHERETIC__ || __JHEXEN__
            R_PrecacheSkinsForState(weaponInfo[i][pclass].mode[k].states[WSN_ATTACK_HOLD]);
#endif
        }
    }
}

/// @return  @c true= maximized view window is in effect.
static boolean maximizedViewWindow(int player)
{
    player_t* plr = players + player;
    if(player < 0 || player >= MAXPLAYERS)
    {
        Con_Error("maximizedViewWindow: Invalid player #%i.", player);
        exit(1);
    }
    return (!(G_GetGameState() == GS_MAP && cfg.screenBlocks <= 10 &&
              !(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK)))); // $democam: can be set on every game tic.
}

static void resizeViewWindow(int player, const rectanglei_t* newDimensions,
    const rectanglei_t* oldDimensions, boolean interpolate)
{
    assert(newDimensions != NULL);
    {
    player_t* plr = players + player;
    rectanglei_t dims;

    if(player < 0 || player >= MAXPLAYERS)
    {
        Con_Error("resizeViewWindow: Invalid player #%i.", player);
        exit(1);
    }

    memcpy(&dims, newDimensions, sizeof(dims));
    dims.x = dims.y = 0;

    // Override @c cfg.screenBlocks and force a maximized window?
    if(!maximizedViewWindow(player) && cfg.screenBlocks <= 10)
    {
        const float xScale = (float)dims.width  / SCREENWIDTH;
        const float yScale = (float)dims.height / SCREENHEIGHT;
#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
        float fscale = cfg.statusbarScale;
        int statusBarHeight, needWidth;

        needWidth = yScale * ST_WIDTH;
        if(needWidth > dims.width)
            fscale *= (float)dims.width/needWidth;
        statusBarHeight = floor(ST_HEIGHT * fscale);
#endif

        if(cfg.screenBlocks != 10)
        {
            dims.width = cfg.screenBlocks * SCREENWIDTH/10;
            dims.x = SCREENWIDTH/2 - dims.width/2;
#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
            dims.height = cfg.screenBlocks * (SCREENHEIGHT - statusBarHeight) / 10;
            dims.y = (SCREENHEIGHT - statusBarHeight - dims.height) / 2;
#else
            dims.height = cfg.screenBlocks * SCREENHEIGHT/10;
            dims.y = (SCREENHEIGHT - dims.height) / 2;
#endif
        }
#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
        else
        {
            dims.x = 0;
            dims.y = 0;
            dims.width  = SCREENWIDTH;
            dims.height = SCREENHEIGHT - statusBarHeight;
        }
#endif
        // Scale from fixed to viewport coordinates.
        dims.x = ROUND(dims.x * xScale);
        dims.y = ROUND(dims.y * yScale);
        dims.width  = ROUND(dims.width  * xScale);
        dims.height = ROUND(dims.height * yScale);
    }

    R_SetViewWindowDimensions(player, dims.x, dims.y, dims.width, dims.height, interpolate);
    }
}

void R_ResizeViewWindow(int flags)
{
    static boolean oldMaximized;
    int i, delta, destBlocks = MINMAX_OF(3, cfg.setBlocks, 13);
    boolean maximized;
    rectanglei_t dims;

    // Override @c cfg.screenBlocks and force a maximized window?
    maximized = maximizedViewWindow(DISPLAYPLAYER);
    if(maximized != oldMaximized)
    {
        oldMaximized = maximized;
        flags |= RWF_FORCE;
    }

    if(!(flags & RWF_FORCE))
    {
        if(cfg.screenBlocks == destBlocks)
            return;
    }

    delta = MINMAX_OF(-1, destBlocks - cfg.screenBlocks, 1);
    if(delta != 0)
    {
        if(cfg.screenBlocks >= 10 && destBlocks != 13)
        {   // When going fullscreen, force a hud show event (to reset the timer).
            for(i = 0; i < MAXPLAYERS; ++i)
                ST_HUDUnHide(i, HUE_FORCE);
        }

        if((cfg.screenBlocks == 11 && destBlocks == 10) ||
           (cfg.screenBlocks == 10 && destBlocks == 11))
        {   // When going to/from statusbar span, do an instant change.
            flags |= RWF_NO_LERP;
        }

        cfg.screenBlocks += delta;
        flags |= RWF_FORCE;
    }

    if(!(flags & RWF_FORCE))
    {
        // No update necessary.
        return;
    }

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!R_ViewportDimensions(i, &dims.x, &dims.y, &dims.width, &dims.height))
        {
            // Player is not local or does not have a viewport.
            continue;
        }
        resizeViewWindow(i, &dims, &dims, (flags & RWF_NO_LERP)==0);
    }
}

int R_UpdateViewport(int hookType, int param, void* data)
{
    const ddhook_viewport_reshape_t* p = (ddhook_viewport_reshape_t*)data;
    resizeViewWindow(param, &p->dimensions, &p->oldDimensions, false);
    return true;
}

#ifndef __JHEXEN__
void R_GetGammaMessageStrings(void)
{
    int                 i;

    // Init some strings.
    for(i = 0; i < 5; ++i)
        strcpy(gammamsg[i], GET_TXT(TXT_GAMMALVL0 + i));
}
#endif

void R_CycleGammaLevel(void)
{
    char buf[50];

    if(GA_QUIT == G_GetGameAction())
    {
        return;
    }

    gammaLevel++;
    if(gammaLevel > 4)
        gammaLevel = 0;

#if __JDOOM__ || __JDOOM64__
    P_SetMessage(players + CONSOLEPLAYER, gammamsg[gammaLevel], true);
#endif

    sprintf(buf, "rend-tex-gamma %f", ((float) gammaLevel / 8.0f) * 1.5f);
    DD_Execute(false, buf);
}
