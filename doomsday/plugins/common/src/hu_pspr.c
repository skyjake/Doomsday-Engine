/* $Id$
 *
 * Copyright (C) 1993-1996 by id Software, Inc.
 *
 * This source is available for distribution and/or modification
 * only under the terms of the DOOM Source Code License as
 * published by id Software. All rights reserved.
 *
 * The source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
 * for more details.
 */

/*
 * Common HUD psprite handling.
 */

// HEADER FILES ------------------------------------------------------------

#if   __WOLFTC__
#  include "wolftc.h"
#elif __JDOOM__
#  include "jdoom.h"
#elif __JHERETIC__
#  include "jheretic.h"
#elif __JHEXEN__
#  include "jhexen.h"
#elif __JSTRIFE__
#  include "jstrife.h
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#if __JHERETIC__
int     PSpriteSY[NUMCLASSES][NUMWEAPONS] = {
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
int     PSpriteSY[NUMCLASSES][NUMWEAPONS] = {
    {0, -12 * FRACUNIT, -10 * FRACUNIT, 10 * FRACUNIT}, // Fighter
    {-8 * FRACUNIT, 10 * FRACUNIT, 10 * FRACUNIT, 0},   // Cleric
    {9 * FRACUNIT, 20 * FRACUNIT, 20 * FRACUNIT, 20 * FRACUNIT},    // Mage
    {10 * FRACUNIT, 10 * FRACUNIT, 10 * FRACUNIT, 10 * FRACUNIT}    // Pig
};
#endif

#if __JSTRIFE__
// Y-adjustment values for full screen (10 weapons)
int     PSpriteSY[NUMCLASSES][NUMWEAPONS] = {
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

    // If the status bar is visible, the sprite is moved up a bit.
    if(Get(DD_VIEWWINDOW_HEIGHT) < 200)
        offy -= FRACUNIT * ((ST_HEIGHT * cfg.sbarscale) / (2 * 20) - 1);
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
        //ddpsp->sprite = psp->state->sprite;
        //ddpsp->frame = psp->state->frame & FF_FRAMEMASK;
        ddpsp->tics = psp->tics;
        // Assume there is no next frame.
        /*ddpsp->nextframe = -1;
           // Is there a next frame?
           if(psp->state->nextstate != S_NULL)
           {
           st = &states[psp->state->nextstate];
           if(st->sprite == ddpsp->sprite)
           {
           // Same sprite, can interpolate.
           ddpsp->nextframe = st->frame & FF_FRAMEMASK;
           ddpsp->nexttime = psp->state->tics;
           }
           } */

        // Choose color and alpha.
        ddpsp->light = 1;
        ddpsp->alpha = 1;

#if __JDOOM__ || __JHERETIC__
        if((pl->powers[pw_invisibility] > 4 * 32) ||
           (pl->powers[pw_invisibility] & 8))
        {
            // shadow draw
            ddpsp->alpha = .25f;
        }
        else
#elif __JHEXEN__
        if(pl->powers[pw_invulnerability] && pl->class == PCLASS_CLERIC)
        {
            if(pl->powers[pw_invulnerability] > 4 * 32)
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
            else if(pl->powers[pw_invulnerability] & 8)
            {
                ddpsp->alpha = .333f;
            }
        }
        else
#endif
        if(psp->state->frame & FF_FULLBRIGHT)
        {
            // full bright
            ddpsp->light = 1;
        }
        else
        {
            // local light
            ddpsp->light =
                P_GetIntp(pl->plr->mo->subsector, DMU_LIGHT_LEVEL) / 255.0;
        }
#if !__JSTRIFE__
        // Needs fullbright?
        if((pl->powers[pw_infrared] > 4 * 32) || (pl->powers[pw_infrared] & 8)
# if __JDOOM__
           || (pl->powers[pw_invulnerability] > 30)
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
        ddpsp->x = FIX2FLT(psp->sx) - lookOffset * 1300;

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

    if(IS_CLIENT)
        return;

    for(i = 0; i < MAXPLAYERS; i++)
        if(players[i].plr->ingame)
            HU_UpdatePlayerSprite(i);
}
