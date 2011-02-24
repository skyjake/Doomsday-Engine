/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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
 * r_common.c : Common routines for refresh.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
# include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "am_map.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// View window current position.
static float windowX = 0, oldWindowX = 0;
static float windowY = 0, oldWindowY = 0;
static float windowWidth = SCREENWIDTH, oldWindowWidth = SCREENWIDTH;
static float windowHeight = SCREENHEIGHT, oldWindowHeight = SCREENHEIGHT;
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

/**
 * Animates the game view window towards the target values.
 */
void R_ViewWindowTicker(void)
{
    if(targetX == -1)
        return; // Nothing to do.

    windowPos += .4f;
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
        windowX = LERP(oldWindowX, targetX, windowPos);
        windowY = LERP(oldWindowY, targetY, windowPos);
        windowWidth = LERP(oldWindowWidth, targetWidth, windowPos);
        windowHeight = LERP(oldWindowHeight, targetHeight, windowPos);
#undef LERP
    }
}

void R_GetViewWindow(float* x, float* y, float* w, float* h)
{
    if(x) *x = windowX;
    if(y) *y = windowY;
    if(w) *w = windowWidth;
    if(h) *h = windowHeight;
}

boolean R_IsFullScreenViewWindow(void)
{
    return (windowWidth >= 320 && windowHeight >= 200 && windowX <= 0 &&
            windowY <= 0);
}

/**
 * Does the given display player's automap obscure the window completely?
 * \note: Window dimensions are in fixed scale {x} 0 - 320, {y} 0 - 200.
 *
 * @param playerid      Index of the player whose map to check.
 * @param x             Top left X coordinate.
 * @param y             Top left Y coordinate.
 * @param w             Width.
 * @param h             Height.
 *
 * @return              @true if there is no point within the given window
 *                      which is even partially visible.
 */
boolean R_MapObscures(int playerid, int x, int y, int w, int h)
{
    boolean             retVal = false;
    automapid_t         map = AM_MapForPlayer(DISPLAYPLAYER);

    if(AM_IsActive(map))
    {
        float   alpha;

        AM_GetColorAndAlpha(map, AMO_BACKGROUND, NULL, NULL, NULL, &alpha);
        if(!(alpha < 1) && !(AM_GlobalAlpha(map) < 1))
        {
            if(AM_IsMapWindowInFullScreenMode(map))
            {
                retVal = true;
            }
            else
            {
                // We'll have to compare the dimensions.
                float       scrwidth = Get(DD_WINDOW_WIDTH);
                float       scrheight = Get(DD_WINDOW_HEIGHT);
                float       fx = FIXXTOSCREENX(x);
                float       fy = FIXYTOSCREENY(x);
                float       fw = FIXXTOSCREENX(w);
                float       fh = FIXYTOSCREENY(h);
                float       mx, my, mw, mh;

                AM_GetWindow(map, &mx, &my, &mw, &mh);
                if(mx >= fx && my >= fy && mw >= fw && mh >= fh)
                    retVal = true;
            }
        }
    }

    return retVal;
}

void R_CachePatch(dpatch_t* dp, const char* name)
{
    lumppatch_t*        patch;

    if(IS_DEDICATED)
        return;

    dp->lump = W_CheckNumForName(name);
    if(dp->lump == -1)
        return;

    patch = (lumppatch_t *) W_CacheLumpNum(dp->lump, PU_CACHE);
    dp->width = SHORT(patch->width);
    dp->height = SHORT(patch->height);
    dp->leftOffset = SHORT(patch->leftOffset);
    dp->topOffset = SHORT(patch->topOffset);

    // Precache the patch while we're at it.
    R_PrecachePatch(dp->lump);
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
    char                buf[50];

    gammaLevel++;
    if(gammaLevel > 4)
        gammaLevel = 0;

#if __JDOOM__ || __JDOOM64__
    P_SetMessage(players + CONSOLEPLAYER, gammamsg[gammaLevel], true);
#endif

    sprintf(buf, "rend-tex-gamma %f", ((float) gammaLevel / 8.0f) * 1.5f);
    DD_Execute(false, buf);
}
