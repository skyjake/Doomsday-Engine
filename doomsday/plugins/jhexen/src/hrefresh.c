/**\file
 *\section Copyright and License Summary
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
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
 * jHexen-specific refresh stuff.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "jhexen.h"

#include "f_infine.h"
#include "r_common.h"
#include "p_mapsetup.h"
#include "g_controls.h"
#include "am_map.h"

// MACROS ------------------------------------------------------------------

#define FMAKERGBA(r,g,b,a) ( (byte)(0xff*r) + ((byte)(0xff*g)<<8) + ((byte)(0xff*b)<<16) + ((byte)(0xff*a)<<24) )

#define viewheight  Get(DD_VIEWWINDOW_HEIGHT)

#define SIZEFACT 4
#define SIZEFACT2 16

// TYPES -------------------------------------------------------------------

// This could hold much more detailed information...
typedef struct textype_s {
    char    name[9];            // Name of the texture.
    int     type;               // Which type?
} textype_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void    X_Drawer(void);
void    R_SetAllDoomsdayFlags(void);
void    H2_AdvanceDemo(void);
void    MN_DrCenterTextA_CS(char *text, int center_x, int y);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern float lookOffset;
extern int actual_leveltime;

extern float deffontRGB[];

extern boolean dontrender;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean setsizeneeded;

// wipegamestate can be set to -1 to force a wipe on the next draw
gamestate_t wipegamestate = GS_DEMOSCREEN;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Don't really change anything here, because i might be in the middle of
 * a refresh.  The change will take effect next refresh.
 */
void R_SetViewSize(int blocks, int detail)
{
    setsizeneeded = true;
    if(cfg.setblocks != blocks && blocks > 10 && blocks < 13)
    {   // When going fullscreen, force a hud show event (to reset the timer).
        ST_HUDUnHide(HUE_FORCE);
    }
    cfg.setblocks = blocks;
    GL_Update(DDUF_BORDER);
}

void R_DrawMapTitle(void)
{
    float       alpha = 1;
    int         y = 12;
    char       *lname, *lauthor;

    if(!cfg.levelTitle || actual_leveltime > 6 * 35)
        return;

    // Make the text a bit smaller.
    gl.MatrixMode(DGL_MODELVIEW);
    gl.PushMatrix();
    gl.Translatef(160, y, 0);
    gl.Scalef(.75f, .75f, 1);   // Scale to 3/4
    gl.Translatef(-160, -y, 0);

    if(actual_leveltime < 35)
        alpha = actual_leveltime / 35.0f;
    if(actual_leveltime > 5 * 35)
        alpha = 1 - (actual_leveltime - 5 * 35) / 35.0f;

    lname = (char *) DD_GetVariable(DD_MAP_NAME);
    lauthor = (char *) DD_GetVariable(DD_MAP_AUTHOR);

    // Use stardard map name if DED didn't define it.
    if(!lname)
        lname = P_GetMapName(gamemap);

    Draw_BeginZoom((1 + cfg.hudScale)/2, 160, y);

    if(lname)
    {
        M_WriteText3(160 - M_StringWidth(lname, hu_font_b) / 2, y, lname,
                    hu_font_b, deffontRGB[0], deffontRGB[1], deffontRGB[2],
                    alpha, false, 0);
        y += 20;
    }

    if(lauthor)
    {
        M_WriteText3(160 - M_StringWidth(lauthor, hu_font_a) / 2, y, lauthor,
                    hu_font_a, .5f, .5f, .5f, alpha, false, 0);
    }

    Draw_EndZoom();

    gl.MatrixMode(DGL_MODELVIEW);
    gl.PopMatrix();
}

void G_Drawer(void)
{
    static boolean viewactivestate = false;
    static boolean menuactivestate = false;
    static int  fullscreenmode = 0;
    static gamestate_t oldgamestate = -1;
    int         py;
    player_t   *vplayer = &players[displayplayer];
    boolean     iscam = (vplayer->plr->flags & DDPF_CAMERA) != 0;   // $democam
    float       x, y, w, h;
    boolean     mapHidesView;

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

    R_GetViewWindow(&x, &y, &w, &h);
    R_ViewWindow((int) x, (int) y, (int) w, (int) h);

    // Do buffered drawing
    switch(G_GetGameState())
    {
    case GS_LEVEL:
        // Clients should be a little careful about the first frames.
        if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
            break;

        // Good luck trying to render the view without a viewpoint...
        if(!vplayer->plr->mo)
            break;

        if(!IS_CLIENT && leveltime < 2)
        {
            // Don't render too early; the first couple of frames
            // might be a bit unstable -- this should be considered
            // a bug, but since there's an easy fix...
            break;
        }

        mapHidesView =
            R_MapObscures(displayplayer, (int) x, (int) y, (int) w, (int) h);

        if(!(MN_CurrentMenuHasBackground() && MN_MenuAlpha() >= 1) &&
           !mapHidesView)
        {
            boolean special200 = false;

            R_HandleSectorSpecials();
            // Set flags for the renderer.
            if(IS_CLIENT)
            {
                // Server updates mobj flags in NetSv_Ticker.
                R_SetAllDoomsdayFlags();
            }
            GL_SetFilter(vplayer->plr->filter); // $democam
            // Check for the sector special 200: use sky2.
            // I wonder where this is used?
            if(P_XSectorOfSubsector(vplayer->plr->mo->subsector)->special == 200)
            {
                special200 = true;
                Rend_SkyParams(0, DD_DISABLE, 0);
                Rend_SkyParams(1, DD_ENABLE, 0);
            }
            // How about a bit of quake?
            if(localQuakeHappening[displayplayer] && !paused)
            {
                int     intensity = localQuakeHappening[displayplayer];

                Set(DD_VIEWX_OFFSET,
                    ((M_Random() % (intensity << 2)) -
                     (intensity << 1)) << FRACBITS);
                Set(DD_VIEWY_OFFSET,
                    ((M_Random() % (intensity << 2)) -
                     (intensity << 1)) << FRACBITS);
            }
            else
            {
                Set(DD_VIEWX_OFFSET, 0);
                Set(DD_VIEWY_OFFSET, 0);
            }
            // The view angle offset.
            Set(DD_VIEWANGLE_OFFSET, ANGLE_MAX * -G_GetLookOffset(displayplayer));
            // Render the view.
            if(!dontrender)
            {
                R_RenderPlayerView(vplayer->plr);
            }
            if(special200)
            {
                Rend_SkyParams(0, DD_ENABLE, 0);
                Rend_SkyParams(1, DD_DISABLE, 0);
            }
            if(!iscam)
                X_Drawer();     // Draw the crosshair.

        }

        // Draw the automap.
        AM_Drawer(displayplayer);

        // Need to update the borders?
        if(oldgamestate != GS_LEVEL ||
            (Get(DD_VIEWWINDOW_WIDTH) != 320 || menuactive ||
                cfg.sbarscale < 20 || !R_IsFullScreenViewWindow()))
        {
            // Update the borders.
            GL_Update(DDUF_BORDER);
        }
        break;

    default:
        break;
    }

    menuactivestate = menuactive;
    viewactivestate = viewactive;
    oldgamestate = wipegamestate = G_GetGameState();

    if(paused && !fi_active)
    {
        //if(AM_IsMapActive(displayplayer))
            py = 4;
        //else
        //    py = 4; // in jDOOM this is viewwindowy + 4

        GL_DrawPatch(160, py, W_GetNumForName("PAUSED"));
    }
}

void G_Drawer2(void)
{
    // Do buffered drawing
    switch(G_GetGameState())
    {
    case GS_LEVEL:
        // These various HUD's will be drawn unless Doomsday advises not to
        if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
        {
            // Draw HUD displays only visible when the automap is open.
            if(AM_IsMapActive(displayplayer))
                HU_DrawMapCounters();

            // Level information is shown for a few seconds in the
            // beginning of a level.
            R_DrawMapTitle();

            GL_Update(DDUF_FULLSCREEN);

            // DJS - Do we need to render a full status bar at this point?
            if (!(AM_IsMapActive(displayplayer) && cfg.automapHudDisplay == 0 ))
            {
                player_t   *vplayer = &players[displayplayer];
                boolean     iscam = (vplayer->plr->flags & DDPF_CAMERA) != 0; // $democam

                if(!iscam)
                {
                    if(true == (viewheight == 200))
                    {
                        // Fullscreen. Which mode?
                        ST_Drawer(cfg.setblocks - 10, true);    // $democam
                    }
                    else
                    {
                        ST_Drawer(0 , true);    // $democam
                    }
                }
            }

            HU_Drawer();
        }
        break;

    case GS_INTERMISSION:
        IN_Drawer();
        break;

    case GS_INFINE:
        GL_Update(DDUF_FULLSCREEN);
        break;

    case GS_WAITING:
        GL_DrawRawScreen(W_GetNumForName("TITLE"), 0, 0);
        gl.Color3f(1, 1, 1);
        MN_DrCenterTextA_CS("WAITING... PRESS ESC FOR MENU", 160, 188);
        GL_Update(DDUF_FULLSCREEN);
        break;

    default:
        break;
    }

    // InFine is drawn whenever active.
    FI_Drawer();

    // The menu is drawn whenever active.
    M_Drawer();
}

int R_GetFilterColor(int filter)
{
    // We have to choose the right color and alpha.
    if(filter >= STARTREDPALS && filter < STARTREDPALS + NUMREDPALS)
        // Red?
        return FMAKERGBA(1, 0, 0, filter / 8.0);    // Full red with filter 8.
    else if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
        // Light Yellow?
        return FMAKERGBA(1, 1, .5, (filter - STARTBONUSPALS + 1) / 16.0);
    else if(filter >= STARTPOISONPALS &&
            filter < STARTPOISONPALS + NUMPOISONPALS)
        // Green?
        return FMAKERGBA(0, 1, 0, (filter - STARTPOISONPALS + 1) / 16.0);
    else if(filter >= STARTSCOURGEPAL)
        // Orange?
        return FMAKERGBA(1, .5, 0, (STARTSCOURGEPAL + 3 - filter) / 6.0);
    else if(filter >= STARTHOLYPAL)
        // White?
        return FMAKERGBA(1, 1, 1, (STARTHOLYPAL + 3 - filter) / 6.0);
    else if(filter == STARTICEPAL)
        // Light blue?
        return FMAKERGBA(.5f, .5f, 1, .4f);
    else if(filter)
        Con_Error("R_GetFilterColor: Strange filter number: %d.\n", filter);
    return 0;
}

void R_SetFilter(int filter)
{
    GL_SetFilter(R_GetFilterColor(filter));
}

void H2_EndFrame(void)
{
    SN_UpdateActiveSequences();
}

/**
 * Updates ddflags of all visible mobjs (in sectorlinks).
 * Not strictly necessary (in single player games at least) but here
 * we tell the engine about light-emitting objects, special effects,
 * object properties (solid, local, low/nograv, etc.), color translation
 * and other interesting little details.
 */
void R_SetAllDoomsdayFlags(void)
{
    uint        i;
    int         Class;
    mobj_t     *mo;

    // Only visible things are in the sector thinglists, so this is good.
    for(i = 0; i < numsectors; ++i)
        for(mo = P_GetPtr(DMU_SECTOR, i, DMU_THINGS); mo; mo = mo->snext)
        {
            if(IS_CLIENT && mo->ddflags & DDMF_REMOTE)
                continue;

            // Reset the flags for a new frame.
            mo->ddflags &= DDMF_CLEAR_MASK;

            if(mo->flags & MF_LOCAL)
                mo->ddflags |= DDMF_LOCAL;
            if(mo->flags & MF_SOLID)
                mo->ddflags |= DDMF_SOLID;
            if(mo->flags & MF_MISSILE)
                mo->ddflags |= DDMF_MISSILE;
            if(mo->flags2 & MF2_FLY)
                mo->ddflags |= DDMF_FLY | DDMF_NOGRAVITY;
            if(mo->flags2 & MF2_FLOATBOB)
                mo->ddflags |= DDMF_BOB | DDMF_NOGRAVITY;
            if(mo->flags2 & MF2_LOGRAV)
                mo->ddflags |= DDMF_LOWGRAVITY;
            if(mo->flags & MF_NOGRAVITY /* || mo->flags2 & MF2_FLY */ )
                mo->ddflags |= DDMF_NOGRAVITY;

            // $democam: cameramen are invisible
            if(P_IsCamera(mo))
                mo->ddflags |= DDMF_DONTDRAW;

            // Choose which ddflags to set.
            if(mo->flags2 & MF2_DONTDRAW)
            {
                mo->ddflags |= DDMF_DONTDRAW;
                continue;       // No point in checking the other flags.
            }

            if((mo->flags & MF_BRIGHTSHADOW) == MF_BRIGHTSHADOW)
                mo->ddflags |= DDMF_BRIGHTSHADOW;
            else
            {
                if(mo->flags & MF_SHADOW)
                    mo->ddflags |= DDMF_SHADOW;
                if(mo->flags & MF_ALTSHADOW ||
                   (cfg.translucentIceCorpse && mo->flags & MF_ICECORPSE))
                    mo->ddflags |= DDMF_ALTSHADOW;
            }

            if((mo->flags & MF_VIEWALIGN && !(mo->flags & MF_MISSILE)) ||
               mo->flags & MF_FLOAT || (mo->flags & MF_MISSILE &&
                                        !(mo->flags & MF_VIEWALIGN)))
                mo->ddflags |= DDMF_VIEWALIGN;

            mo->ddflags |= mo->flags & MF_TRANSLATION;

            // Which translation table to use?
            if(mo->flags & MF_TRANSLATION)
            {
                if(mo->player)
                {
                    Class = mo->player->class;
                }
                else
                {
                    Class = mo->special1;
                }
                if(Class > 2)
                {
                    Class = 0;
                }
                // The last two bits.
                mo->ddflags |= Class << DDMF_CLASSTRSHIFT;
            }

            // An offset for the light emitted by this object.
            /*          Class = MobjLightOffsets[mo->type];
               if(Class < 0) Class = 8-Class;
               // Class must now be in range 0-15.
               mo->ddflags |= Class << DDMF_LIGHTOFFSETSHIFT; */

            // The Mage's ice shards need to be a bit smaller.
            // This'll make them half the normal size.
            if(mo->type == MT_SHARDFX1)
                mo->ddflags |= 2 << DDMF_LIGHTSCALESHIFT;
        }
}
