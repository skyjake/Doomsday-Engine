/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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
 * d_refresh.c: - jDoom64 specific
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#include "jdoom64.h"

#include "hu_stuff.h"
#include "hu_menu.h"
#include "hu_pspr.h"
#include "hu_msg.h"
#include "am_map.h"
#include "g_common.h"
#include "r_common.h"
#include "d_net.h"
#include "f_infine.h"
#include "x_hair.h"
#include "g_controls.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "p_actor.h"
#include "rend_automap.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void            R_SetAllDoomsdayFlags(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float quitDarkenOpacity = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * Draws a special filter over the screen (eg the inversing filter used
 * when in god mode).
 */
static void drawSpecialFilter(int pnum, int x, int y, int w, int h)
{
#define T (player->powers[PT_INVULNERABILITY])

    player_t* player = &players[pnum];
    float max = 30, str, r, g, b;

    if(!T)
        return;

    if(T < max)
        str = T / max;
    else if(T < 4 * 32 && !(T & 8))
        str = .7f;
    else if(T > INVULNTICS - max)
        str = (INVULNTICS - T) / max;
    else
        str = 1; // Full inversion.

    // Draw an inversing filter.
    DGL_Disable(DGL_TEXTURING);
    DGL_BlendMode(BM_INVERSE);

    r = MINMAX_OF(0.f, str * 2, 1.f);
    g = MINMAX_OF(0.f, str * 2 - .4, 1.f);
    b = MINMAX_OF(0.f, str * 2 - .8, 1.f);

    DGL_DrawRect(x, y, w, h, r, g, b, 1);

    // Restore the normal rendering state.
    DGL_BlendMode(BM_NORMAL);
    DGL_Enable(DGL_TEXTURING);

#undef T
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
        rgba[CA] = (deathmatch? 1.0f : cfg.filterStrength) * filter / 9.f;
        return true;
    }

    if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
    {   // Gold.
        rgba[CR] = 1;
        rgba[CG] = .8f;
        rgba[CB] = .5f;
        rgba[CA] = cfg.filterStrength * (filter - STARTBONUSPALS + 1) / 16.f;
        return true;
    }

    if(filter == 13) // RADIATIONPAL
    {   // Green.
        rgba[CR] = 0;
        rgba[CG] = .7f;
        rgba[CB] = 0;
        rgba[CA] = cfg.filterStrength * .25f;
        return true;
    }

    if(filter)
        Con_Message("R_GetFilterColor: Real strange filter number: %d.\n", filter);

    return false;
}

static void rendPlayerView(int player)
{
    player_t* plr = &players[player];
    float viewPos[3], viewPitch, pspriteOffsetY;
    angle_t viewAngle;
    int isFullBright = ((plr->powers[PT_INFRARED] > 4 * 32) ||
                        (plr->powers[PT_INFRARED] & 8) ||
                        plr->powers[PT_INVULNERABILITY] > 30);

    if(IS_CLIENT)
    {
        // Server updates mobj flags in NetSv_Ticker.
        R_SetAllDoomsdayFlags();
    }

    viewPos[VX] = plr->plr->mo->pos[VX] + plr->viewOffset[VX];
    viewPos[VY] = plr->plr->mo->pos[VY] + plr->viewOffset[VY];
    viewPos[VZ] = plr->viewZ + plr->viewOffset[VZ];
    viewAngle = plr->plr->mo->angle + (int) (ANGLE_MAX * -G_GetLookOffset(player));
    viewPitch = plr->plr->lookDir;

    DD_SetVariable(DD_VIEW_X, &viewPos[VX]);
    DD_SetVariable(DD_VIEW_Y, &viewPos[VY]);
    DD_SetVariable(DD_VIEW_Z, &viewPos[VZ]);
    DD_SetVariable(DD_VIEW_ANGLE, &viewAngle);
    DD_SetVariable(DD_VIEW_PITCH, &viewPitch);

    pspriteOffsetY = HU_PSpriteYOffset(plr);
    DD_SetVariable(DD_PSPRITE_OFFSET_Y, &pspriteOffsetY);

    // $democam
    GL_SetFilter((plr->plr->flags & DDPF_VIEW_FILTER)? true : false);
    if(plr->plr->flags & DDPF_VIEW_FILTER)
    {
        const float* color = plr->plr->filterColor;
        GL_SetFilterColor(color[CR], color[CG], color[CB], color[CA]);
    }

    // How about fullbright?
    DD_SetInteger(DD_FULLBRIGHT, isFullBright);

    // Render the view with possible custom filters.
    R_RenderPlayerView(player);
}

static void rendHUD(int player, int viewW, int viewH)
{
    player_t* plr;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    if(G_GetGameState() != GS_MAP)
        return;

    if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
        return;

    plr = &players[player];

    // Draw the automap?
    AM_Drawer(player);

    // These various HUD's will be drawn unless Doomsday advises not to
    if(DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
    {
        ST_Drawer(player);

        // Set up the fixed 320x200 projection.
        DGL_MatrixMode(DGL_PROJECTION);
        DGL_PushMatrix();
        DGL_LoadIdentity();
        DGL_Ortho(0, 0, SCREENWIDTH, SCREENHEIGHT, -1, 1);

        // Draw HUD displays only visible when the automap is open.
        if(AM_IsActive(AM_MapForPlayer(player)))
            HU_DrawMapCounters();

        HU_Drawer(player);

        DGL_MatrixMode(DGL_PROJECTION);
        DGL_PopMatrix();

        // Level information is shown for a few seconds in the beginning of a level.
        if(cfg.mapTitle && !(actualMapTime > 6 * TICSPERSEC))
        {
            int needWidth;
            float scale;

            if(viewW >= viewH)
            {
                needWidth = (float)viewH/SCREENHEIGHT * SCREENWIDTH;
                scale = (float)viewH/SCREENHEIGHT;
            }
            else
            {
                needWidth = (float)viewW/SCREENWIDTH * SCREENWIDTH;
                scale = (float)viewW/SCREENWIDTH;
            }
            if(needWidth > viewW)
                scale *= (float)viewW/needWidth;

            scale *= (1+cfg.hudScale)/2;
            // Make the title 3/4 smaller.
            scale *= .75f;

            Hu_DrawMapTitle(viewW/2, (float)viewH/SCREENHEIGHT * 6, scale);
        }
    }
}

/**
 * Draws the in-viewport display.
 *
 * @param layer         @c 0 = bottom layer (before the viewport border).
 *                      @c 1 = top layer (after the viewport border).
 */
void D_Display(int layer)
{
    int player = DISPLAYPLAYER;
    int vpWidth, vpHeight;
    player_t* plr = &players[player];
    float x, y, w, h, xScale, yScale;

    R_GetViewPort(player, NULL, NULL, &vpWidth, &vpHeight);

    if(layer != 0)
    {
        rendHUD(player, vpWidth, vpHeight);
        return;
    }

    xScale = (float)vpWidth/SCREENWIDTH;
    yScale = (float)vpHeight/SCREENHEIGHT;

    if(G_GetGameState() == GS_MAP && cfg.screenBlocks <= 10 &&
       !(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))) // $democam: can be set on every frame.
    {
        R_GetViewWindow(&x, &y, &w, &h);
    }
    else
    {   // Full screen.
        x = 0;
        y = 0;
        w = SCREENWIDTH;
        h = SCREENHEIGHT;
    }

    R_SetViewWindow((int) (x * xScale), (int) (y * yScale), (int) (w * xScale), (int) (h * yScale));

    switch(G_GetGameState())
    {
    case GS_MAP:
        if(!R_MapObscures(player, (int) x, (int) y, (int) w, (int) h))
        {
            if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
                return;

            rendPlayerView(player);

            drawSpecialFilter(player, (int) (x * xScale), (int) (y * yScale), (int) (w * xScale), (int) (h * yScale));

            // Crosshair.
            if(!(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))) // $democam
                X_Drawer(player);
        }
        break;
    case GS_STARTUP:
        DGL_Disable(DGL_TEXTURING);
        DGL_DrawRect(0, 0, vpWidth, vpHeight, 0, 0, 0, 1);
        DGL_Enable(DGL_TEXTURING);
        break;
    default:
        break;
    }
}

void D_Display2(void)
{
    switch(G_GetGameState())
    {
    case GS_INTERMISSION:
        WI_Drawer();
        break;

    case GS_WAITING:
        //gl.Clear(DGL_COLOR_BUFFER_BIT);
        //M_WriteText2(5, 188, "WAITING... PRESS ESC FOR MENU", GF_FONTA, 1, 0, 0, 1);
        break;

    default:
        break;
    }

    // InFine is drawn whenever active.
    FI_Drawer();

    // Draw HUD displays; menu, messages.
    Hu_Drawer();

    if(G_GetGameAction() == GA_QUIT)
    {
        DGL_Disable(DGL_TEXTURING);
        DGL_DrawRect(0, 0, 320, 200, 0, 0, 0, quitDarkenOpacity);
        DGL_Enable(DGL_TEXTURING);
    }
}

/**
 * Updates the mobj flags used by Doomsday with the state of our local flags
 * for the given mobj.
 */
void P_SetDoomsdayFlags(mobj_t* mo)
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
        mo->ddFlags |= DDMF_MISSILE;
    if(mo->type == MT_LIGHTSOURCE)
        mo->ddFlags |= DDMF_ALWAYSLIT | DDMF_DONTDRAW;
    if(mo->info && mo->info->flags2 & MF2_ALWAYSLIT)
        mo->ddFlags |= DDMF_ALWAYSLIT;

    if(mo->flags2 & MF2_FLY)
        mo->ddFlags |= DDMF_FLY | DDMF_NOGRAVITY;

    // $democam: cameramen are invisible
    if(P_MobjIsCamera(mo))
        mo->ddFlags |= DDMF_DONTDRAW;

    if((mo->flags & MF_CORPSE) && cfg.corpseTime && mo->corpseTics == -1)
        mo->ddFlags |= DDMF_DONTDRAW;

    // Choose which ddflags to set.
    if(mo->flags2 & MF2_DONTDRAW)
    {
        mo->ddFlags |= DDMF_DONTDRAW;
        return; // No point in checking the other flags.
    }

    if(mo->flags2 & MF2_LOGRAV)
        mo->ddFlags |= DDMF_LOWGRAVITY;

    // The torches often go into the ceiling. This'll prevent
    // them from 'jumping'.
    if(mo->type == MT_MISC41 || mo->type == MT_MISC42 || mo->type == MT_MISC43 || // tall torches
       mo->type == MT_MISC44 || mo->type == MT_MISC45 || mo->type == MT_MISC46)  // short torches
        mo->ddFlags |= DDMF_NOFITBOTTOM;

    if(mo->flags & MF_BRIGHTSHADOW)
        mo->ddFlags |= DDMF_BRIGHTSHADOW;
    else if(mo->flags & MF_SHADOW)
        mo->ddFlags |= DDMF_SHADOW;

    if(((mo->flags & MF_VIEWALIGN) && !(mo->flags & MF_MISSILE)) ||
       (mo->flags & MF_FLOAT) || ((mo->flags & MF_MISSILE) &&
                                !(mo->flags & MF_VIEWALIGN)))
        mo->ddFlags |= DDMF_VIEWALIGN;

    if(mo->flags & MF_TRANSLATION)
        mo->tmap = (mo->flags & MF_TRANSLATION) >> MF_TRANSSHIFT;
}

/**
 * Updates the status flags for all visible things.
 */
void R_SetAllDoomsdayFlags(void)
{
    int                 i;
    int                 count = DD_GetInteger(DD_SECTOR_COUNT);
    mobj_t*             iter;

    // Only visible things are in the sector thinglists, so this is good.
    for(i = 0; i < count; ++i)
    {
        for(iter = P_GetPtr(DMU_SECTOR, i, DMT_MOBJS); iter; iter = iter->sNext)
            P_SetDoomsdayFlags(iter);
    }
}
