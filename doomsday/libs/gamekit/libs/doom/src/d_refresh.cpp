/** @file d_refresh.cpp  DOOM specific refresh functions/utilities.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "jdoom.h"
#include "d_refresh.h"

#include <cstring>
#include "dmu_lib.h"
#include "hu_menu.h"
#include "hu_stuff.h"
#include "hu_pspr.h"
#include "d_net.h"
#include "g_common.h"
#include "g_controls.h"
#include "gamesession.h"
#include "r_common.h"
#include "p_mapsetup.h"
#include "p_tick.h"
#include "x_hair.h"

float quitDarkenOpacity = 0;

dd_bool R_ViewFilterColor(float rgba[4], int filter)
{
    if(!rgba) return false;

    // We have to choose the right color and alpha.
    if(filter >= STARTREDPALS && filter < STARTREDPALS + NUMREDPALS)
    {
        // Red.
        rgba[CR] = 1;
        rgba[CG] = 0;
        rgba[CB] = 0;
        rgba[CA] = (gfw_Rule(deathmatch)? 1.0f : cfg.common.filterStrength) * (filter+1) / (float)NUMREDPALS;
        return true;
    }

    if(gameMode == doom2_hacx &&
       filter >= STARTINVULPALS && filter < STARTINVULPALS + NUMINVULPALS)
    {
        // Blue.
        rgba[CR] = .16f;
        rgba[CG] = .16f;
        rgba[CB] = .92f;
        rgba[CA] = cfg.common.filterStrength * .98f * (filter - STARTINVULPALS + 1) / (float)NUMINVULPALS;
        return true;
    }

    if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
    {
        if(gameMode == doom2_hacx)
        {
            // The original palette shift desaturates everything evenly.
            // Rather than mess with this right now when we'll be replacing
            // all the filter stuff entirely soon enough - simply use gray.
            rgba[CR] = .5f;
            rgba[CG] = .5f;
            rgba[CB] = .5f;
            rgba[CA] = cfg.common.filterStrength * .25f * (filter - STARTBONUSPALS + 1) / (float)NUMBONUSPALS;
        }
        else
        {
            // Gold.
            rgba[CR] = 1;
            rgba[CG] = .8f;
            rgba[CB] = .5f;
            rgba[CA] = cfg.common.filterStrength * .25f * (filter - STARTBONUSPALS + 1) / (float)NUMBONUSPALS;
        }
        return true;
    }

    if(filter == 13)
    {
        // Green.
        rgba[CR] = 0;
        rgba[CG] = .7f;
        rgba[CB] = 0;
        rgba[CA] = cfg.common.filterStrength * .25f;
        return true;
    }

    if(filter)
    {
        App_Log(DE2_GL_WARNING, "Invalid view filter number: %d", filter);
    }

    return false;
}

void R_UpdateViewFilter(int player)
{
#define RADIATIONPAL            (13) /// Radiation suit, green shift.

    player_t *plr = players + player;
    int palette = 0;

    if(player < 0 || player >= MAXPLAYERS)
        return;

    // Not currently present?
    if(!plr->plr->inGame) return;

    if(gameMode == doom2_hacx && plr->powers[PT_INVULNERABILITY])
    {
        // A blue shift is used in HacX.
        const int max = 10;
        const int cnt = plr->powers[PT_INVULNERABILITY];

        if(cnt < max)
            palette = .5f + (NUMINVULPALS-1) * ((float)cnt / max);
        else if(cnt < 4 * 32 && !(cnt & 8))
            palette = .5f + (NUMINVULPALS-1) * .7f;
        else if(cnt > INVULNTICS - max)
            palette = .5f + (NUMINVULPALS-1) * ((float)(INVULNTICS - cnt) / max);
        else
            palette = NUMINVULPALS-1; // Full shift.

        if(palette >= NUMINVULPALS)
            palette = NUMINVULPALS - 1;
        palette += STARTINVULPALS;
    }
    else
    {
        int cnt = plr->damageCount;

        if(plr->powers[PT_STRENGTH])
        {
            // Slowly fade the berzerk out.
            int bzc = 12 - (plr->powers[PT_STRENGTH] >> 6);
            cnt = MAX_OF(cnt, bzc);
        }

        if(cnt)
        {
            // In Chex Quest the green palette shift is used instead (perhaps to
            // suggest the player is being covered in goo?).
            if(gameMode == doom_chex)
            {
                palette = RADIATIONPAL;
            }
            else
            {
                palette = (cnt + 7) >> 3;
                if(palette >= NUMREDPALS)
                    palette = NUMREDPALS - 1;

                palette += STARTREDPALS;
            }
        }
        else if(plr->bonusCount)
        {
            palette = (plr->bonusCount + 7) >> 3;
            if(palette >= NUMBONUSPALS)
                palette = NUMBONUSPALS - 1;

            palette += STARTBONUSPALS;
        }
        else if(plr->powers[PT_IRONFEET] > 4 * 32 ||
                plr->powers[PT_IRONFEET] & 8)
        {
            palette = RADIATIONPAL;
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

#undef RADIATIONPAL
}

void G_RendPlayerView(int player)
{
    player_t *plr = &players[player];
    float pspriteOffsetY;
    int isFullBright = ((plr->powers[PT_INFRARED] > 4 * 32) ||
                        (plr->powers[PT_INFRARED] & 8) ||
                        plr->powers[PT_INVULNERABILITY] > 30);

    if(IS_CLIENT)
    {
        // Server updates mobj flags in NetSv_Ticker.
        R_SetAllDoomsdayFlags();
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

    // How about fullbright?
    DD_SetInteger(DD_RENDER_FULLBRIGHT, isFullBright);

    // Render the view with possible custom filters.
    R_RenderPlayerView(player);
}

void D_DrawWindow(const Size2Raw * /*windowSize*/)
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

void D_EndFrame()
{
    if (G_GameState() == GS_MAP)
    {
        for (int i = 0; i < MAXPLAYERS; ++i)
        {
            player_t *plr = players + i;

            if (!plr->plr->inGame) continue;
            if (!plr->plr->mo) continue;

            // View angles are updated with fractional ticks, so we can just use the current values.
            R_SetViewAngle(i, Player_ViewYawAngle(i));
            R_SetViewPitch(i, plr->plr->lookDir);
        }
    }
}

void Mobj_UpdateColorMap(mobj_t *mo)
{
    DE_ASSERT(mo != 0);

    if(mo->flags & MF_TRANSLATION)
    {
        mo->tmap = (mo->flags & MF_TRANSLATION) >> MF_TRANSSHIFT;
    }
    else
    {
        mo->tmap = 0;
    }
}

/**
 * Updates the mobj flags used by Doomsday with the state of our local flags
 * for the given mobj.
 */
void P_SetDoomsdayFlags(mobj_t *mo)
{
    DE_ASSERT(mo != 0);

    // Client mobjs can't be set here.
    if(IS_CLIENT && (mo->ddFlags & DDMF_REMOTE))
    {
        // Color translation can be applied for remote mobjs, too.
        Mobj_UpdateColorMap(mo);
        return;
    }

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
    if(mo->info && (mo->info->flags2 & MF2_ALWAYSLIT))
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

    /**
     * The torches often go into the ceiling. This'll prevent them from
     * 'jumping' when they do.
     *
     * @todo Add a thing def flag for this.
     */
    if(mo->type == MT_MISC41 || mo->type == MT_MISC42 || mo->type == MT_MISC43 || // tall torches
       mo->type == MT_MISC44 || mo->type == MT_MISC45 || mo->type == MT_MISC46)  // short torches
        mo->ddFlags |= DDMF_NOFITBOTTOM;

    if(mo->flags & MF_BRIGHTSHADOW)
        mo->ddFlags |= DDMF_BRIGHTSHADOW;
    else if(mo->flags & MF_SHADOW)
        mo->ddFlags |= DDMF_SHADOW;

    if(((mo->flags & MF_VIEWALIGN) && !(mo->flags & MF_MISSILE)) ||
       ((mo->flags & MF_MISSILE) && !(mo->flags & MF_VIEWALIGN)) ||
       (mo->flags & MF_FLOAT))
        mo->ddFlags |= DDMF_VIEWALIGN;

    Mobj_UpdateColorMap(mo);
}

void R_SetAllDoomsdayFlags()
{
    if(G_GameState() != GS_MAP) return;

    // Only visible things are in the sector thinglists, so this is good.
    for(int i = 0; i < numsectors; ++i)
    for(mobj_t *iter = (mobj_t *)P_GetPtr(DMU_SECTOR, i, DMT_MOBJS); iter; iter = iter->sNext)
    {
        P_SetDoomsdayFlags(iter);
    }
}
