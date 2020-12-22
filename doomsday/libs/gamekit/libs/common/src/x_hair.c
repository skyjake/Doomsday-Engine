/**
 * @file x_hair.c
 * HUD Crosshairs, drawing and config.
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

#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "hu_stuff.h"
#include "p_user.h"
#include "r_common.h"
#include "x_hair.h"


void X_Register(void)
{
    C_VAR_FLOAT("view-cross-angle",     &cfg.common.xhairAngle,        0, 0, 1);
    C_VAR_FLOAT("view-cross-size",      &cfg.common.xhairSize,         0, 0, 1);
    C_VAR_INT  ("view-cross-type",      &cfg.common.xhair,             0, 0, NUM_XHAIRS);
    C_VAR_BYTE ("view-cross-vitality",  &cfg.common.xhairVitality,     0, 0, 1);
    C_VAR_FLOAT("view-cross-r",         &cfg.common.xhairColor[0],     0, 0, 1);
    C_VAR_FLOAT("view-cross-g",         &cfg.common.xhairColor[1],     0, 0, 1);
    C_VAR_FLOAT("view-cross-b",         &cfg.common.xhairColor[2],     0, 0, 1);
    C_VAR_FLOAT("view-cross-a",         &cfg.common.xhairColor[3],     0, 0, 1);
    C_VAR_FLOAT("view-cross-width",     &cfg.common.xhairLineWidth,    0, .5f, 5);
    C_VAR_FLOAT("view-cross-live-r",    &cfg.common.xhairLiveRed,      0, 0, 1);
    C_VAR_FLOAT("view-cross-live-g",    &cfg.common.xhairLiveGreen,    0, 0, 1);
    C_VAR_FLOAT("view-cross-live-b",    &cfg.common.xhairLiveBlue,     0, 0, 1); 
    C_VAR_FLOAT("view-cross-dead-r",    &cfg.common.xhairDeadRed,      0, 0, 1);
    C_VAR_FLOAT("view-cross-dead-g",    &cfg.common.xhairDeadGreen,    0, 0, 1);
    C_VAR_FLOAT("view-cross-dead-b",    &cfg.common.xhairDeadBlue,     0, 0, 1);
}

static float Max3F(float x, float y, float z)
{
    return (x > y) ? (x > z) ? x : z : (y > z) ? y : z;
}

static float Min3F(float x, float y, float z)
{
    return (x < y) ? (x < z) ? x : z : (y < z) ? y : z;
}

static float const RGBtoHue(float const red, float const green, float const blue)
{
    float const rgbMax  = Max3F(red, green, blue);
    float const rgbMin  = Min3F(red, green, blue);
    float const chroma  = rgbMax - rgbMin;

    float hue = 0;

    if(chroma != 0)
    {
        float const deltaRed   = (((rgbMax - red)   / 6.0F) + (chroma / 2.0F)) / chroma;
        float const deltaGreen = (((rgbMax - green) / 6.0F) + (chroma / 2.0F)) / chroma;
        float const deltaBlue  = (((rgbMax - blue)  / 6.0F) + (chroma / 2.0F)) / chroma;

        if(rgbMax == red)
        {
            hue = deltaBlue - deltaGreen;
        } 
        else if(rgbMax == green) 
        {
            hue = (1.0F / 3.0F) + deltaRed - deltaBlue;
        }
        else if(rgbMax == blue)
        {
            hue = (2.0F / 3.0F) + deltaGreen - deltaRed;
        }
    }

    return hue;
}

static dd_bool currentColor(player_t* player, float color[3])
{
    if(!player || !color) return false;

    if(cfg.common.xhairVitality)
    {
        float const liveHue = RGBtoHue(cfg.common.xhairLiveRed, 
                                       cfg.common.xhairLiveGreen,
                                       cfg.common.xhairLiveBlue);
        float const deadHue = RGBtoHue(cfg.common.xhairDeadRed,
                                       cfg.common.xhairDeadGreen,
                                       cfg.common.xhairDeadBlue);

        M_HSVToRGB(color, deadHue + (liveHue - deadHue) 
                          * MINMAX_OF(0, (float) player->plr->mo->health / maxHealth, 1), 1, 1);
    }
    else
    {
        // Custom color.
        color[CR] = MINMAX_OF(0, cfg.common.xhairColor[CR], 1);
        color[CG] = MINMAX_OF(0, cfg.common.xhairColor[CG], 1);
        color[CB] = MINMAX_OF(0, cfg.common.xhairColor[CB], 1);
    }
    return true;
}

static float currentOpacity(player_t* player)
{
    float opacity = MINMAX_OF(0, cfg.common.xhairColor[3], 1);

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
    player_t* player = players + pnum;
    int xhair = MINMAX_OF(0, cfg.common.xhair, NUM_XHAIRS);
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
    scale = .125f + MINMAX_OF(0, cfg.common.xhairSize, 1) * .125f * win.size.height * ((float)80/SCREENHEIGHT);

    oldLineWidth = DGL_GetFloat(DGL_LINE_WIDTH);
    DGL_SetFloat(DGL_LINE_WIDTH, cfg.common.xhairLineWidth);

    currentColor(player, color);
    DGL_Color4fv(color);

    GL_DrawSvg3(VG_XHAIR1 + (xhair-1), &origin, scale, MINMAX_OF(0.f, cfg.common.xhairAngle, 1.f) * 360);

    // Restore the previous state.
    DGL_SetFloat(DGL_LINE_WIDTH, oldLineWidth);
}
