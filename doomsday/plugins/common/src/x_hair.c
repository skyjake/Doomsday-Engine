/**\file x_hair.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * Crosshairs, drawing and config.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <string.h>

#if __JDOOM__
#  include "jdoom.h"
#elif __JDOOM64__
#  include "jdoom64.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#endif

#include "x_hair.h"
#include "p_user.h"
#include "hu_stuff.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CVARs for the crosshair
cvartemplate_t xhairCVars[] = {
    {"view-cross-type", 0, CVT_INT, &cfg.xhair, 0, NUM_XHAIRS},
    {"view-cross-size", 0, CVT_FLOAT, &cfg.xhairSize, 0, 1},
    {"view-cross-vitality", 0, CVT_BYTE, &cfg.xhairVitality, 0, 1},
    {"view-cross-r", 0, CVT_FLOAT, &cfg.xhairColor[0], 0, 1},
    {"view-cross-g", 0, CVT_FLOAT, &cfg.xhairColor[1], 0, 1},
    {"view-cross-b", 0, CVT_FLOAT, &cfg.xhairColor[2], 0, 1},
    {"view-cross-a", 0, CVT_FLOAT, &cfg.xhairColor[3], 0, 1},
    {NULL}
};

// CODE --------------------------------------------------------------------

/**
 * Register CVARs and CCmds for the crosshair.
 */
void X_Register(void)
{
    int i;

    for(i = 0; xhairCVars[i].name; ++i)
        Con_AddVariable(xhairCVars + i);
}

void X_Drawer(int player)
{
#define XHAIR_LINE_WIDTH    1.f

    int xhair = MINMAX_OF(0, cfg.xhair, NUM_XHAIRS), centerX, centerY;
    int winX, winY, winW, winH;
    float alpha, scale, oldLineWidth;
    player_t* plr = &players[player];

    // Is there a crosshair to draw?
    if(xhair == 0) return;

    alpha = MINMAX_OF(0, cfg.xhairColor[3], 1);
    // Dead players are incapable of aiming.
    if(plr->plr->flags & DDPF_DEAD)
    {
        if(plr->rebornWait == 0) return;
        // Make use of the reborn timer to fade out on death.
        if(plr->rebornWait < PLAYER_REBORN_TICS)
        {
            alpha *= (float)plr->rebornWait / PLAYER_REBORN_TICS;
        }
    }

    if(!(alpha > 0))
        return;

    R_ViewWindowDimensions(player, &winX, &winY, &winW, &winH);
    centerX = winX + (winW / 2);
    centerY = winY + (winH / 2);
    scale = .125f + MINMAX_OF(0, cfg.xhairSize, 1) * .125f * winH * ((float)80/SCREENHEIGHT);

    oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
    DGL_SetFloat(DGL_LINE_WIDTH, XHAIR_LINE_WIDTH);

    if(cfg.xhairVitality)
    {   // Color the crosshair according to how close the player is to death.
#define HUE_DEAD            0.f
#define HUE_LIVE            .3f

        float vitalColor[4];

        R_HSVToRGB(vitalColor, HUE_DEAD +
            (HUE_LIVE - HUE_DEAD) * MINMAX_OF(0,
                (float) plr->plr->mo->health / maxHealth, 1), 1, 1);
        vitalColor[3] = alpha;
        DGL_Color4fv(vitalColor);

#undef HUE_DEAD
#undef HUE_LIVE
    }
    else
    {
        float color[4];

        color[0] = MINMAX_OF(0, cfg.xhairColor[0], 1);
        color[1] = MINMAX_OF(0, cfg.xhairColor[1], 1);
        color[2] = MINMAX_OF(0, cfg.xhairColor[2], 1);
        color[3] = alpha;

        DGL_Color4fv(color);
    }

    GL_DrawVectorGraphic2(VG_XHAIR1 + (xhair-1), centerX, centerY, scale);

    // Restore the previous state.
    DGL_SetFloat(DGL_LINE_WIDTH, oldLineWidth);

#undef XHAIR_LINE_WIDTH
}
