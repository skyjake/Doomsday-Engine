/* DE1: $Id$
 * Copyright (C) 2005 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * d_refresh.c
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>
#include "doomdef.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "wi_stuff.h"
#include "hu_stuff.h"
#include "hu_pspr.h"
#include "Common/am_map.h"
#include "d_config.h"
#include "x_hair.h"
#include "Mn_def.h"
#include "m_menu.h"
#include "g_common.h"
#include "r_common.h"
#include "d_net.h"
#include "f_infine.h"

#include "Common/x_hair.h"

// MACROS ------------------------------------------------------------------

#define viewheight  Get(DD_VIEWWINDOW_HEIGHT)
#define SIZEFACT 4
#define SIZEFACT2 16

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void    R_SetAllDoomsdayFlags();

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int actual_leveltime;

 // Name graphics of each level (centered)
extern dpatch_t *lnames;

extern boolean amap_fullyopen;

extern boolean inhelpscreens;
extern float lookOffset;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

gamestate_t wipegamestate = GS_DEMOSCREEN;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int setdetail;

// CODE --------------------------------------------------------------------

/*
 * Creates the translation tables to map
 * the green color ramp to gray, brown, red.
 * Assumes a given structure of the PLAYPAL.
 * Could be read from a lump instead.
 */
void R_InitTranslation(void)
{
    byte   *translationtables = (byte *)
        DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);
    int     i;

    // translate just the 16 green colors
    for(i = 0; i < 256; i++)
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

/*
 * Draws a special filter over the screen
 * (eg the inversing filter used when in god mode).
 */
void R_DrawSpecialFilter(void)
{
    player_t *player = &players[displayplayer];

    if(player->powers[pw_invulnerability])
    {
        float   max = 30;
        float   str, r, g, b;
        int     t = player->powers[pw_invulnerability];

        if(t < max)
            str = t / max;
        else if(t < 4 * 32 && !(t & 8))
            str = .7f;
        else if(t > INVULNTICS - max)
            str = (INVULNTICS - t) / max;
        else
            str = 1;            // Full inversion.
        // Draw an inversing filter.
        gl.Disable(DGL_TEXTURING);
        gl.Func(DGL_BLENDING, DGL_ONE_MINUS_DST_COLOR,
                DGL_ONE_MINUS_SRC_COLOR);
        r = str * 2;
        g = str * 2 - .4;
        b = str * 2 - .8;
        if(r < 0)
            r = 0;
        if(r > 1)
            r = 1;
        if(g < 0)
            g = 0;
        if(g > 1)
            g = 1;
        if(b < 0)
            b = 0;
        if(b > 1)
            b = 1;
        GL_DrawRect(0, 0, 320, 200, r, g, b, 1);

        // Restore the normal rendering state.
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
        gl.Enable(DGL_TEXTURING);
    }
}

/*
 * Show map name and author.
 */
void R_DrawLevelTitle(void)
{
    float   alpha = 1;
    int     y = 12;
    int     mapnum;
    char   *lname, *lauthor, *ptr;

    if(!cfg.levelTitle || actual_leveltime > 6 * 35)
        return;

    // Make the text a bit smaller.
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();
    gl.Translatef(160, y, 0);
    gl.Scalef(.7f, .7f, 1);
    gl.Translatef(-160, -y, 0);

    if(actual_leveltime < 35)
        alpha = actual_leveltime / 35.0f;
    if(actual_leveltime > 5 * 35)
        alpha = 1 - (actual_leveltime - 5 * 35) / 35.0f;

    // Get the strings from Doomsday
    lname = (char *) DD_GetVariable(DD_MAP_NAME);
    lauthor = (char *) DD_GetVariable(DD_MAP_AUTHOR);

    // Compose the mapnumber used to check the map name patches array.
    if(gamemode == commercial)
        mapnum = gamemap -1;
    else
        mapnum = ((gameepisode -1) * 9) + gamemap -1;

    if(lname)
    {
        ptr = strchr(lname, ':');   // Skip the E#M# or Level #.
        if(ptr)
        {
            lname = ptr + 1;
            while(*lname && isspace(*lname))
                lname++;
        }
        WI_DrawPatch(SCREENWIDTH / 2, y, 1, 1, 1, alpha, lnames[mapnum].lump,
                     lname, false, ALIGN_CENTER);
        y += 14;                //9;
    }

    gl.Color4f(.5f, .5f, .5f, alpha);
    if(lauthor && W_IsFromIWAD(lnames[mapnum].lump) &&
       (!cfg.hideAuthorIdSoft || stricmp(lauthor, "id software")))
    {
        M_WriteText2(160 - M_StringWidth(lauthor, hu_font_a) / 2, y, lauthor,
                     hu_font_a, -1, -1, -1, -1);
    }

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();
}

/*
 * Do not really change anything here,
 * because Doomsday might be in the middle of a refresh.
 * The change will take effect next refresh.
 */
void R_SetViewSize(int blocks, int detail)
{
    cfg.setsizeneeded = true;
    cfg.setblocks = blocks;
    setdetail = detail;
}

/*
 * Draw current display, possibly wiping it from the previous
 * wipegamestate can be set to -1 to force a wipe on the next draw
 */
void D_Display(void)
{
    static boolean viewactivestate = false;
    static boolean menuactivestate = false;
    static boolean inhelpscreensstate = false;
    static int fullscreenmode = 0;
    static gamestate_t oldgamestate = -1;
    int     ay;
    boolean redrawsbar;
    player_t *player = &players[displayplayer];
    boolean iscam = (player->plr->flags & DDPF_CAMERA) != 0;    // $democam

    if(nodrawers)
        return;                 // for comparative timing / profiling

    redrawsbar = false;

    // $democam: can be set on every frame
    if(cfg.setblocks > 10 || iscam)
    {
        // Full screen.
        R_SetViewWindowTarget(0, 0, 320, 200);
    }
    else
    {
        int w = cfg.setblocks * 32;
        int h = cfg.setblocks * (200 - ST_HEIGHT * cfg.sbarscale / 20) / 10;
        R_SetViewWindowTarget(160 - (w >> 1),
                              (200 - ST_HEIGHT * cfg.sbarscale / 20 - h) >> 1,
                              w, h);
    }
    
    {
        float x, y, w, h;
        R_GetViewWindow(&x, &y, &w, &h);
        R_ViewWindow((int) x, (int) y, (int) w, (int) h);
    } 

    // Do buffered drawing.
    switch (gamestate)
    {
    case GS_LEVEL:
        if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
            break;
        if(leveltime < 2)
        {
            // Don't render too early; the first couple of frames
            // might be a bit unstable -- this should be considered
            // a bug, but since there's an easy fix...
            break;
        }
        if(!automapactive || !amap_fullyopen || cfg.automapBack[3] < 1
           /*|| cfg.automapWidth < 1 || cfg.automapHeight < 1*/)
        {
            // Draw the player view.
            if(IS_CLIENT)
            {
                // Server updates mobj flags in NetSv_Ticker.
                R_SetAllDoomsdayFlags();
            }
            // The view angle offset.
            Set(DD_VIEWANGLE_OFFSET, ANGLE_MAX * -lookOffset);
            GL_SetFilter(players[displayplayer].plr->filter);   // $democam

            // How about fullbright?
            Set(DD_FULLBRIGHT, (player->powers[pw_infrared] > 4 * 32) ||
                (player->powers[pw_infrared] & 8) ||
                player->powers[pw_invulnerability] > 30);

            // Render the view with possible custom filters.
            R_RenderPlayerView(players[displayplayer].plr);

            R_DrawSpecialFilter();
            // Crosshair.
            if(!iscam)
                X_Drawer();     // $democam
        }

        // Draw the automap?
        if(automapactive)
            AM_Drawer();

        // These various HUD's will be drawn unless Doomsday advises not to
        if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
        {
            // Level information is shown for a few seconds in the
            // beginning of a level.
            R_DrawLevelTitle();

            if((viewheight != 200))
                redrawsbar = true;
            if(inhelpscreensstate && !inhelpscreens)
                redrawsbar = true;  // just put away the help screen

            // Do we need to render a full status bar at this point?
            if (!(automapactive && cfg.automapHudDisplay == 0 ))
            {
                if(!iscam)
                {
                    if(true == (viewheight == 200))
                    {
                        // Fullscreen. Which mode?
                        ST_Drawer(cfg.setblocks - 10 , redrawsbar);
                    }
                    else
                    {
                        ST_Drawer(0 , redrawsbar);
                    }
                }
                fullscreenmode = viewheight == 200;
            }

            HU_Drawer();
        }

        // Need to update the borders?
        if(oldgamestate != GS_LEVEL ||
            ((Get(DD_VIEWWINDOW_WIDTH) != 320 || menuactive ||
                cfg.sbarscale < 20 || !R_IsFullScreenViewWindow() ||
                (automapactive && cfg.automapHudDisplay == 0 ))))
        {
            // Update the borders.
            GL_Update(DDUF_BORDER);
        }

        break;

    case GS_INTERMISSION:
        WI_Drawer();
        break;

    case GS_WAITING:
        gl.Clear(DGL_COLOR_BUFFER_BIT);
        M_WriteText2(5, 188, "WAITING... PRESS ESC FOR MENU", hu_font_a, 1, 0, 0, 1);

    default:
        break;
    }

    GL_Update(DDUF_FULLSCREEN);

    menuactivestate = menuactive;
    viewactivestate = viewactive;
    inhelpscreensstate = inhelpscreens;
    oldgamestate = wipegamestate = gamestate;

    // draw pause pic (but not if InFine active)
    if(paused && !fi_active)
    {
        if(automapactive)
            ay = 4;
        else
            ay = viewwindowy + 4;

        WI_DrawPatch(SCREENWIDTH /2, ay, 1, 1, 1, 1, W_GetNumForName("M_PAUSE"),
                     NULL, false, ALIGN_CENTER);
    }

    // InFine is drawn whenever active.
    FI_Drawer();
}

/*
 * Updates the mobj flags used by Doomsday with the state
 * of our local flags for the given mobj.
 */
void P_SetDoomsdayFlags(mobj_t *mo)
{
    // Client mobjs can't be set here.
    if(IS_CLIENT && mo->ddflags & DDMF_REMOTE)
        return;

    // Reset the flags for a new frame.
    mo->ddflags &= DDMF_CLEAR_MASK;

    // Local objects aren't sent to clients.
    if(mo->flags & MF_LOCAL)
        mo->ddflags |= DDMF_LOCAL;
    if(mo->flags & MF_SOLID)
        mo->ddflags |= DDMF_SOLID;
    if(mo->flags & MF_NOGRAVITY)
        mo->ddflags |= DDMF_NOGRAVITY;
    if(mo->flags2 & MF2_FLOATBOB)
        mo->ddflags |= DDMF_NOGRAVITY | DDMF_BOB;
    if(mo->flags & MF_MISSILE)
    {
        mo->ddflags |= DDMF_MISSILE;
    }
    if(mo->type == MT_LIGHTSOURCE)
        mo->ddflags |= DDMF_ALWAYSLIT | DDMF_DONTDRAW;
    if(mo->info && mo->info->flags2 & MF2_ALWAYSLIT)
        mo->ddflags |= DDMF_ALWAYSLIT;

    if(mo->flags2 & MF2_FLY)
        mo->ddflags |= DDMF_FLY | DDMF_NOGRAVITY;

    // $democam: cameramen are invisible
    if(P_IsCamera(mo))
        mo->ddflags |= DDMF_DONTDRAW;

    if(mo->flags & MF_CORPSE && cfg.corpseTime && mo->corpsetics == -1)
        mo->ddflags |= DDMF_DONTDRAW;

    // Choose which ddflags to set.
    if(mo->flags2 & MF2_DONTDRAW)
    {
        mo->ddflags |= DDMF_DONTDRAW;
        return;                 // No point in checking the other flags.
    }

    if(mo->flags2 & MF2_LOGRAV)
        mo->ddflags |= DDMF_LOWGRAVITY;

    // The torches often go into the ceiling. This'll prevent
    // them from 'jumping'.
    if(mo->type == MT_MISC41 || mo->type == MT_MISC42 || mo->type == MT_MISC43  // tall torches
       || mo->type == MT_MISC44 || mo->type == MT_MISC45 || mo->type == MT_MISC46)  // short torches
        mo->ddflags |= DDMF_NOFITBOTTOM;

    if(mo->flags & MF_BRIGHTSHADOW)
        mo->ddflags |= DDMF_BRIGHTSHADOW;
    else if(mo->flags & MF_SHADOW)
        mo->ddflags |= DDMF_SHADOW;

    if((mo->flags & MF_VIEWALIGN && !(mo->flags & MF_MISSILE)) ||
       mo->flags & MF_FLOAT || (mo->flags & MF_MISSILE &&
                                !(mo->flags & MF_VIEWALIGN)))
        mo->ddflags |= DDMF_VIEWALIGN;

    mo->ddflags |= mo->flags & MF_TRANSLATION;
}

/*
 * Updates the status flags for all visible things.
 */
void R_SetAllDoomsdayFlags()
{
    int     i;
    int     count = DD_GetInteger(DD_SECTOR_COUNT);
    mobj_t *iter;

    // Only visible things are in the sector thinglists, so this is good.
    for(i = 0; i < count; i++)
    {
        for(iter = P_GetPtr(DMU_SECTOR, i, DMU_THINGS); iter; iter = iter->snext)
            P_SetDoomsdayFlags(iter);
    }
}
