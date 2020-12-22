/**\file r_common.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#include "g_common.h"
#include "g_controls.h"
#include "hu_stuff.h"
#include "p_actor.h"
#include "player.h"
#include "r_common.h"
#include "r_special.h"
#include "x_hair.h"

Size2Rawf viewScale = { 1, 1 };
float aspectScale = 1;

static int gammaLevel;
#ifndef __JHEXEN__
char gammamsg[5][81];
#endif

void R_PrecachePSprites(void)
{
    int i, k;

    if(IS_DEDICATED)
        return;

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        for(k = 0; k < NUMWEAPLEVELS; ++k)
        {
            int pclass = players[CONSOLEPLAYER].class_;

            Models_CacheForState(weaponInfo[i][pclass].mode[k].states[WSN_UP]);
            Models_CacheForState(weaponInfo[i][pclass].mode[k].states[WSN_DOWN]);
            Models_CacheForState(weaponInfo[i][pclass].mode[k].states[WSN_READY]);
            Models_CacheForState(weaponInfo[i][pclass].mode[k].states[WSN_ATTACK]);
            Models_CacheForState(weaponInfo[i][pclass].mode[k].states[WSN_FLASH]);
#if __JHERETIC__ || __JHEXEN__
            Models_CacheForState(weaponInfo[i][pclass].mode[k].states[WSN_ATTACK_HOLD]);
#endif
        }
    }
}

/// @return  @c true= maximized view window is in effect.
static dd_bool maximizedViewWindow(int player)
{
    player_t* plr = players + player;
    if(player < 0 || player >= MAXPLAYERS)
    {
        Con_Error("maximizedViewWindow: Invalid player #%i.", player);
        exit(1);
    }
    return (!(G_GameState() == GS_MAP && cfg.common.screenBlocks <= 10 &&
              !(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK)))); // $democam: can be set on every game tic.
}

static void calcStatusBarSize(Size2Raw *size, Size2Rawf const *viewScale, int maxWidth)
{
    /**
     * @todo Refactor: This information should be queried from the status bar widget. -jk
     */
#if __JDOOM__ || __JHERETIC__ || __JHEXEN__
    float factor = 1;

    float const VGA_ASPECT  = 1.f/1.2f;
    float const aspectRatio = viewScale->width / viewScale->height;

    if(aspectRatio < VGA_ASPECT)
    {
        // We're below the VGA aspect, which means the status bar will be
        // scaled smaller.
        factor *= aspectRatio / VGA_ASPECT;
    }

    factor *= cfg.common.statusbarScale;

    size->width  =      ST_WIDTH  * factor;
    size->height = ceil(ST_HEIGHT * factor);
#else
    size->width = size->height = 0;
#endif
}

void R_StatusBarSize(int player, Size2Raw *statusBarSize)
{
    Size2Raw viewSize;
    R_ViewWindowSize(player, &viewSize);
    calcStatusBarSize(statusBarSize, &viewScale, viewSize.width);
}

static void resizeViewWindow(int player, const RectRaw* newGeometry,
                             const RectRaw* oldGeometry, dd_bool interpolate)
{
    RectRaw window;

    DE_ASSERT(newGeometry);
    DE_ASSERT(player >= 0 && player < MAXPLAYERS);

    DE_UNUSED(oldGeometry);

    // Calculate fixed 320x200 scale factors.
    viewScale.width  = (float)newGeometry->size.width  / SCREENWIDTH;
    viewScale.height = (float)newGeometry->size.height / SCREENHEIGHT;
    aspectScale = newGeometry->size.width >= newGeometry->size.height? viewScale.width : viewScale.height;

    // Determine view window geometry.
    memcpy(&window, newGeometry, sizeof(window));
    window.origin.x = window.origin.y = 0;

    // Override @c cfg.common.screenBlocks and force a maximized window?
    if(!maximizedViewWindow(player) && cfg.common.screenBlocks <= 10)
    {
        Size2Raw statusBarSize;
        calcStatusBarSize(&statusBarSize, &viewScale, newGeometry->size.width);

        if(cfg.common.screenBlocks != 10)
        {
            window.size.width  = cfg.common.screenBlocks * SCREENWIDTH/10;
            window.size.height = cfg.common.screenBlocks * (SCREENHEIGHT - statusBarSize.height) / 10;

            window.origin.x = (SCREENWIDTH - window.size.width) / 2;
            window.origin.y = (SCREENHEIGHT - statusBarSize.height - window.size.height) / 2;
        }
        else
        {
            window.origin.x = 0;
            window.origin.y = 0;
            window.size.width  = SCREENWIDTH;
            window.size.height = SCREENHEIGHT - statusBarSize.height;
        }

        // Scale from fixed to viewport coordinates.
        window.origin.x = ROUND(window.origin.x * viewScale.width);
        window.origin.y = ROUND(window.origin.y * viewScale.height);
        window.size.width  = ROUND(window.size.width  * viewScale.width);
        window.size.height = ROUND(window.size.height * viewScale.height);
    }

    R_SetViewWindowGeometry(player, &window, interpolate);
}

void R_ResizeViewWindow(int flags)
{
    static dd_bool oldMaximized;
    int i, delta, destBlocks = MINMAX_OF(3, cfg.common.setBlocks, 13);
    dd_bool maximized;
    RectRaw port;

    if(IS_DEDICATED) return;

    // Override @c cfg.common.screenBlocks and force a maximized window?
    maximized = maximizedViewWindow(DISPLAYPLAYER);
    if(maximized != oldMaximized)
    {
        oldMaximized = maximized;
        flags |= RWF_FORCE|RWF_NO_LERP;
    }

    if(!(flags & RWF_FORCE))
    {
        if(cfg.common.screenBlocks == destBlocks)
            return;
    }

    delta = MINMAX_OF(-1, destBlocks - cfg.common.screenBlocks, 1);
    if(delta != 0)
    {
        if(cfg.common.screenBlocks >= 10 && destBlocks != 13)
        {
            // When going fullscreen, force a hud show event (to reset the timer).
            for(i = 0; i < MAXPLAYERS; ++i)
                ST_HUDUnHide(i, HUE_FORCE);
        }

        if((cfg.common.screenBlocks == 11 && destBlocks == 10) ||
           (cfg.common.screenBlocks == 10 && destBlocks == 11))
        {
            // When going to/from statusbar span, do an instant change.
            flags |= RWF_NO_LERP;
        }

        cfg.common.screenBlocks += delta;
        flags |= RWF_FORCE;
    }

    // No update necessary?
    if(!(flags & RWF_FORCE)) return;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        if(!R_ViewPortGeometry(i, &port))
        {
            // Player is not local or does not have a viewport.
            continue;
        }
        resizeViewWindow(i, &port, &port, (flags & RWF_NO_LERP)==0);
    }
}

int R_UpdateViewport(int hookType, int param, void* data)
{
    const ddhook_viewport_reshape_t* p = (ddhook_viewport_reshape_t*)data;
    resizeViewWindow(param, &p->geometry, &p->oldGeometry, false);
    return true;
}

#ifndef __JHEXEN__
void R_GetGammaMessageStrings(void)
{
    int i;
    for(i = 0; i < 5; ++i)
    {
        strcpy(gammamsg[i], GET_TXT(TXT_GAMMALVL0 + i));
    }
}
#endif

void R_CycleGammaLevel(void)
{
    char buf[50];

    if(G_QuitInProgress()) return;

    gammaLevel++;
    if(gammaLevel > 4)
        gammaLevel = 0;

#if __JDOOM__ || __JDOOM64__
    P_SetMessageWithFlags(&players[CONSOLEPLAYER], gammamsg[gammaLevel], LMF_NO_HIDE);
#endif

    sprintf(buf, "rend-tex-gamma %f", ((float) gammaLevel / 8.0f) * 1.5f);
    DD_Execute(false, buf);
}

/**
 * Tells the engine where the camera is located. This has to be called before
 * the end of G_Ticker() (after thinkers have been run), so that the up-to-date
 * sharp camera position and angles are available when the new sharp world is
 * saved.
 *
 * @param player  Player # to update.
 */
void R_UpdateConsoleView(int player)
{
    coord_t viewOrigin[3];
    player_t* plr;
    mobj_t* mo;

    if(IS_DEDICATED || player < 0 || player >= MAXPLAYERS) return;
    plr = &players[player];
    mo = plr->plr->mo;
    if(!mo || !plr->plr->inGame) return; // Not present?

    viewOrigin[VX] = mo->origin[VX] + plr->viewOffset[VX];
    viewOrigin[VY] = mo->origin[VY] + plr->viewOffset[VY];
    viewOrigin[VZ] = plr->viewZ + plr->viewOffset[VZ];
    R_SetViewOrigin(player, viewOrigin);
    R_SetViewAngle(player, Player_ViewYawAngle(player));
    R_SetViewPitch(player, plr->plr->lookDir);
}

static void rendHUD(int player, const RectRaw* portGeometry)
{
    if(player < 0 || player >= MAXPLAYERS) return;
    if(G_GameState() != GS_MAP) return;
    if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME))) return;
    if(!DD_GetInteger(DD_GAME_DRAW_HUD_HINT)) return; // The engine advises not to draw any HUD displays.

    ST_Drawer(player);
    HU_DrawScoreBoard(player);
    Hu_MapTitleDrawer(portGeometry);
}

void G_DrawViewPort(int port, RectRaw const *portGeometry,
                    RectRaw const *windowGeometry, int player, int layer)
{
    switch (G_GameState())
    {
    case GS_MAP: {
        player_t* plr = players + player;
        dd_bool isAutomapObscuring = ST_AutomapObscures2(player, windowGeometry);

        if (IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
            return;

        if (cfg.common.automapNeverObscure || Con_GetInteger("rend-vr-mode") == 9) // Oculus Rift mode
        {
            // Automap will not cover the full view.
            isAutomapObscuring = false;
        }

        switch (layer)
        {
        case 0: // Primary layer (3D view).
            if (!isAutomapObscuring)
            {
                G_RendPlayerView(player);
#if defined(__JDOOM64__)
                G_RendSpecialFilter(player, windowGeometry);
#endif
            }
            break;

        default: // HUD layer.
            // Crosshair.
            if (!isAutomapObscuring && !(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))) // $democam
            {
                X_Drawer(player);
            }

            // Other HUD elements.
            rendHUD(player, portGeometry);
            break;
        }
        break; }

    case GS_STARTUP:
        if (layer == 0)
        {
            DGL_DrawRectf2Color(0, 0, portGeometry->size.width, portGeometry->size.height,
                                0, 0, 0, 1);
        }
        break;

    default:
        break;
    }
}

void G_ResetViewEffects()
{
    GL_ResetViewEffects();
    R_InitSpecialFilter();
}
