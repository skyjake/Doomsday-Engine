/**
 * @file x_hair.c
 * HUD Crosshairs, drawing and config.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "hu_stuff.h"
#include "p_user.h"
#include "r_common.h"
#include "x_hair.h"

void X_Register(void)
{
    C_VAR_FLOAT("view-cross-angle", &cfg.xhairAngle, 0, 0, 1);
    C_VAR_FLOAT("view-cross-size", &cfg.xhairSize, 0, 0, 1);
    C_VAR_INT  ("view-cross-type", &cfg.xhair, 0, 0, NUM_XHAIRS);
    C_VAR_BYTE ("view-cross-vitality", &cfg.xhairVitality, 0, 0, 1);
    C_VAR_FLOAT("view-cross-r", &cfg.xhairColor[0], 0, 0, 1);
    C_VAR_FLOAT("view-cross-g", &cfg.xhairColor[1], 0, 0, 1);
    C_VAR_FLOAT("view-cross-b", &cfg.xhairColor[2], 0, 0, 1);
    C_VAR_FLOAT("view-cross-a", &cfg.xhairColor[3], 0, 0, 1);
}

static boolean currentColor(player_t* player, float color[3])
{
    if(!player || !color) return false;

    if(cfg.xhairVitality)
    {
        // Color the crosshair according to how close the player is to death.
        /// @todo These colors should be cvars.
#define HUE_DEAD            0.f
#define HUE_LIVE            .3f

        M_HSVToRGB(color, HUE_DEAD +
            (HUE_LIVE - HUE_DEAD) * MINMAX_OF(0,
                (float) player->plr->mo->health / maxHealth, 1), 1, 1);

#undef HUE_DEAD
#undef HUE_LIVE
    }
    else
    {
        // Custom color.
        color[CR] = MINMAX_OF(0, cfg.xhairColor[CR], 1);
        color[CG] = MINMAX_OF(0, cfg.xhairColor[CG], 1);
        color[CB] = MINMAX_OF(0, cfg.xhairColor[CB], 1);
    }
    return true;
}

static float currentOpacity(player_t* player)
{
    float opacity = MINMAX_OF(0, cfg.xhairColor[3], 1);

    // Dead players are incapable of aiming so we want to fade out the crosshair on death.
    if(player->plr->flags & DDPF_DEAD)
    {
        // Make use of the reborn timer to implement the fade out.
        if(player->rebornWait <= 0) return 0;
        if(player->rebornWait < PLAYER_REBORN_TICS)
        {
            opacity *= (float)player->rebornWait / PLAYER_REBORN_TICS;
        }
    }

    return opacity;
}

void X_Drawer(int pnum)
{
#define XHAIR_LINE_WIDTH    1.f

    player_t* player = players + pnum;
    int xhair = MINMAX_OF(0, cfg.xhair, NUM_XHAIRS);
    float scale, oldLineWidth, color[4];
    Point2Rawf origin;
    RectRaw win;

    if(pnum < 0 || pnum >= MAXPLAYERS) return;

    // Is there a crosshair to draw?
    if(xhair == 0) return;

    color[CA] = currentOpacity(player);
    if(!(color[CA] > 0)) return;

    R_ViewWindowGeometry(pnum, &win);
    origin.x = win.origin.x + (win.size.width  / 2);
    origin.y = win.origin.y + (win.size.height / 2);
    scale = .125f + MINMAX_OF(0, cfg.xhairSize, 1) * .125f * win.size.height * ((float)80/SCREENHEIGHT);

    oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
    DGL_SetFloat(DGL_LINE_WIDTH, XHAIR_LINE_WIDTH);

    currentColor(player, color);
    DGL_Color4fv(color);

    GL_DrawSvg3(VG_XHAIR1 + (xhair-1), &origin, scale, MINMAX_OF(0.f, cfg.xhairAngle, 1.f) * 360);

    // Restore the previous state.
    DGL_SetFloat(DGL_LINE_WIDTH, oldLineWidth);

#undef XHAIR_LINE_WIDTH
}
