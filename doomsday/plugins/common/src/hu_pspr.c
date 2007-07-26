/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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

/*
 * Common HUD psprite handling.
 */

// HEADER FILES ------------------------------------------------------------

#if  __DOOM64TC__
#  include "doom64tc.h"
#elif __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h"
#endif

#include "g_controls.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#if __JHERETIC__
int     PSpriteSY[NUM_PLAYER_CLASSES][NUM_WEAPON_TYPES] = {
// Player
    { 0, 5 * FRACUNIT, 15 * FRACUNIT, 15 * FRACUNIT,
     15 * FRACUNIT, 15 * FRACUNIT, 15 * FRACUNIT, 15 * FRACUNIT},
// Chicken
    {15 * FRACUNIT, 15 * FRACUNIT, 15 * FRACUNIT, 15 * FRACUNIT,
     15 * FRACUNIT, 15 * FRACUNIT, 15 * FRACUNIT, 15 * FRACUNIT}
};
#endif

#if __JHEXEN__
// Y-adjustment values for full screen (4 weapons)
int     PSpriteSY[NUM_PLAYER_CLASSES][NUM_WEAPON_TYPES] = {
    {0, -12 * FRACUNIT, -10 * FRACUNIT, 10 * FRACUNIT}, // Fighter
    {-8 * FRACUNIT, 10 * FRACUNIT, 10 * FRACUNIT, 0},   // Cleric
    {9 * FRACUNIT, 20 * FRACUNIT, 20 * FRACUNIT, 20 * FRACUNIT},    // Mage
    {10 * FRACUNIT, 10 * FRACUNIT, 10 * FRACUNIT, 10 * FRACUNIT}    // Pig
};
#endif

#if __JSTRIFE__
// Y-adjustment values for full screen (10 weapons)
int     PSpriteSY[NUM_PLAYER_CLASSES][NUM_WEAPON_TYPES] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
#endif

// CODE --------------------------------------------------------------------

/*
 * Calculates the Y offset for the player's psprite. The offset depends
 * on the size of the game window.
 */
int HU_PSpriteYOffset(player_t *pl)
{
#if __JDOOM__
    int     offy = FRACUNIT * (cfg.plrViewHeight - 41) * 2;
# if !__DOOM64TC__
    // If the status bar is visible, the sprite is moved up a bit.
    if(Get(DD_VIEWWINDOW_HEIGHT) < 200)
        offy -= FRACUNIT * ((ST_HEIGHT * cfg.sbarscale) / (2 * 20) - 1);
# endif
    return offy;

#else
    if(Get(DD_VIEWWINDOW_HEIGHT) == SCREENHEIGHT)
        return PSpriteSY[pl->class][pl->readyweapon];
    return PSpriteSY[pl->class][pl->readyweapon] * (20 - cfg.sbarscale) / 20.0f -
        FRACUNIT * (39 * cfg.sbarscale) / 40.0f;
#endif
}

/*
 * Calculates the Y offset for the player's psprite. The offset depends
 * on the size of the game window.
 */
void HU_UpdatePlayerSprite(int pnum)
{
    extern float lookOffset;
    int     i;
    pspdef_t *psp;
    ddpsprite_t *ddpsp;
    float   fov = 90;           //*(float*) Con_GetVariable("r_fov")->ptr;
    player_t *pl = players + pnum;

    for(i = 0; i < NUMPSPRITES; i++)
    {
        psp = pl->psprites + i;
        ddpsp = pl->plr->psprites + i;
        if(!psp->state)         // A NULL state?
        {
            //ddpsp->sprite = -1;
            ddpsp->stateptr = 0;
            continue;
        }
        ddpsp->stateptr = psp->state;
        ddpsp->tics = psp->tics;

        // Choose color and alpha.
        ddpsp->light = 1;
        ddpsp->alpha = 1;

#if __JDOOM__ || __JHERETIC__
        if((pl->powers[PT_INVISIBILITY] > 4 * 32) ||
           (pl->powers[PT_INVISIBILITY] & 8))
        {
            // shadow draw
            ddpsp->alpha = .25f;
        }
        else
#elif __JHEXEN__
        if(pl->powers[PT_INVULNERABILITY] && pl->class == PCLASS_CLERIC)
        {
            if(pl->powers[PT_INVULNERABILITY] > 4 * 32)
            {
                if(pl->plr->mo->flags2 & MF2_DONTDRAW)
                {               // don't draw the psprite
                    ddpsp->alpha = .333f;
                }
                else if(pl->plr->mo->flags & MF_SHADOW)
                {
                    ddpsp->alpha = .666f;
                }
            }
            else if(pl->powers[PT_INVULNERABILITY] & 8)
            {
                ddpsp->alpha = .333f;
            }
        }
        else
#endif
        if(psp->state->flags & STF_FULLBRIGHT)
        {
            // full bright
            ddpsp->light = 1;
        }
        else
        {
            // local light
            ddpsp->light =
                P_GetFloatp(pl->plr->mo->subsector, DMU_LIGHT_LEVEL);
        }
#if !__JSTRIFE__
        // Needs fullbright?
        if((pl->powers[PT_INFRARED] > 4 * 32) || (pl->powers[PT_INFRARED] & 8)
# if __JDOOM__
           || (pl->powers[PT_INVULNERABILITY] > 30)
# endif
           )
        {
            // Torch lights up the psprite.
            ddpsp->light = 1;
        }
#endif
        // Add some extra light.
        ddpsp->light += .1f;

        // Offset from center.
        ddpsp->x = FIX2FLT(psp->sx) - G_GetLookOffset(pnum) * 1300;

        if(fov > 90)
            fov = 90;
        ddpsp->y = FIX2FLT(psp->sy) + (90 - fov) / 90 * 80;
    }
}

/*
 * Updates the state of the player sprites (gives their data to the
 * engine so it can render them). Servers handle psprites of all players.
 */
void HU_UpdatePsprites(void)
{
    int     i;

    Set(DD_PSPRITE_OFFSET_Y,
        HU_PSpriteYOffset(players + consoleplayer) >> (FRACBITS - 4));

    //if(IS_CLIENT)
    //  return;

    for(i = 0; i < MAXPLAYERS; i++)
        if(players[i].plr->ingame)
        {
            if(!IS_CLIENT || consoleplayer == i)
            {
                HU_UpdatePlayerSprite(i);
            }
        }
}
