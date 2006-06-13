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
 * H_Refresh.c
 */

// HEADER FILES ------------------------------------------------------------

#include <ctype.h>  // has isspace
#include <math.h>
#include "jheretic.h"
#include "f_infine.h"
#include "r_common.h"

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

extern boolean finalestage;

extern boolean amap_fullyopen;

extern boolean inhelpscreens;
extern float lookOffset;

extern const float deffontRGB[];
extern const float deffontRGB2[];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

player_t *viewplayer;

gamestate_t wipegamestate = GS_DEMOSCREEN;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Creates the translation tables to map
 * the green color ramp to gray, brown, red.
 * Assumes a given structure of the PLAYPAL.
 * Could be read from a lump instead.
 */
void R_InitTranslationTables(void)
{
    byte   *translationtables = (byte *)
        DD_GetVariable(DD_TRANSLATIONTABLES_ADDRESS);
    int     i;

    // Fill out the translation tables
    for(i = 0; i < 256; i++)
    {
        if(i >= 225 && i <= 240)
        {
            translationtables[i] = 114 + (i - 225); // yellow
            translationtables[i + 256] = 145 + (i - 225);   // red
            translationtables[i + 512] = 190 + (i - 225);   // blue
        }
        else
        {
            translationtables[i] = translationtables[i + 256] =
                translationtables[i + 512] = i;
        }
    }
}

/*
 * Draws a special filter over the screen
 */
void R_DrawRingFilter()
{
    gl.Disable(DGL_TEXTURING);
    if(cfg.ringFilter == 1)
    {
        gl.Func(DGL_BLENDING, DGL_SRC_COLOR, DGL_SRC_COLOR);
        GL_DrawRect(0, 0, 320, 200, .5f, .35f, .1f, 1);

        /*gl.Func(DGL_BLENDING, DGL_ZERO, DGL_SRC_COLOR);
           GL_DrawRect(0, 0, 320, 200, 1, .7f, 0, 1);
           gl.Func(DGL_BLENDING, DGL_ONE, DGL_DST_COLOR);
           GL_DrawRect(0, 0, 320, 200, .1f, 0, 0, 1); */

        /*gl.Func(DGL_BLENDING, DGL_ZERO, DGL_SRC_COLOR);
           GL_DrawRect(0, 0, 320, 200, 0, .6f, 0, 1);
           gl.Func(DGL_BLENDING, DGL_ONE_MINUS_DST_COLOR, DGL_ONE_MINUS_SRC_COLOR);
           GL_DrawRect(0, 0, 320, 200, 1, 0, 0, 1); */
    }
    else
    {
        gl.Func(DGL_BLENDING, DGL_DST_COLOR, DGL_SRC_COLOR);
        GL_DrawRect(0, 0, 320, 200, 0, 0, .6f, 1);
    }
    // Restore the normal rendering state.
    gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
    gl.Enable(DGL_TEXTURING);

}

/*
 * Show map name and author.
 */
void R_DrawLevelTitle(void)
{
    float   alpha = 1;
    int     y = 13;
    char   *lname, *lauthor, *ptr;

    if(!cfg.levelTitle || actual_leveltime > 6 * 35)
        return;

    if(actual_leveltime < 35)
        alpha = actual_leveltime / 35.0f;
    if(actual_leveltime > 5 * 35)
        alpha = 1 - (actual_leveltime - 5 * 35) / 35.0f;

    lname = (char *) DD_GetVariable(DD_MAP_NAME);
    lauthor = (char *) DD_GetVariable(DD_MAP_AUTHOR);
    Draw_BeginZoom((1 + cfg.hudScale)/2, 160, y);

    if(lname)
    {
        // Skip the ExMx.
        ptr = strchr(lname, ':');
        if(ptr)
        {
            lname = ptr + 1;
            while(*lname && isspace(*lname))
                lname++;
        }
        M_WriteText3(160 - M_StringWidth(lname, hu_font_b) / 2, y, lname,
                    hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2], alpha, false, 0);
        y += 20;
    }

    if(lauthor && stricmp(lauthor, "raven software"))
    {
        M_WriteText3(160 - M_StringWidth(lauthor, hu_font_a) / 2, y, lauthor,
                    hu_font_a, .5f, .5f, .5f, alpha, false, 0);
    }
    Draw_EndZoom();
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
    //static int fullscreenmode = 0;
    static gamestate_t oldgamestate = -1;
    int     ay;
    player_t *vplayer = &players[displayplayer];
    boolean iscam = (vplayer->plr->flags & DDPF_CAMERA) != 0;   // $democam

    // $democam: can be set on every frame
    if(cfg.setblocks > 10 || iscam)
    {
        // Full screen.
        R_SetViewWindowTarget(0, 0, 320, 200);
    }
    else
    {
        int w = cfg.setblocks * 32;
        int h = cfg.setblocks * (200 - SBARHEIGHT * cfg.sbarscale / 20) / 10;
        R_SetViewWindowTarget(160 - (w >> 1),
                              (200 - SBARHEIGHT * cfg.sbarscale / 20 - h) >> 1,
                              w, h);
    }

    {
        float x, y, w, h;
        R_GetViewWindow(&x, &y, &w, &h);
        R_ViewWindow((int) x, (int) y, (int) w, (int) h);
    }

    // Do buffered drawing
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
        if(!automapactive || !amap_fullyopen || cfg.automapBack[3] < 1 /*|| cfg.automapWidth < 1 || cfg.automapHeight < 1*/)
        {
            // Draw the player view.
            if(IS_CLIENT)
            {
                // Server updates mobj flags in NetSv_Ticker.
                R_SetAllDoomsdayFlags();
            }
            // The view angle offset.
            Set(DD_VIEWANGLE_OFFSET, ANGLE_MAX * -lookOffset);
            GL_SetFilter(vplayer->plr->filter);

            // How about fullbright?
            Set(DD_FULLBRIGHT, vplayer->powers[pw_invulnerability]);

            // Render the view with possible custom filters.
            R_RenderPlayerView(vplayer->plr);

            if(vplayer->powers[pw_invulnerability])
                R_DrawRingFilter();

            // Crosshair.
            if(!iscam)
                X_Drawer();
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

            // Do we need to render a full status bar at this point?
            if(!(automapactive && cfg.automapHudDisplay == 0))
            {
                if(!iscam)
                {
                    if(viewheight == 200)
                    {
                        // Fullscreen. Which mode?
                        ST_Drawer(cfg.setblocks - 10, true);    // $democam
                    }
                    else
                    {
                        ST_Drawer(0, true);    // $democam
                    }
                }
                //fullscreenmode = (viewheight == 200);
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
        IN_Drawer();
        break;

    case GS_WAITING:
        // Clear the screen while waiting, doesn't mess up the menu.
        gl.Clear(DGL_COLOR_BUFFER_BIT);
        break;

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
            ay = 4; // in jDOOM this is viewwindowy + 4

        GL_DrawPatch(160, ay, W_GetNumForName("PAUSED"));
    }

    // InFine is drawn whenever active.
    FI_Drawer();
}

/*
 * Updates the mobj flags used by Doomsday with the state
 * of our local flags for the given mobj.
 */
void R_SetDoomsdayFlags(mobj_t *mo)
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
        // Mace death balls are controlled by the server.
        //if(mo->type != MT_MACEFX4)
        mo->ddflags |= DDMF_MISSILE;
    }
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

    if(mo->flags & MF_BRIGHTSHADOW)
        mo->ddflags |= DDMF_BRIGHTSHADOW;
    else if(mo->flags & MF_SHADOW)
        mo->ddflags |= DDMF_ALTSHADOW;

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
            R_SetDoomsdayFlags(iter);
    }
}
