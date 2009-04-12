/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * h_refresh.c: - jHeretic specific.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#include "jheretic.h"

#include "hu_stuff.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "hu_pspr.h"
#include "am_map.h"
#include "g_common.h"
#include "r_common.h"
#include "d_net.h"
#include "f_infine.h"
#include "x_hair.h"
#include "g_controls.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "rend_automap.h"

// MACROS ------------------------------------------------------------------

#define WINDOWHEIGHT         Get(DD_VIEWWINDOW_HEIGHT)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void R_SetAllDoomsdayFlags(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern const float defFontRGB[];
extern const float defFontRGB2[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Creates the translation tables to map the green color ramp to gray,
 * brown, red.
 *
 * \note Assumes a given structure of the PLAYPAL. Could be read from a
 * lump instead.
 */
static void initTranslation(void)
{
    int                 i;
    byte*               translationtables =
        (byte*) DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);

    // Fill out the translation tables.
    for(i = 0; i < 256; ++i)
    {
        if(i >= 225 && i <= 240)
        {
            translationtables[i] = 114 + (i - 225); // yellow
            translationtables[i + 256] = 145 + (i - 225); // red
            translationtables[i + 512] = 190 + (i - 225); // blue
        }
        else
        {
            translationtables[i] = translationtables[i + 256] =
                translationtables[i + 512] = i;
        }
    }
}

void R_InitRefresh(void)
{
    initTranslation();
}

/**
 * Draws a special filter over the screen.
 */
void R_DrawSpecialFilter(int pnum)
{
    float               x, y, w, h;
    player_t*           player = &players[pnum];

    if(player->powers[PT_INVULNERABILITY] <= BLINKTHRESHOLD &&
       !(player->powers[PT_INVULNERABILITY] & 8))
        return;

    R_GetViewWindow(&x, &y, &w, &h);
    DGL_Disable(DGL_TEXTURING);
    if(PLRPROFILE.screen.ringFilter == 1)
    {
        DGL_BlendFunc(DGL_SRC_COLOR, DGL_SRC_COLOR);
        DGL_DrawRect(x, y, w, h, .5f, .35f, .1f, 1);
    }
    else
    {
        DGL_BlendFunc(DGL_DST_COLOR, DGL_SRC_COLOR);
        DGL_DrawRect(x, y, w, h, 0, 0, .6f, 1);
    }

    // Restore the normal rendering state.
    DGL_BlendMode(BM_NORMAL);
    DGL_Enable(DGL_TEXTURING);
}

boolean R_GetFilterColor(float rgba[4], int filter)
{
    if(!rgba)
        return false;

    // We have to choose the right color and alpha.
    if(filter >= STARTREDPALS && filter < STARTREDPALS + NUMREDPALS)
    {   // Red.
        rgba[CR] = 1;
        rgba[CG] = 0;
        rgba[CB] = 0;
        rgba[CA] = filter / 8.f; // Full red with filter 8.
        return true;
    }
    else if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
    {   // Light Yellow.
        rgba[CR] = 1;
        rgba[CG] = 1;
        rgba[CB] = .5f;
        rgba[CA] = (filter - STARTBONUSPALS + 1) / 16.f;
        return true;
    }

    if(filter)
        Con_Message("R_GetFilterColor: Real strange filter number: %d.\n", filter);

    return false;
}

void R_DrawMapTitle(int x, int y, float alpha, dpatch_t* font,
                      boolean center)
{
    int                 strX;
    char*               lname, *lauthor;

    lname = P_GetMapNiceName();
    if(lname)
    {
        strX = x;
        if(center)
            strX -= M_StringWidth(lname, font) / 2;

        M_WriteText3(strX, y, lname, font,
                     defFontRGB[0], defFontRGB[1], defFontRGB[2], alpha,
                     false, 0);
        y += 20;
    }

    lauthor = (char *) DD_GetVariable(DD_MAP_AUTHOR);
    if(lauthor && stricmp(lauthor, "raven software"))
    {
        strX = x;
        if(center)
            strX -= M_StringWidth(lauthor, huFontA) / 2;

        M_WriteText3(strX, y, lauthor, huFontA, .5f, .5f, .5f, alpha,
                     false, 0);
    }
}

/**
 * Do not really change anything here, because Doomsday might be in the
 * middle of a refresh. The change will take effect next refresh.
 */
void R_SetViewSize(int player, int blocks)
{
    if(PLRPROFILE.screen.setBlocks != blocks && blocks > 10 && blocks < 13)
    {   // Going to/from fullscreen. Force a hud show event (to reset the timer).
        ST_HUDUnHide(player, HUE_FORCE);
    }

    PLRPROFILE.screen.setBlocks = blocks;
}

static void rendPlayerView(int player)
{
    player_t*           plr = &players[player];
    int                 viewAngleOffset =
        ANGLE_MAX * -G_GetLookOffset(player);
    boolean             isFullBright =
        (plr->powers[PT_INVULNERABILITY] > BLINKTHRESHOLD) ||
                       (plr->powers[PT_INVULNERABILITY] & 8);

    if(IS_CLIENT)
    {
        // Server updates mobj flags in NetSv_Ticker.
        R_SetAllDoomsdayFlags();
    }

    DD_SetVariable(DD_VIEWX_OFFSET, &plr->viewOffset[VX]);
    DD_SetVariable(DD_VIEWY_OFFSET, &plr->viewOffset[VY]);
    DD_SetVariable(DD_VIEWZ_OFFSET, &plr->viewOffset[VZ]);
    // The view angle offset.
    DD_SetVariable(DD_VIEWANGLE_OFFSET, &viewAngleOffset);

    // $democam
    GL_SetFilter((plr->plr->flags & DDPF_VIEW_FILTER)? true : false);
    if(plr->plr->flags & DDPF_VIEW_FILTER)
    {
        const float*        color = plr->plr->filterColor;
        GL_SetFilterColor(color[CR], color[CG], color[CB], color[CA]);
    }

    // How about fullbright?
    DD_SetInteger(DD_FULLBRIGHT, isFullBright);

    // Render the view with possible custom filters.
    R_RenderPlayerView(player);
}

static void rendHUD(int player)
{
    player_t*           plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    if(G_GetGameState() != GS_MAP)
        return;

    if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
        return;

    plr = &players[player];

    // These various HUD's will be drawn unless Doomsday advises not to
    if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
    {
        automapid_t         map = AM_MapForPlayer(player);
        boolean             redrawsbar = false;

        if((WINDOWHEIGHT != 200))
            redrawsbar = true;

        if(!(IS_NETGAME && deathmatch))
            HU_DrawCheatCounters();

        // Do we need to render a full status bar at this point?
        if(!(AM_IsActive(map) && PLRPROFILE.automap.hudDisplay == 0) &&
           !(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK)))
        {
            int         viewmode =
                ((WINDOWHEIGHT == 200)? (PLRPROFILE.screen.setBlocks - 10) : 0);

            ST_Drawer(player, viewmode, redrawsbar); // $democam
        }

        HU_Drawer(player);
    }
}

/**
 * Draws the in-viewport display.
 *
 * @param layer         @c 0 = bottom layer (before the viewport border).
 *                      @c 1 = top layer (after the viewport border).
 */
void H_Display(int layer)
{
    int                 player = DISPLAYPLAYER;
    player_t*           plr = &players[player];
    float               x, y, w, h;

    if(layer == 0)
    {
        // $democam: can be set on every frame.
        if(PLRPROFILE.screen.setBlocks > 10 || (P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK)))
        {
            // Full screen.
            R_SetViewWindowTarget(0, 0, 320, 200);
        }
        else
        {
            int                 w = PLRPROFILE.screen.setBlocks * 32;
            int                 h = PLRPROFILE.screen.setBlocks *
                (200 - SBARHEIGHT * PLRPROFILE.statusbar.scale / 20) / 10;

            R_SetViewWindowTarget(160 - (w / 2),
                                  (200 - SBARHEIGHT * PLRPROFILE.statusbar.scale / 20 - h) / 2,
                                  w, h);
        }

        R_GetViewWindow(&x, &y, &w, &h);
        R_SetViewWindow((int) x, (int) y, (int) w, (int) h);

        if(!(MN_CurrentMenuHasBackground() && Hu_MenuAlpha() >= 1) &&
           !R_MapObscures(player, (int) x, (int) y, (int) w, (int) h))
        {
            if(G_GetGameState() != GS_MAP)
                return;

            if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
                return;

            if(!IS_CLIENT && mapTime < 2)
            {
                // Don't render too early; the first couple of frames
                // might be a bit unstable -- this should be considered
                // a bug, but since there's an easy fix...
                return;
            }

            rendPlayerView(player);

            R_DrawSpecialFilter(player);

            // Crosshair.
            if(!(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))) // $democam
                X_Drawer(player);
        }

        // Draw the automap?
        AM_Drawer(player);
    }
    else if(layer == 1)
    {
        rendHUD(player);
    }
}

void H_Display2(void)
{
    switch(G_GetGameState())
    {
    case GS_MAP:
        if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
            break;

        if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
        {
            // Level information is shown for a few seconds in the
            // beginning of a level.
            if(gs.cfg.mapTitle || actualMapTime <= 6 * TICSPERSEC)
            {
                int         x, y;
                float       alpha = 1;

                if(actualMapTime < 35)
                    alpha = actualMapTime / 35.0f;
                if(actualMapTime > 5 * 35)
                    alpha = 1 - (actualMapTime - 5 * 35) / 35.0f;

                x = SCREENWIDTH / 2;
                y = 13;
                Draw_BeginZoom((1 + PLRPROFILE.hud.scale)/2, x, y);
                R_DrawMapTitle(x, y, alpha, huFontB, true);
                Draw_EndZoom();
            }
        }
        break;

    case GS_INTERMISSION:
        IN_Drawer();
        break;

    case GS_WAITING:
        // Clear the screen while waiting, doesn't mess up the menu.
        //gl.Clear(DGL_COLOR_BUFFER_BIT);
        break;

    case GS_INFINE:
        if(!fiCmdExecuted)
        {   // A (de)briefing is in process but the script hasn't started yet.
            // Just clear the screen, then.
            DGL_Disable(DGL_TEXTURING);
            DGL_DrawRect(0, 0, 320, 200, 0, 0, 0, 1);
            DGL_Enable(DGL_TEXTURING);
        }
        break;

    default:
        break;
    }

    // Draw pause pic (but not if InFine active).
    if(paused && !fiActive)
    {
        GL_DrawPatch(SCREENWIDTH/2, 4, W_GetNumForName("PAUSED"));
    }

    // InFine is drawn whenever active.
    FI_Drawer();

    // Draw HUD displays; menu, messages.
    Hu_Drawer();
}

/**
 * Updates the mobj flags used by Doomsday with the state of our local flags
 * for the given mobj.
 */
void R_SetDoomsdayFlags(mobj_t* mo)
{
    // Client mobjs can't be set here.
    if(IS_CLIENT && mo->ddFlags & DDMF_REMOTE)
        return;

    // Reset the flags for a new frame.
    mo->ddFlags &= DDMF_CLEAR_MASK;

    // Local objects aren't sent to clients.
    if(mo->flags & MF_LOCAL)
        mo->ddFlags |= DDMF_LOCAL;
    if(mo->flags & MF_SOLID)
        mo->ddFlags |= DDMF_SOLID;
    if(mo->flags & MF_NOGRAVITY)
        mo->ddFlags |= DDMF_NOGRAVITY;
    if(mo->flags2 & MF2_FLOATBOB)
        mo->ddFlags |= DDMF_NOGRAVITY | DDMF_BOB;
    if(mo->flags & MF_MISSILE)
    {   // Mace death balls are controlled by the server.
        //if(mo->type != MT_MACEFX4)
        mo->ddFlags |= DDMF_MISSILE;
    }
    if(mo->info && (mo->info->flags2 & MF2_ALWAYSLIT))
        mo->ddFlags |= DDMF_ALWAYSLIT;

    if(mo->flags2 & MF2_FLY)
        mo->ddFlags |= DDMF_FLY | DDMF_NOGRAVITY;

    // $democam: cameramen are invisible.
    if(P_MobjIsCamera(mo))
        mo->ddFlags |= DDMF_DONTDRAW;

    if((mo->flags & MF_CORPSE) && PLRPROFILE.corpseTime && mo->corpseTics == -1)
        mo->ddFlags |= DDMF_DONTDRAW;

    // Choose which ddflags to set.
    if(mo->flags2 & MF2_DONTDRAW)
    {
        mo->ddFlags |= DDMF_DONTDRAW;
        return; // No point in checking the other flags.
    }

    if(mo->flags2 & MF2_LOGRAV)
        mo->ddFlags |= DDMF_LOWGRAVITY;

    if(mo->flags & MF_BRIGHTSHADOW)
        mo->ddFlags |= DDMF_BRIGHTSHADOW;
    else if(mo->flags & MF_SHADOW)
        mo->ddFlags |= DDMF_ALTSHADOW;

    if(((mo->flags & MF_VIEWALIGN) && !(mo->flags & MF_MISSILE)) ||
       (mo->flags & MF_FLOAT) ||
       ((mo->flags & MF_MISSILE) && !(mo->flags & MF_VIEWALIGN)))
        mo->ddFlags |= DDMF_VIEWALIGN;

    mo->ddFlags |= (mo->flags & MF_TRANSLATION);
}

/**
 * Updates the status flags for all visible things.
 */
void R_SetAllDoomsdayFlags(void)
{
    uint                i;
    mobj_t*             iter;

    // Only visible things are in the sector thinglists, so this is good.
    for(i = 0; i < numsectors; ++i)
    {
        iter = P_GetPtr(DMU_SECTOR, i, DMT_MOBJS);

        while(iter)
        {
            R_SetDoomsdayFlags(iter);
            iter = iter->sNext;
        }
    }
}
