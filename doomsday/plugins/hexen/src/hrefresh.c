/**\file hrefresh.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <string.h>

#include "jhexen.h"

#include "dmu_lib.h"
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

float quitDarkenOpacity = 0;

boolean R_ViewFilterColor(float rgba[4], int filter)
{
    if(!rgba) return false;

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
        Con_Error("R_ViewFilterColor: Strange filter number: %d.\n", filter);
    return false;
}

/**
 * Sets the new palette based upon the current values of
 * player_t->damageCount and player_t->bonusCount.
 */
void R_UpdateViewFilter(int player)
{
    player_t* plr = players + player;
    int palette = 0;

    if(IS_DEDICATED && !player) return;

    if(player < 0 || player >= MAXPLAYERS)
    {
#if _DEBUG
        Con_Message("Warning: R_UpdateViewFilter: Invalid player #%i, ignoring.", player);
#endif
        return;
    }

    // Not currently present?
    if(!plr->plr->inGame) return;

    if(G_GameState() == GS_MAP)
    {
        if(plr->poisonCount)
        {
            palette = 0;
            palette = (plr->poisonCount + 7) >> 3;
            if(palette >= NUMPOISONPALS)
            {
                palette = NUMPOISONPALS - 1;
            }
            palette += STARTPOISONPALS;
        }
        else if(plr->damageCount)
        {
            palette = (plr->damageCount + 7) >> 3;
            if(palette >= NUMREDPALS)
            {
                palette = NUMREDPALS - 1;
            }
            palette += STARTREDPALS;
        }
        else if(plr->bonusCount)
        {
            palette = (plr->bonusCount + 7) >> 3;
            if(palette >= NUMBONUSPALS)
            {
                palette = NUMBONUSPALS - 1;
            }
            palette += STARTBONUSPALS;
        }
        else if(plr->plr->mo->flags2 & MF2_ICEDAMAGE)
        {   // Frozen player
            palette = STARTICEPAL;
        }
    }

    // $democam
    if(palette)
    {
        plr->plr->flags |= DDPF_VIEW_FILTER;
        R_ViewFilterColor(plr->plr->filterColor, palette);
    }
    else
    {
        plr->plr->flags &= ~DDPF_VIEW_FILTER;
    }
}

static void rendPlayerView(int player)
{
    player_t* plr = &players[player];
    boolean special200 = false;
    float pspriteOffsetY;

    if(!plr->plr->mo)
    {
        Con_Message("rendPlayerView: Rendering view of player %i, who has no mobj!", player);
        return;
    }

    if(IS_CLIENT)
    {
        // Server updates mobj flags in NetSv_Ticker.
        R_SetAllDoomsdayFlags();
    }

    // Check for the sector special 200: use sky2.
    // I wonder where this is used?
    if(P_ToXSectorOfBspLeaf(plr->plr->mo->bspLeaf)->special == 200)
    {
        special200 = true;
        R_SkyParams(0, DD_DISABLE, NULL);
        R_SkyParams(1, DD_ENABLE, NULL);
    }

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
        R_SkyParams(0, DD_ENABLE, NULL);
        R_SkyParams(1, DD_DISABLE, NULL);
    }
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

void X_DrawViewPort(int port, const RectRaw* portGeometry,
    const RectRaw* windowGeometry, int player, int layer)
{
    if(layer != 0)
    {
        rendHUD(player, portGeometry);
        return;
    }

    switch(G_GameState())
    {
    case GS_MAP: {
        player_t* plr = players + player;

        if(!ST_AutomapObscures2(player, windowGeometry))
        {
            if(IS_CLIENT && (!Get(DD_GAME_READY) || !Get(DD_GOTFRAME))) return;

            rendPlayerView(player);

            // Crosshair.
            if(!(P_MobjIsCamera(plr->plr->mo) && Get(DD_PLAYBACK))) // $democam
                X_Drawer(player);
        }
        break;
      }
    case GS_STARTUP:
        DGL_DrawRectf2Color(0, 0, portGeometry->size.width, portGeometry->size.height, 0, 0, 0, 1);
        break;

    default: break;
    }
}

void X_DrawWindow(const Size2Raw* windowSize)
{
    if(G_GameState() == GS_INTERMISSION)
    {
        IN_Drawer();
    }

    // Draw HUD displays; menu, messages.
    Hu_Drawer();

    if(G_QuitInProgress())
    {
        DGL_DrawRectf2Color(0, 0, 320, 200, 0, 0, 0, quitDarkenOpacity);
    }
}

void X_EndFrame(void)
{
    int i;

    SN_UpdateActiveSequences();

    if(G_GameState() != GS_MAP) return;

    for(i = 0; i < MAXPLAYERS; ++i)
    {
        player_t* plr = players + i;

        if(!plr->plr->inGame || !plr->plr->mo) continue;

        // View angles are updated with fractional ticks, so we can just use the current values.
        R_SetViewAngle(i, plr->plr->mo->angle + (int) (ANGLE_MAX * -G_GetLookOffset(i)));
        R_SetViewPitch(i, plr->plr->lookDir);
    }
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
            {
                Mobj_UpdateTranslationClassAndMap(mo);
                continue;
            }

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

            Mobj_UpdateTranslationClassAndMap(mo);

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
