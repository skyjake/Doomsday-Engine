/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * d_refresh.c
 */

// HEADER FILES ------------------------------------------------------------

#include "wolftc.h"

#include "hu_stuff.h"
#include "hu_pspr.h"
#include "am_map.h"
#include "g_common.h"
#include "r_common.h"
#include "d_net.h"
#include "f_infine.h"
#include "x_hair.h"
#include "g_controls.h"
#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

#define WINDOWHEIGHT  Get(DD_VIEWWINDOW_HEIGHT)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int setdetail;

// CODE --------------------------------------------------------------------

/**
 * Creates the translation tables to map the green color ramp to gray,
 * brown, red.
 * NOTE: Assumes a given structure of the PLAYPAL.
 *       Could be read from a lump instead.
 */
static void initTranslation(void)
{
    byte       *translationtables = (byte *)
                    DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);
    int         i;

    // translate just the 16 green colors
    for(i = 0; i < 256; ++i)
    {
        if(i >= 0x70 && i <= 0x7f)
        {
            // map green ramp to gray, brown, red
            translationtables[i] = 0x60 + (i & 0xf);
            translationtables[i + 256] = 0x40 + (i & 0xf);
            translationtables[i + 512] = 0x20 + (i & 0xf);
        }
        else
        {
            // Keep all other colors as is.
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
 * Draws a special filter over the screen (eg the inversing filter used when
 * in god mode).
 */
void R_DrawSpecialFilter(void)
{
    player_t       *player = &players[DISPLAYPLAYER];

    if(player->powers[PT_INVULNERABILITY])
    {
        float           x, y, w, h;
        float           max = 30;
        float           str, r, g, b;
        int             t = player->powers[PT_INVULNERABILITY];

        if(t < max)
            str = t / max;
        else if(t < 4 * 32 && !(t & 8))
            str = .7f;
        else if(t > INVULNTICS - max)
            str = (INVULNTICS - t) / max;
        else
            str = 1; // Full inversion.

        // Draw an inversing filter.
        DGL_Disable(DGL_TEXTURING);
        GL_BlendMode(BM_INVERSE);

        r = MINMAX_OF(0.f, str * 2, 1.f);
        g = MINMAX_OF(0.f, str * 2 - .4, 1.f);
        b = MINMAX_OF(0.f, str * 2 - .8, 1.f);

        R_GetViewWindow(&x, &y, &w, &h);
        GL_DrawRect(x, y, w, h, r, g, b, 1);

        // Restore the normal rendering state.
        GL_BlendMode(BM_NORMAL);
        DGL_Enable(DGL_TEXTURING);
    }
}

/**
 * Show map name and author.
 */
void R_DrawLevelTitle(void)
{
    float       alpha = 1;
    int         y = 12;
    int         mapnum;
    char       *lname, *lauthor;

    if(!cfg.levelTitle || actualLevelTime > 6 * 35)
        return;

    // Make the text a bit smaller.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_Translatef(160, y, 0);
    DGL_Scalef(.7f, .7f, 1);
    DGL_Translatef(-160, -y, 0);

    if(actualLevelTime < 35)
        alpha = actualLevelTime / 35.0f;
    if(actualLevelTime > 5 * 35)
        alpha = 1 - (actualLevelTime - 5 * 35) / 35.0f;

    // Get the strings from Doomsday
    lname = P_GetMapNiceName();
    lauthor = (char *) DD_GetVariable(DD_MAP_AUTHOR);

    // Compose the mapnumber used to check the map name patches array.
    if(gamemode == commercial)
        mapnum = gamemap -1;
    else
        mapnum = ((gameepisode -1) * 9) + gamemap -1;

    if(lname)
    {
        WI_DrawPatch(SCREENWIDTH / 2, y, 1, 1, 1, alpha, levelNamePatches[mapnum].lump,
                     lname, false, ALIGN_CENTER);
        y += 14;                //9;
    }

    //DGL_Color4f(.5f, .5f, .5f, alpha);
    if(lauthor && W_IsFromIWAD(levelNamePatches[mapnum].lump) &&
       (!cfg.hideAuthorIdSoft || stricmp(lauthor, "id software")))
    {
        M_WriteText3(160 - M_StringWidth(lauthor, huFontA) / 2, y, lauthor,
                     huFontA, .5f, .5f, .5f, alpha, false, 0);
    }

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

/**
 * Do not really change anything here, because Doomsday might be in the
 * middle of a refresh. The change will take effect next refresh.
 */
void R_SetViewSize(int blocks, int detail)
{
    cfg.setSizeNeeded = true;
    if(cfg.setBlocks != blocks && blocks > 10 && blocks < 13)
    {   // When going fullscreen, force a hud show event (to reset the timer).
        ST_HUDUnHide(HUE_FORCE);
    }
    cfg.setBlocks = blocks;
    setdetail = detail;
}

void D_Display(void)
{
    static boolean viewactivestate = false;
    static boolean menuactivestate = false;
    static gamestate_t oldgamestate = -1;
    player_t   *player = &players[DISPLAYPLAYER];
    boolean     iscam = (player->plr->flags & DDPF_CAMERA) != 0; // $democam
    float       x, y, w, h;
    boolean     mapHidesView;

    // $democam: can be set on every frame
    if(cfg.setBlocks > 10 || iscam)
    {
        // Full screen.
        R_SetViewWindowTarget(0, 0, 320, 200);
    }
    else
    {
        int w = cfg.setBlocks * 32;
        int h = cfg.setBlocks * (200 - ST_HEIGHT * cfg.statusbarScale / 20) / 10;
        R_SetViewWindowTarget(160 - (w >> 1),
                              (200 - ST_HEIGHT * cfg.statusbarScale / 20 - h) >> 1,
                              w, h);
    }

    R_GetViewWindow(&x, &y, &w, &h);
    R_ViewWindow((int) x, (int) y, (int) w, (int) h);

    // Do buffered drawing.
    switch(G_GetGameState())
    {
    case GS_LEVEL:
        if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
            break;
        if(!IS_CLIENT && leveltime < 2)
        {
            // Don't render too early; the first couple of frames
            // might be a bit unstable -- this should be considered
            // a bug, but since there's an easy fix...
            break;
        }

        mapHidesView =
            R_MapObscures(DISPLAYPLAYER, (int) x, (int) y, (int) w, (int) h);

        if(!(MN_CurrentMenuHasBackground() && Hu_MenuAlpha() >= 1) &&
           !mapHidesView)
        {
            int viewAngleOffset = ANGLE_MAX * -G_GetLookOffset(DISPLAYPLAYER);

            // Draw the player view.
            if(IS_CLIENT)
            {
                // Server updates mobj flags in NetSv_Ticker.
                R_SetAllDoomsdayFlags();
            }
            // The view angle offset.
            DD_SetVariable(DD_VIEWANGLE_OFFSET, &viewAngleOffset);
            GL_SetFilter(players[DISPLAYPLAYER].plr->filter);   // $democam

            // How about fullbright?
            Set(DD_FULLBRIGHT, (player->powers[PT_INFRARED] > 4 * 32) ||
                (player->powers[PT_INFRARED] & 8) ||
                player->powers[PT_INVULNERABILITY] > 30);

            // Render the view with possible custom filters.
            R_RenderPlayerView(DISPLAYPLAYER);

            R_DrawSpecialFilter();
            // Crosshair.
            if(!iscam)
                X_Drawer();     // $democam
        }

        // Draw the automap?
        AM_Drawer(DISPLAYPLAYER);
        break;

    default:
        break;
    }

    menuactivestate = Hu_MenuIsActive();
    viewactivestate = viewActive;
    oldgamestate = wipegamestate = G_GetGameState();
}

void D_Display2(void)
{
    // Do buffered drawing.
    switch(G_GetGameState())
    {
    case GS_LEVEL:
        if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
            break;

        if(!IS_CLIENT && leveltime < 2)
        {
            // Don't render too early; the first couple of frames
            // might be a bit unstable -- this should be considered
            // a bug, but since there's an easy fix...
            break;
        }

        // These various HUD's will be drawn unless Doomsday advises not to
        if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
        {
            boolean     redrawsbar = false;

            // Draw HUD displays only visible when the automap is open.
            if(AM_IsMapActive(DISPLAYPLAYER))
                HU_DrawMapCounters();

            // Level information is shown for a few seconds in the
            // beginning of a level.
            R_DrawLevelTitle();

            if(WINDOWHEIGHT != 200)
                redrawsbar = true;

            // Do we need to render a full status bar at this point?
            if(!(AM_IsMapActive(DISPLAYPLAYER) && cfg.automapHudDisplay == 0 ))
            {
                player_t   *player = &players[DISPLAYPLAYER];
                boolean     iscam = (player->plr->flags & DDPF_CAMERA) != 0; // $democam

                if(!iscam)
                {
                    if(WINDOWHEIGHT == 200)
                    {
                        // Fullscreen. Which mode?
                        ST_Drawer(cfg.setBlocks - 10 , redrawsbar);
                    }
                    else
                    {
                        ST_Drawer(0 , redrawsbar);
                    }
                }
            }
            HU_Drawer();
        }
        break;

    case GS_INTERMISSION:
        WI_Drawer();
        break;

    case GS_WAITING:
        //gl.Clear(DGL_COLOR_BUFFER_BIT);
        //M_WriteText2(5, 188, "WAITING... PRESS ESC FOR MENU", huFontA, 1, 0, 0, 1);
        break;

    default:
        break;
    }

    // draw pause pic (but not if InFine active)
    if(paused && !fi_active)
    {
        WI_DrawPatch(SCREENWIDTH /2, 4, 1, 1, 1, 1, W_GetNumForName("M_PAUSE"),
                     NULL, false, ALIGN_CENTER);
    }

    // InFine is drawn whenever active.
    FI_Drawer();

    // The menu is drawn whenever active.
    Hu_MenuDrawer();
}

/**
 * Updates the mobj flags used by Doomsday with the state of our local flags
 * for the given mobj.
 */
void P_SetDoomsdayFlags(mobj_t *mo)
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
    {
        mo->ddFlags |= DDMF_MISSILE;
    }
    if(mo->type == MT_LIGHTSOURCE)
        mo->ddFlags |= DDMF_ALWAYSLIT | DDMF_DONTDRAW;
    if(mo->info && mo->info->flags2 & MF2_ALWAYSLIT)
        mo->ddFlags |= DDMF_ALWAYSLIT;

    if(mo->flags2 & MF2_FLY)
        mo->ddFlags |= DDMF_FLY | DDMF_NOGRAVITY;

    // $democam: cameramen are invisible
    if(P_IsCamera(mo))
        mo->ddFlags |= DDMF_DONTDRAW;

    if(mo->flags & MF_CORPSE && cfg.corpseTime && mo->corpseTics == -1)
        mo->ddFlags |= DDMF_DONTDRAW;

    // Choose which ddflags to set.
    if(mo->flags2 & MF2_DONTDRAW)
    {
        mo->ddFlags |= DDMF_DONTDRAW;
        return;                 // No point in checking the other flags.
    }

    if(mo->flags2 & MF2_LOGRAV)
        mo->ddFlags |= DDMF_LOWGRAVITY;

    // The torches often go into the ceiling. This'll prevent
    // them from 'jumping'.
    if(mo->type == MT_MISC41 || mo->type == MT_MISC42 || mo->type == MT_MISC43  // tall torches
       || mo->type == MT_MISC44 || mo->type == MT_MISC45 || mo->type == MT_MISC46)  // short torches
        mo->ddFlags |= DDMF_NOFITBOTTOM;

    if(mo->flags & MF_BRIGHTSHADOW)
        mo->ddFlags |= DDMF_BRIGHTSHADOW;
    else if(mo->flags & MF_SHADOW)
        mo->ddFlags |= DDMF_SHADOW;

    if((mo->flags & MF_VIEWALIGN && !(mo->flags & MF_MISSILE)) ||
       mo->flags & MF_FLOAT || (mo->flags & MF_MISSILE &&
                                !(mo->flags & MF_VIEWALIGN)))
        mo->ddFlags |= DDMF_VIEWALIGN;

    mo->ddFlags |= mo->flags & MF_TRANSLATION;
}

/**
 * Updates the status flags for all visible things.
 */
void R_SetAllDoomsdayFlags(void)
{
    uint        i;
    mobj_t     *iter;

    // Only visible things are in the sector thinglists, so this is good.
    for(i = 0; i < numsectors; ++i)
    {
        for(iter = P_GetPtr(DMU_SECTOR, i, DMT_MOBJS); iter; iter = iter->sNext)
            P_SetDoomsdayFlags(iter);
    }
}
