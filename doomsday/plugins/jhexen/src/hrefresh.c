/**\file hrefresh.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * Refresh - Hexen specific.
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#include "jhexen.h"

#include "r_common.h"
#include "p_mapsetup.h"
#include "g_controls.h"
#include "g_common.h"
#include "hu_menu.h"
#include "hu_msg.h"
#include "hu_pspr.h"
#include "hu_log.h"
#include "hu_stuff.h"
#include "am_map.h"
#include "x_hair.h"
#include "p_tick.h"
#include "hu_automap.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float quitDarkenOpacity = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

static void rendPlayerView(int player)
{
    player_t* plr = &players[player];
    boolean special200 = false;
    float viewPos[3], viewPitch, pspriteOffsetY;
    angle_t viewAngle;

    if(!plr->plr->mo)
    {
        Con_Message("rendPlayerView: Rendering view of player %i, who has no mobj!\n", player);
        return;
    }

    if(IS_CLIENT)
    {
        // Server updates mobj flags in NetSv_Ticker.
        R_SetAllDoomsdayFlags();
    }

    // Check for the sector special 200: use sky2.
    // I wonder where this is used?
    if(P_ToXSectorOfSubsector(plr->plr->mo->subsector)->special == 200)
    {
        special200 = true;
        Rend_SkyParams(0, DD_DISABLE, NULL);
        Rend_SkyParams(1, DD_ENABLE, NULL);
    }

    // How about a bit of quake?
    if(localQuakeHappening[player] && !P_IsPaused())
    {
        int intensity = localQuakeHappening[player];

        plr->viewOffset[VX] =
            (float) ((M_Random() % (intensity << 2)) - (intensity << 1));
        plr->viewOffset[VY] =
            (float) ((M_Random() % (intensity << 2)) - (intensity << 1));
    }
    else
    {
        plr->viewOffset[VX] = plr->viewOffset[VY] = 0;
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
    GL_SetFilter((plr->plr->flags & DDPF_USE_VIEW_FILTER)? true : false);
    if(plr->plr->flags & DDPF_USE_VIEW_FILTER)
    {
        const float* color = plr->plr->filterColor;
        GL_SetFilterColor(color[CR], color[CG], color[CB], color[CA]);
    }

    // Render the view with possible custom filters.
    R_RenderPlayerView(player);

    if(special200)
    {
        Rend_SkyParams(0, DD_ENABLE, NULL);
        Rend_SkyParams(1, DD_DISABLE, NULL);
    }
}

static void rendHUD(int player, int viewW, int viewH)
{
    if(player < 0 || player >= MAXPLAYERS)
        return;

    if(G_GetGameState() != GS_MAP)
        return;

    if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
        return;

    if(!DD_GetInteger(DD_GAME_DRAW_HUD_HINT))
        return; // The engine advises not to draw any HUD displays.

    ST_Drawer(player);
    HU_DrawScoreBoard(player);

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

/**
 * Draws the in-viewport display.
 *
 * @param layer         @c 0 = bottom layer (before the viewport border).
 *                      @c 1 = top layer (after the viewport border).
 */
void G_Display(int layer)
{
    const int player = DISPLAYPLAYER;
    player_t* plr = players + player;

    if(layer != 0)
    {
        rectanglei_t vp;
        R_ViewportDimensions(player, &vp.x, &vp.y, &vp.width, &vp.height);
        rendHUD(player, vp.width, vp.height);
        return;
    }   

    switch(G_GetGameState())
    {
    case GS_MAP: {
        rectanglei_t vw;
        R_ViewWindowDimensions(player, &vw.x, &vw.y, &vw.width, &vw.height);
        if(!ST_AutomapWindowObscures(player, vw.x, vw.y, vw.width, vw.height))
        {
            if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME)))
                return;

            rendPlayerView(player);

            // Crosshair.
            if(!(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))) // $democam
                X_Drawer(player);
        }
        break;
      }
    case GS_STARTUP: {
        rectanglei_t vp;
        R_ViewportDimensions(player, &vp.x, &vp.y, &vp.width, &vp.height);
        DGL_DrawRectColor(0, 0, vp.width, vp.height, 0, 0, 0, 1);
        break;
      }
    default:
        break;
    }
}

void G_Display2(void)
{
    if(G_GetGameState() == GS_INTERMISSION)
    {
        IN_Drawer();
    }

    // Draw HUD displays; menu, messages.
    Hu_Drawer();

    if(G_GetGameAction() == GA_QUIT)
    {
        DGL_DrawRectColor(0, 0, 320, 200, 0, 0, 0, quitDarkenOpacity);
    }
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
        rgba[CA] = (deathmatch? 1.0f : cfg.filterStrength) * filter / 8.f; // Full red with filter 8.
        return true;
    }
    else if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
    {   // Light Yellow.
        rgba[CR] = 1;
        rgba[CG] = 1;
        rgba[CB] = .5f;
        rgba[CA] = cfg.filterStrength * (filter - STARTBONUSPALS + 1) / 16.f;
        return true;
    }
    else if(filter >= STARTPOISONPALS && filter < STARTPOISONPALS + NUMPOISONPALS)
    {   // Green.
        rgba[CR] = 0;
        rgba[CG] = 1;
        rgba[CB] = 0;
        rgba[CA] = cfg.filterStrength * (filter - STARTPOISONPALS + 1) / 16.f;
        return true;
    }
    else if(filter >= STARTSCOURGEPAL)
    {   // Orange.
        rgba[CR] = 1;
        rgba[CG] = .5f;
        rgba[CB] = 0;
        rgba[CA] = cfg.filterStrength * (STARTSCOURGEPAL + 3 - filter) / 6.f;
        return true;
    }
    else if(filter >= STARTHOLYPAL)
    {   // White.
        rgba[CR] = 1;
        rgba[CG] = 1;
        rgba[CB] = 1;
        rgba[CA] = cfg.filterStrength * (STARTHOLYPAL + 3 - filter) / 6.f;
        return true;
    }
    else if(filter == STARTICEPAL)
    {   // Light blue.
        rgba[CR] = .5f;
        rgba[CG] = .5f;
        rgba[CB] = 1;
        rgba[CA] = cfg.filterStrength * .4f;
        return true;
    }

    if(filter)
        Con_Error("R_GetFilterColor: Strange filter number: %d.\n", filter);
    return false;
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
    uint                i;
    mobj_t*             mo;

    // Only visible things are in the sector thinglists, so this is good.
    for(i = 0; i < numsectors; ++i)
        for(mo = P_GetPtr(DMU_SECTOR, i, DMT_MOBJS); mo; mo = mo->sNext)
        {
            if(IS_CLIENT && mo->ddFlags & DDMF_REMOTE)
                continue;

            // Reset the flags for a new frame.
            mo->ddFlags &= DDMF_CLEAR_MASK;

            if(mo->flags & MF_LOCAL)
                mo->ddFlags |= DDMF_LOCAL;
            if(mo->flags & MF_SOLID)
                mo->ddFlags |= DDMF_SOLID;
            if(mo->flags & MF_MISSILE)
                mo->ddFlags |= DDMF_MISSILE;
            if(mo->flags2 & MF2_FLY)
                mo->ddFlags |= DDMF_FLY | DDMF_NOGRAVITY;
            if(mo->flags2 & MF2_FLOATBOB)
                mo->ddFlags |= DDMF_BOB | DDMF_NOGRAVITY;
            if(mo->flags2 & MF2_LOGRAV)
                mo->ddFlags |= DDMF_LOWGRAVITY;
            if(mo->flags & MF_NOGRAVITY /* || mo->flags2 & MF2_FLY */ )
                mo->ddFlags |= DDMF_NOGRAVITY;

            // $democam: cameramen are invisible.
            if(P_MobjIsCamera(mo))
                mo->ddFlags |= DDMF_DONTDRAW;

            // Choose which ddflags to set.
            if(mo->flags2 & MF2_DONTDRAW)
            {
                mo->ddFlags |= DDMF_DONTDRAW;
                continue; // No point in checking the other flags.
            }

            if((mo->flags & MF_BRIGHTSHADOW) == MF_BRIGHTSHADOW)
                mo->ddFlags |= DDMF_BRIGHTSHADOW;
            else
            {
                if(mo->flags & MF_SHADOW)
                    mo->ddFlags |= DDMF_SHADOW;
                if(mo->flags & MF_ALTSHADOW ||
                   (cfg.translucentIceCorpse && mo->flags & MF_ICECORPSE))
                    mo->ddFlags |= DDMF_ALTSHADOW;
            }

            if((mo->flags & MF_VIEWALIGN && !(mo->flags & MF_MISSILE)) ||
               mo->flags & MF_FLOAT || (mo->flags & MF_MISSILE &&
                                        !(mo->flags & MF_VIEWALIGN)))
                mo->ddFlags |= DDMF_VIEWALIGN;

            R_SetTranslation(mo);

            // An offset for the light emitted by this object.
            /*          Class = MobjLightOffsets[mo->type];
               if(Class < 0) Class = 8-Class;
               // Class must now be in range 0-15.
               mo->ddFlags |= Class << DDMF_LIGHTOFFSETSHIFT; */

            // The Mage's ice shards need to be a bit smaller.
            // This'll make them half the normal size.
            if(mo->type == MT_SHARDFX1)
                mo->ddFlags |= 2 << DDMF_LIGHTSCALESHIFT;
        }
}
