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

// View window current position.
static int windowX = 0, oldWindowX = 0;
static int windowY = 0, oldWindowY = 0;
static int windowWidth = SCREENWIDTH, oldWindowWidth = SCREENWIDTH;
static int windowHeight = SCREENHEIGHT, oldWindowHeight = SCREENHEIGHT;
static int targetX = -1, targetY = -1, targetWidth = -1, targetHeight = -1;
static float windowPos = 0;

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

void R_SetViewWindowTarget(int x, int y, int w, int h)
{
    if(targetX == -1)
    {   // First call.
        windowX = oldWindowX = targetX = x;
        windowY = oldWindowY = targetY = y;
        windowWidth = oldWindowWidth = targetWidth = w;
        windowHeight = oldWindowHeight = targetHeight = h;
        return;
    }

    if(x == targetX && y == targetY && w == targetWidth && h == targetHeight)
    {
        return;
    }

    oldWindowX = windowX;
    oldWindowY = windowY;
    oldWindowWidth = windowWidth;
    oldWindowHeight = windowHeight;

    // Restart the timer.
    windowPos = 0;

    targetX = x;
    targetY = y;
    targetWidth = w;
    targetHeight = h;
}

void R_UpdateViewWindow(boolean force)
{
    int destBlocks = MINMAX_OF(3, cfg.setBlocks, 13);
    boolean instantChange = force;
    int x = 0, y = 0, w = SCREENWIDTH, h = SCREENHEIGHT;

    if(!force && cfg.screenBlocks == cfg.setBlocks)
        return;

    if(cfg.screenBlocks >= 10 && destBlocks != 13)
    {   // When going fullscreen, force a hud show event (to reset the timer).
        int i;
        for(i = 0; i < MAXPLAYERS; ++i)
            ST_HUDUnHide(i, HUE_FORCE);
    }
    if((cfg.screenBlocks == 11 && destBlocks == 10) ||
       (cfg.screenBlocks == 10 && destBlocks == 11))
    {   // When going to/from statusbar span, do an instant change.
        instantChange = true;
    }

    if(destBlocks > cfg.screenBlocks)
        cfg.screenBlocks++;
    else if(destBlocks < cfg.screenBlocks)
        cfg.screenBlocks--;

    if(cfg.screenBlocks <= 10)
    {
#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
        float fscale = cfg.statusbarScale;
        int statusBarHeight, needWidth, viewW, viewH;

        R_GetViewPort(0, NULL, NULL, &viewW, &viewH);

        needWidth = (float)viewH/SCREENHEIGHT * ST_WIDTH;
        if(needWidth > viewW)
            fscale *= (float)viewW/needWidth;
        statusBarHeight = floor(ST_HEIGHT * fscale);
#endif

        if(cfg.screenBlocks != 10)
        {
            w = cfg.screenBlocks * SCREENWIDTH/10;
            x = SCREENWIDTH/2 - w/2;
#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
            h = cfg.screenBlocks * (SCREENHEIGHT - statusBarHeight) / 10;
            y = (SCREENHEIGHT - statusBarHeight - h) / 2;
#else
            h = cfg.screenBlocks * SCREENHEIGHT/10;
            y = (SCREENHEIGHT - h) / 2;
#endif
        }
#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
        else
        {
            h -= statusBarHeight;
        }
#endif
    }

    R_SetViewWindowTarget(x, y, w, h);
    if(instantChange)
        windowPos = 1;
}

/**
 * Animates the game view window towards the target values.
 */
void R_ViewWindowTicker(timespan_t ticLength)
{
    static int oldViewW = -1, oldViewH = -1;

    int destBlocks = MINMAX_OF(3, cfg.setBlocks, 13);
    int viewW, viewH;

    /**
     * \kludge \fixme Doomsday does not notify us when viewport dimensions change
     * so we must track them here.
     */
    R_GetViewPort(0, NULL, NULL, &viewW, &viewH);

    R_UpdateViewWindow(viewW != oldViewW || viewH != oldViewH);

    oldViewW = viewW;
    oldViewH = viewH;

    if(targetX == -1)
        return; // Nothing to do.

    windowPos += (float)(.4 * ticLength * TICRATE);
    if(windowPos >= 1)
    {
        windowX = targetX;
        windowY = targetY;
        windowWidth = targetWidth;
        windowHeight = targetHeight;
    }
    else
    {
#define LERP(start, end, pos) (end * pos + start * (1 - pos))
        const float x = LERP(oldWindowX, targetX, windowPos);
        const float y = LERP(oldWindowY, targetY, windowPos);
        const float w = LERP(oldWindowWidth, targetWidth, windowPos);
        const float h = LERP(oldWindowHeight, targetHeight, windowPos);
        windowX = ROUND(x);
        windowY = ROUND(y);
        windowWidth  = ROUND(w);
        windowHeight = ROUND(h);
#undef LERP
    }
}

void R_GetSmoothedViewWindow(int* x, int* y, int* w, int* h)
{
    if(x) *x = windowX;
    if(y) *y = windowY;
    if(w) *w = windowWidth;
    if(h) *h = windowHeight;
}

boolean R_ViewWindowFillsPort(void)
{
    return (windowWidth >= 320 && windowHeight >= 200 && windowX <= 0 && windowY <= 0);
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
