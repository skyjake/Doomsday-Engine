/** @file h_special.cpp  Special color effects for the game view.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "r_special.h"
#include "common.h"

static float appliedFilter[MAXPLAYERS];

#if defined(__JHERETIC__)
static byte ringShader = 0;
#endif

void R_SpecialFilterRegister()
{
#if defined(__JHERETIC__)
    C_VAR_BYTE("rend-ring-effect", &ringShader, 0, 0, 1);
#endif
    R_InitSpecialFilter();
}

void R_InitSpecialFilter()
{
    for(int i = 0; i < MAXPLAYERS; ++i)
    {
        appliedFilter[i] = -1;
    }
}

void R_ClearSpecialFilter(int player, float fadeDuration)
{
    if (appliedFilter[player] > 0)
    {
        DD_Executef(true, "postfx %i opacity 1; postfx %i none %f", player, player, fadeDuration);
        appliedFilter[player] = -1;
    }
}

void R_UpdateSpecialFilterWithTimeDelta(int player, float delta)
{
    const player_t *plr       = players + player;
    const char *    fxName    = "none";
    float           intensity = 1.f;

#if defined(__JDOOM__)
    {
        // In HacX a simple blue shift is used instead.
        if (gameMode == doom2_hacx) return;

        fxName = "monochrome.inverted";

        int filter = plr->powers[PT_INVULNERABILITY];
        if (!filter)
        {
            // Clear the filter.
            R_ClearSpecialFilter(player, delta);
            return;
        }
        if (filter < 4 * 32 && !(filter & 8))
        {
            intensity = 0;
        }
    }
#endif

#if defined(__JHERETIC__)
    {
        delta  = 0; // not animated
        fxName = (ringShader == 0 ? "colorize.gold" : "colorize.inverted.gold");

        const int filter = plr->powers[PT_INVULNERABILITY];
        if (!filter)
        {
            // Clear the filter.
            R_ClearSpecialFilter(player, delta);
            return;
        }
        if (filter <= BLINKTHRESHOLD && !(filter & 8))
        {
            intensity = 0;
        }
    }
#endif

    if (gfw_CurrentGame() == GFW_HEXEN || gfw_CurrentGame() == GFW_DOOM64)
    {
        R_ClearSpecialFilter(player, delta);
        return;
    }

    // Activate the filter; load the shader if the effect was previously not in use.
    if (appliedFilter[player] < 0)
    {
        DD_Executef(true, "postfx %i %s %f", player, fxName, delta);
    }

    // Update filter opacity.
    if (!FEQUAL(appliedFilter[player], intensity))
    {
        DD_Executef(true, "postfx %i opacity %f", player, intensity);
        appliedFilter[player] = intensity;
    }
}

void R_UpdateSpecialFilter(int player)
{
    R_UpdateSpecialFilterWithTimeDelta(player, .3f);
}
