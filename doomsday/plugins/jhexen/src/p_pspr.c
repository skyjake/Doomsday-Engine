/**\file
 *\section Copyright and License Summary
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

/*
 * P_pspr.c: Weapon sprite animation.
 *
 * Weapon sprite animation, weapon objects.
 * Action functions for weapons.
 */

// HEADER FILES ------------------------------------------------------------

#include "jhexen.h"

#include "p_player.h"
#include "p_map.h"

// MACROS ------------------------------------------------------------------

#define LOWERSPEED      (FRACUNIT*6)
#define RAISESPEED      (FRACUNIT*6)
#define WEAPONBOTTOM    (128*FRACUNIT)
#define WEAPONTOP       (32*FRACUNIT)

#define ZAGSPEED        (FRACUNIT)
#define MAX_ANGLE_ADJUST (5*ANGLE_1)
#define HAMMER_RANGE    (MELEERANGE+MELEERANGE/2)
#define AXERANGE        (2.25*MELEERANGE)
#define FLAMESPEED      (0.45*FRACUNIT)
#define CFLAMERANGE     (12*64*FRACUNIT)
#define FLAMEROTSPEED   (2*FRACUNIT)

#define SHARDSPAWN_LEFT     1
#define SHARDSPAWN_RIGHT    2
#define SHARDSPAWN_UP       4
#define SHARDSPAWN_DOWN     8

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

extern void P_ExplodeMissile(mobj_t *mo);
extern void C_DECL A_UnHideThing(mobj_t *actor);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern fixed_t FloatBobOffsets[64];

// PUBLIC DATA DEFINITIONS -------------------------------------------------

fixed_t bulletslope;

weaponinfo_t weaponinfo[NUMWEAPONS][NUMCLASSES] = {
    {                           // First Weapons
     {                          // Fighter First Weapon - Punch
     {
      GM_ANY,                   // Gamemode bits
      {0, 0},                   // type: mana1 | mana2
      {0, 0},                   // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_PUNCHUP,                // upstate
      0,                        // raise sound id
      S_PUNCHDOWN,              // downstate
      S_PUNCHREADY,             // readystate
      0,                        // readysound
      S_PUNCHATK1_1,            // atkstate
      S_PUNCHATK1_1,            // holdatkstate
      S_NULL                    // flashstate
      }
     },
     {
     {                          // Cleric First Weapon - Mace
      GM_ANY,                   // Gamemode bits
      {0, 0},                   // type: mana1 | mana2
      {0, 0},                   // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_CMACEUP,                // upstate
      0,                        // raise sound id
      S_CMACEDOWN,              // downstate
      S_CMACEREADY,             // readystate
      0,                        // readysound
      S_CMACEATK_1,             // atkstate
      S_CMACEATK_1,             // holdatkstate
      S_NULL                    // flashstate
      }
      },
     {
     {                          // Mage First Weapon - Wand
      GM_ANY,                   // Gamemode bits
      {0, 0},                   // type: mana1 | mana2
      {0, 0},                   // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_MWANDUP,
      0,                        // raise sound id
      S_MWANDDOWN,
      S_MWANDREADY,
      0,                        // readysound
      S_MWANDATK_1,
      S_MWANDATK_1,
      S_NULL
      }
     },
     {
     {                          // Pig - Snout
      GM_ANY,                   // Gamemode bits
      {0, 0},                   // type: mana1 | mana2
      {0, 0},                   // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_SNOUTUP,                // upstate
      0,                        // raise sound id
      S_SNOUTDOWN,              // downstate
      S_SNOUTREADY,             // readystate
      0,                        // readysound
      S_SNOUTATK1,              // atkstate
      S_SNOUTATK1,              // holdatkstate
      S_NULL                    // flashstate
      }
      }
     },
    {                           // Second Weapons
    {
     {                          // Fighter - Axe
      GM_ANY,                   // Gamemode bits
      {1, 0},                   // type: mana1 | mana2
      {2, 0},                   // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_FAXEUP,                 // upstate
      0,                        // raise sound id
      S_FAXEDOWN,               // downstate
      S_FAXEREADY,              // readystate
      0,                        // readysound
      S_FAXEATK_1,              // atkstate
      S_FAXEATK_1,              // holdatkstate
      S_NULL                    // flashstate
      }
     },
     {
     {                          // Cleric - Serpent Staff
      GM_ANY,                   // Gamemode bits
      {1, 0},                   // type: mana1 | mana2
      {1, 0},                   // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_CSTAFFUP,               // upstate
      0,                        // raise sound id
      S_CSTAFFDOWN,             // downstate
      S_CSTAFFREADY,            // readystate
      0,                        // readysound
      S_CSTAFFATK_1,            // atkstate
      S_CSTAFFATK_1,            // holdatkstate
      S_NULL                    // flashstate
      }
     },
     {
     {                          // Mage - Cone of shards
      GM_ANY,                   // Gamemode bits
      {1, 0},                   // type: mana1 | mana2
      {3, 0},                   // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_CONEUP,                 // upstate
      0,                        // raise sound id
      S_CONEDOWN,               // downstate
      S_CONEREADY,              // readystate
      0,                        // readysound
      S_CONEATK1_1,             // atkstate
      S_CONEATK1_3,             // holdatkstate
      S_NULL                    // flashstate
      }
     },
     {
     {                          // Pig - Snout
      GM_ANY,                   // Gamemode bits
      {0, 0},                   // type: mana1 | mana2
      {0, 0},                   // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_SNOUTUP,                // upstate
      0,                        // raise sound id
      S_SNOUTDOWN,              // downstate
      S_SNOUTREADY,             // readystate
      0,                        // readysound
      S_SNOUTATK1,              // atkstate
      S_SNOUTATK1,              // holdatkstate
      S_NULL                    // flashstate
      }
     }
    },
    {                           // Third Weapons
    {
     {                          // Fighter - Hammer
      GM_ANY,                   // Gamemode bits
      {0, 1},                   // type: mana1 | mana2
      {0, 3},                   // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_FHAMMERUP,              // upstate
      0,                        // raise sound id
      S_FHAMMERDOWN,            // downstate
      S_FHAMMERREADY,           // readystate
      0,                        // readysound
      S_FHAMMERATK_1,           // atkstate
      S_FHAMMERATK_1,           // holdatkstate
      S_NULL                    // flashstate
      }
     },
     {
     {                          // Cleric - Flame Strike
      GM_ANY,                   // Gamemode bits
      {0, 1},                   // type: mana1 | mana2
      {0, 4},                   // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_CFLAMEUP,               // upstate
      0,                        // raise sound id
      S_CFLAMEDOWN,             // downstate
      S_CFLAMEREADY1,           // readystate
      0,                        // readysound
      S_CFLAMEATK_1,            // atkstate
      S_CFLAMEATK_1,            // holdatkstate
      S_NULL                    // flashstate
      }
     },
     {
     {                          // Mage - Lightning
      GM_ANY,                   // Gamemode bits
      {0, 1},                   // type: mana1 | mana2
      {0, 5},                   // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_MLIGHTNINGUP,           // upstate
      0,                        // raise sound id
      S_MLIGHTNINGDOWN,         // downstate
      S_MLIGHTNINGREADY,        // readystate
      0,                        // readysound
      S_MLIGHTNINGATK_1,        // atkstate
      S_MLIGHTNINGATK_1,        // holdatkstate
      S_NULL                    // flashstate
      }
     },
     {
     {                          // Pig - Snout
      GM_ANY,                   // Gamemode bits
      {0, 0},                   // type: mana1 | mana2
      {0, 0},                   // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_SNOUTUP,                // upstate
      0,                        // raise sound id
      S_SNOUTDOWN,              // downstate
      S_SNOUTREADY,             // readystate
      0,                        // readysound
      S_SNOUTATK1,              // atkstate
      S_SNOUTATK1,              // holdatkstate
      S_NULL                    // flashstate
      }
     }
    },
    {                           // Fourth Weapons
     {
     {                          // Fighter - Rune Sword
      GM_ANY,                   // Gamemode bits
      {1, 1},                   // type: mana1 | mana2
      {14, 14},                 // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_FSWORDUP,               // upstate
      0,                        // raise sound id
      S_FSWORDDOWN,             // downstate
      S_FSWORDREADY,            // readystate
      0,                        // readysound
      S_FSWORDATK_1,            // atkstate
      S_FSWORDATK_1,            // holdatkstate
      S_NULL                    // flashstate
      }
     },
     {
     {                          // Cleric - Holy Symbol
      GM_ANY,                   // Gamemode bits
      {1, 1},                   // type: mana1 | mana2
      {18, 18},                 // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_CHOLYUP,                // upstate
      0,                        // raise sound id
      S_CHOLYDOWN,              // downstate
      S_CHOLYREADY,             // readystate
      0,                        // readysound
      S_CHOLYATK_1,             // atkstate
      S_CHOLYATK_1,             // holdatkstate
      S_NULL                    // flashstate
      }
     },
     {
     {                          // Mage - Staff
      GM_ANY,                   // Gamemode bits
      {1, 1},                   // type: mana1 | mana2
      {15, 15},                 // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_MSTAFFUP,               // upstate
      0,                        // raise sound id
      S_MSTAFFDOWN,             // downstate
      S_MSTAFFREADY,            // readystate
      0,                        // readysound
      S_MSTAFFATK_1,            // atkstate
      S_MSTAFFATK_1,            // holdatkstate
      S_NULL                    // flashstate
      }
     },
     {
     {                          // Pig - Snout
      GM_ANY,                   // Gamemode bits
      {0, 0},                   // type: mana1 | mana2
      {0, 0},                   // pershot: mana1 | mana2
      true,                     // autofire when raised if fire held
      S_SNOUTUP,                // upstate
      0,                        // raise sound id
      S_SNOUTDOWN,              // downstate
      S_SNOUTREADY,             // readystate
      0,                        // readysound
      S_SNOUTATK1,              // atkstate
      S_SNOUTATK1,              // holdatkstate
      S_NULL                    // flashstate
      }
     }
    }
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Offset in state->misc1/2.
 */
void P_SetPSpriteOffset(pspdef_t * psp, player_t *player, state_t * state)
{
    ddpsprite_t *ddpsp = player->plr->psprites;

    // Clear the Offset flag by default.
    //ddpsp->flags &= ~DDPSPF_OFFSET;

    if(state->misc[0])
    {
        // Set coordinates.
        psp->sx = state->misc[0] << FRACBITS;
        //ddpsp->flags |= DDPSPF_OFFSET;
        ddpsp->offx = state->misc[0];
    }
    if(state->misc[1])
    {
        psp->sy = state->misc[1] << FRACBITS;
        //ddpsp->flags |= DDPSPF_OFFSET;
        ddpsp->offy = state->misc[1];
    }
}

void P_SetPsprite(player_t *player, int position, statenum_t stnum)
{
    pspdef_t *psp;
    state_t *state;

    psp = &player->psprites[position];
    do
    {
        if(!stnum)
        {                       // Object removed itself.
            psp->state = NULL;
            break;
        }
        state = &states[stnum];
        psp->state = state;
        psp->tics = state->tics;    // could be 0

        P_SetPSpriteOffset(psp, player, state);
        if(state->action)
        {                       // Call action routine.
            state->action(player, psp);
            if(!psp->state)
            {
                break;
            }
        }
        stnum = psp->state->nextstate;
    } while(!psp->tics);        // An initial state of 0 could cycle through.
}

/*
 * Identical to P_SetPsprite, without calling the action function
 */
void P_SetPspriteNF(player_t *player, int position, statenum_t stnum)
{
    pspdef_t *psp;
    state_t *state;

    psp = &player->psprites[position];
    do
    {
        if(!stnum)
        {                       // Object removed itself.
            psp->state = NULL;
            break;
        }
        state = &states[stnum];
        psp->state = state;
        psp->tics = state->tics;    // could be 0

        P_SetPSpriteOffset(psp, player, state);
        stnum = psp->state->nextstate;
    } while(!psp->tics);        // An initial state of 0 could cycle through.
}

void P_ActivateMorphWeapon(player_t *player)
{
    player->pendingweapon = WP_NOCHANGE;
    player->psprites[ps_weapon].sy = WEAPONTOP;
    player->readyweapon = WP_FIRST; // Snout is the first weapon
    player->update |= PSF_WEAPONS;
    P_SetPsprite(player, ps_weapon, S_SNOUTREADY);
}

void P_PostMorphWeapon(player_t *player, weapontype_t weapon)
{
    player->pendingweapon = WP_NOCHANGE;
    player->readyweapon = weapon;
    player->psprites[ps_weapon].sy = WEAPONBOTTOM;
    player->update |= PSF_WEAPONS;
    P_SetPsprite(player, ps_weapon, weaponinfo[weapon][player->class].mode[0].upstate);
}

/*
 * Starts bringing the pending weapon up from the bottom of the screen.
 */
void P_BringUpWeapon(player_t *player)
{
    statenum_t newState;
    weaponmodeinfo_t *wminfo;

    wminfo = WEAPON_INFO(player->pendingweapon, player->class, 0);

    newState = wminfo->upstate;
    if(player->class == PCLASS_FIGHTER && player->pendingweapon == WP_SECOND &&
       player->ammo[MANA_1])
    {
        newState = S_FAXEUP_G;
    }

    if(player->pendingweapon == WP_NOCHANGE)
        player->pendingweapon = player->readyweapon;

    if(wminfo->raisesound)
        S_StartSound(wminfo->raisesound, player->plr->mo);

    player->pendingweapon = WP_NOCHANGE;
    player->psprites[ps_weapon].sy = WEAPONBOTTOM;
    P_SetPsprite(player, ps_weapon, newState);
}

/**
 * Checks if there is enough ammo to shoot with the current weapon. If not,
 * a weapon change event is dispatched (which may or may not do anything
 * depending on the player's config).
 *
 * @returns             <code>true</code> if there is enough mana to shoot.
 */
boolean P_CheckAmmo(player_t *player)
{
    ammotype_t i;
    int     count;
    boolean good;

    // KLUDGE: Work around the multiple firing modes problems.
    // We need to split the weapon firing routines and implement them as
    // new fire modes.
    if(player->class == PCLASS_FIGHTER || player->readyweapon != WP_FOURTH)
        return true;
    // < KLUDGE

    // Check we have enough of ALL ammo types used by this weapon.
    good = true;

    for(i=0; i < NUMAMMO && good; ++i)
    {
        if(!weaponinfo[player->readyweapon][player->class].mode[0].ammotype[i])
            continue; // Weapon does not take this type of ammo.

        // Minimal amount for one shot varies.
        count = weaponinfo[player->readyweapon][player->class].mode[0].pershot[i];

        // Return if current ammunition sufficient.
        if(player->ammo[i] < count)
        {
            good = false;
        }
    }
    if(good)
        return true;

    // Out of ammo, pick a weapon to change to.
    P_MaybeChangeWeapon(player, WP_NOCHANGE, AM_NOAMMO, false);

    // Now set appropriate weapon overlay.
    P_SetPsprite(player, ps_weapon,
                 weaponinfo[player->readyweapon][player->class].mode[0].downstate);
    return false;
}

void P_FireWeapon(player_t *player)
{
    statenum_t attackState;

    if(!P_CheckAmmo(player))
        return;

    // Psprite state.
    P_SetMobjState(player->plr->mo, PCLASS_INFO(player->class)->attackstate);
    if(player->class == PCLASS_FIGHTER && player->readyweapon == WP_SECOND &&
       player->ammo[MANA_1] > 0)
    {                           // Glowing axe
        attackState = S_FAXEATK_G1;
    }
    else
    {
        if(player->refire)
            attackState =
                weaponinfo[player->readyweapon][player->class].mode[0].holdatkstate;
        else
            attackState =
                weaponinfo[player->readyweapon][player->class].mode[0].atkstate;
    }

    P_SetPsprite(player, ps_weapon, attackState);
    P_NoiseAlert(player->plr->mo, player->plr->mo);

    player->update |= PSF_AMMO;

    // Psprite state.
    player->plr->psprites[0].state = DDPSP_FIRE;
}

/*
 * The player died, so put the weapon away.
 */
void P_DropWeapon(player_t *player)
{
    P_SetPsprite(player, ps_weapon,
                 weaponinfo[player->readyweapon][player->class].mode[0].downstate);
}

/*
 * The player can fire the weapon or change to another weapon at this time.
 */
void C_DECL A_WeaponReady(player_t *player, pspdef_t * psp)
{
    weaponmodeinfo_t *wminfo;
    ddpsprite_t *ddpsp;

    // Change player from attack state
    if(player->plr->mo->state >= &states[PCLASS_INFO(player->class)->attackstate] &&
       player->plr->mo->state <= &states[PCLASS_INFO(player->class)->attackendstate])
    {
        P_SetMobjState(player->plr->mo, PCLASS_INFO(player->class)->normalstate);
    }

    if(player->readyweapon != WP_NOCHANGE)
    {
        wminfo = WEAPON_INFO(player->readyweapon, player->class, 0);

        // A weaponready sound?
        if(psp->state == &states[wminfo->readystate] && wminfo->readysound)
            S_StartSound(wminfo->readysound, player->plr->mo);

        // check for change
        //  if player is dead, put the weapon away
        if(player->pendingweapon != WP_NOCHANGE || !player->health)
        {   //  (pending weapon should allready be validated)
            P_SetPsprite(player, ps_weapon, wminfo->downstate);
            return;
        }
    }

    // check for autofire
    if(player->cmd.attack)
    {
        wminfo = WEAPON_INFO(player->readyweapon, player->class, 0);

        if(!player->attackdown || wminfo->autofire)
        {
            player->attackdown = true;
            P_FireWeapon(player);
            return;
        }
    }
    else
        player->attackdown = false;

    ddpsp = player->plr->psprites;

    if(!player->morphTics)
    {
        // Bob the weapon based on movement speed.
        psp->sx = G_GetInteger(DD_PSPRITE_BOB_X);
        psp->sy = G_GetInteger(DD_PSPRITE_BOB_Y);

        ddpsp->offx = ddpsp->offy = 0;
    }

    // Psprite state.
    ddpsp->state = DDPSP_BOBBING;
}

/*
 * The player can re fire the weapon without lowering it entirely.
 */
void C_DECL A_ReFire(player_t *player, pspdef_t * psp)
{
    if(player->cmd.attack &&
       player->pendingweapon == WP_NOCHANGE &&
       player->health)
    {
        player->refire++;
        P_FireWeapon(player);
    }
    else
    {
        player->refire = 0;
        P_CheckAmmo(player);
    }
}

void C_DECL A_Lower(player_t *player, pspdef_t * psp)
{
    // Psprite state.
    player->plr->psprites[0].state = DDPSP_DOWN;

    if(player->morphTics)
    {
        psp->sy = WEAPONBOTTOM;
    }
    else
    {
        psp->sy += LOWERSPEED;
    }
    if(psp->sy < WEAPONBOTTOM)
    {                           // Not lowered all the way yet
        return;
    }
    if(player->playerstate == PST_DEAD)
    {                           // Player is dead, so don't bring up a pending weapon
        psp->sy = WEAPONBOTTOM;
        return;
    }
    if(!player->health)
    {                           // Player is dead, so keep the weapon off screen
        P_SetPsprite(player, ps_weapon, S_NULL);
        return;
    }
    player->readyweapon = player->pendingweapon;
    player->update |= PSF_WEAPONS;
    P_BringUpWeapon(player);
}

void C_DECL A_Raise(player_t *player, pspdef_t * psp)
{
    // Psprite state.
    player->plr->psprites[0].state = DDPSP_UP;

    psp->sy -= RAISESPEED;
    if(psp->sy > WEAPONTOP)
    {                           // Not raised all the way yet
        return;
    }
    psp->sy = WEAPONTOP;
    if(player->class == PCLASS_FIGHTER && player->readyweapon == WP_SECOND &&
       player->ammo[MANA_1])
    {
        P_SetPsprite(player, ps_weapon, S_FAXEREADY_G);
    }
    else
    {
        P_SetPsprite(player, ps_weapon,
                     weaponinfo[player->readyweapon][player->class].mode[0].
                     readystate);
    }
}

void AdjustPlayerAngle(mobj_t *pmo)
{
    angle_t angle;
    int     difference;

    angle = R_PointToAngle2(pmo->pos[VX], pmo->pos[VY],
                            linetarget->pos[VX], linetarget->pos[VY]);
    difference = (int) angle - (int) pmo->angle;
    if(abs(difference) > MAX_ANGLE_ADJUST)
    {
        pmo->angle += difference > 0 ? MAX_ANGLE_ADJUST : -MAX_ANGLE_ADJUST;
    }
    else
    {
        pmo->angle = angle;
    }
    pmo->player->plr->flags |= DDPF_FIXANGLES;
}

void C_DECL A_SnoutAttack(player_t *player, pspdef_t * psp)
{
    angle_t angle;
    int     damage;
    int     slope;

    damage = 3 + (P_Random() & 3);
    angle = player->plr->mo->angle;
    slope = P_AimLineAttack(player->plr->mo, angle, MELEERANGE);
    PuffType = MT_SNOUTPUFF;
    PuffSpawned = NULL;
    P_LineAttack(player->plr->mo, angle, MELEERANGE, slope, damage);
    S_StartSound(SFX_PIG_ACTIVE1 + (P_Random() & 1), player->plr->mo);
    if(linetarget)
    {
        AdjustPlayerAngle(player->plr->mo);

        if(PuffSpawned)
        {                       // Bit something
            S_StartSound(SFX_PIG_ATTACK, player->plr->mo);
        }
    }
}

void C_DECL A_FHammerAttack(player_t *player, pspdef_t * psp)
{
    angle_t angle;
    mobj_t *pmo = player->plr->mo;
    int     damage;
    fixed_t power;
    int     slope;
    int     i;

    damage = 60 + (P_Random() & 63);
    power = 10 * FRACUNIT;
    PuffType = MT_HAMMERPUFF;
    for(i = 0; i < 16; i++)
    {
        angle = pmo->angle + i * (ANG45 / 32);
        slope = P_AimLineAttack(pmo, angle, HAMMER_RANGE);
        if(linetarget)
        {
            P_LineAttack(pmo, angle, HAMMER_RANGE, slope, damage);
            AdjustPlayerAngle(pmo);
            if(linetarget->flags & MF_COUNTKILL || linetarget->player)
            {
                P_ThrustMobj(linetarget, angle, power);
            }
            pmo->special1 = false;  // Don't throw a hammer
            goto hammerdone;
        }
        angle = pmo->angle - i * (ANG45 / 32);
        slope = P_AimLineAttack(pmo, angle, HAMMER_RANGE);
        if(linetarget)
        {
            P_LineAttack(pmo, angle, HAMMER_RANGE, slope, damage);
            AdjustPlayerAngle(pmo);
            if(linetarget->flags & MF_COUNTKILL || linetarget->player)
            {
                P_ThrustMobj(linetarget, angle, power);
            }
            pmo->special1 = false;  // Don't throw a hammer
            goto hammerdone;
        }
    }
    // didn't find any targets in meleerange, so set to throw out a hammer
    PuffSpawned = NULL;
    angle = pmo->angle;
    slope = P_AimLineAttack(pmo, angle, HAMMER_RANGE);
    P_LineAttack(pmo, angle, HAMMER_RANGE, slope, damage);
    if(PuffSpawned)
    {
        pmo->special1 = false;
    }
    else
    {
        pmo->special1 = true;
    }
  hammerdone:
    if(player->ammo[MANA_2] <
       weaponinfo[player->readyweapon][player->class].mode[0].pershot[MANA_2])
    {   // Don't spawn a hammer if the player doesn't have enough mana
        pmo->special1 = false;
    }
    return;
}

void C_DECL A_FHammerThrow(player_t *player, pspdef_t * psp)
{
    mobj_t *mo;

    if(!player->plr->mo->special1)
        return;

    P_ShotAmmo(player);

    mo = P_SpawnPlayerMissile(player->plr->mo, MT_HAMMER_MISSILE);
    if(mo)
        mo->special1 = 0;
}

void C_DECL A_FSwordAttack(player_t *player, pspdef_t * psp)
{
    mobj_t *pmo;

    P_ShotAmmo(player);

    pmo = player->plr->mo;
    P_SPMAngleXYZ(pmo, pmo->pos[VX], pmo->pos[VY], pmo->pos[VZ] - 10 * FRACUNIT,
                  MT_FSWORD_MISSILE, pmo->angle + ANG45 / 4);
    P_SPMAngleXYZ(pmo, pmo->pos[VX], pmo->pos[VY], pmo->pos[VZ] - 5 * FRACUNIT,
                  MT_FSWORD_MISSILE, pmo->angle + ANG45 / 8);
    P_SPMAngleXYZ(pmo, pmo->pos[VX], pmo->pos[VY], pmo->pos[VZ],
                  MT_FSWORD_MISSILE, pmo->angle);
    P_SPMAngleXYZ(pmo, pmo->pos[VX], pmo->pos[VY], pmo->pos[VZ] + 5 * FRACUNIT,
                  MT_FSWORD_MISSILE, pmo->angle - ANG45 / 8);
    P_SPMAngleXYZ(pmo, pmo->pos[VX], pmo->pos[VY], pmo->pos[VZ] + 10 * FRACUNIT,
                  MT_FSWORD_MISSILE, pmo->angle - ANG45 / 4);
    S_StartSound(SFX_FIGHTER_SWORD_FIRE, pmo);
}

void C_DECL A_FSwordAttack2(mobj_t *actor)
{
    angle_t angle = actor->angle;

    P_SpawnMissileAngle(actor, MT_FSWORD_MISSILE, angle + ANG45 / 4, 0);
    P_SpawnMissileAngle(actor, MT_FSWORD_MISSILE, angle + ANG45 / 8, 0);
    P_SpawnMissileAngle(actor, MT_FSWORD_MISSILE, angle, 0);
    P_SpawnMissileAngle(actor, MT_FSWORD_MISSILE, angle - ANG45 / 8, 0);
    P_SpawnMissileAngle(actor, MT_FSWORD_MISSILE, angle - ANG45 / 4, 0);
    S_StartSound(SFX_FIGHTER_SWORD_FIRE, actor);
}

void C_DECL A_FSwordFlames(mobj_t *actor)
{
    int     i;

    for(i = 1 + (P_Random() & 3); i; i--)
    {
        P_SpawnMobj(actor->pos[VX] + ((P_Random() - 128) << 12),
                    actor->pos[VY] + ((P_Random() - 128) << 12),
                    actor->pos[VZ] + ((P_Random() - 128) << 11), MT_FSWORD_FLAME);
    }
}

void C_DECL A_MWandAttack(player_t *player, pspdef_t * psp)
{
    mobj_t *mo;

    mo = P_SpawnPlayerMissile(player->plr->mo, MT_MWAND_MISSILE);
    if(mo)
    {
        mo->thinker.function = P_BlasterMobjThinker;
    }
    S_StartSound(SFX_MAGE_WAND_FIRE, player->plr->mo);
}

void C_DECL A_LightningReady(player_t *player, pspdef_t * psp)
{
    A_WeaponReady(player, psp);
    if(P_Random() < 160)
    {
        S_StartSound(SFX_MAGE_LIGHTNING_READY, player->plr->mo);
    }
}

void C_DECL A_LightningClip(mobj_t *actor)
{
    mobj_t *cMo;
    mobj_t *target = 0;
    int     zigZag;

    if(actor->type == MT_LIGHTNING_FLOOR)
    {
        actor->pos[VZ] = actor->floorz;
        target = actor->lastenemy->tracer;
    }
    else if(actor->type == MT_LIGHTNING_CEILING)
    {
        actor->pos[VZ] = actor->ceilingz - actor->height;
        target = actor->tracer;
    }
    if(actor->type == MT_LIGHTNING_FLOOR)
    {   // floor lightning zig-zags, and forces the ceiling lightning to mimic
        cMo = actor->lastenemy;
        zigZag = P_Random();
        if((zigZag > 128 && actor->special1 < 2) || actor->special1 < -2)
        {
            P_ThrustMobj(actor, actor->angle + ANG90, ZAGSPEED);
            if(cMo)
            {
                P_ThrustMobj(cMo, actor->angle + ANG90, ZAGSPEED);
            }
            actor->special1++;
        }
        else
        {
            P_ThrustMobj(actor, actor->angle - ANG90, ZAGSPEED);
            if(cMo)
            {
                P_ThrustMobj(cMo, cMo->angle - ANG90, ZAGSPEED);
            }
            actor->special1--;
        }
    }
    if(target)
    {
        if(target->health <= 0)
        {
            P_ExplodeMissile(actor);
        }
        else
        {
            actor->angle = R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                                           target->pos[VX], target->pos[VY]);
            actor->momx = 0;
            actor->momy = 0;
            P_ThrustMobj(actor, actor->angle, actor->info->speed >> 1);
        }
    }
}

void C_DECL A_LightningZap(mobj_t *actor)
{
    mobj_t *mo;
    fixed_t deltaZ;

    A_LightningClip(actor);

    actor->health -= 8;
    if(actor->health <= 0)
    {
        P_SetMobjState(actor, actor->info->deathstate);
        return;
    }
    if(actor->type == MT_LIGHTNING_FLOOR)
    {
        deltaZ = 10 * FRACUNIT;
    }
    else
    {
        deltaZ = -10 * FRACUNIT;
    }
    mo = P_SpawnMobj(actor->pos[VX] + ((P_Random() - 128) * actor->radius / 256),
                     actor->pos[VY] + ((P_Random() - 128) * actor->radius / 256),
                     actor->pos[VZ] + deltaZ, MT_LIGHTNING_ZAP);
    if(mo)
    {
        mo->lastenemy = actor;
        mo->momx = actor->momx;
        mo->momy = actor->momy;
        mo->target = actor->target;
        if(actor->type == MT_LIGHTNING_FLOOR)
        {
            mo->momz = 20 * FRACUNIT;
        }
        else
        {
            mo->momz = -20 * FRACUNIT;
        }
    }

    if(actor->type == MT_LIGHTNING_FLOOR && P_Random() < 160)
    {
        S_StartSound(SFX_MAGE_LIGHTNING_CONTINUOUS, actor);
    }
}

void C_DECL A_MLightningAttack2(mobj_t *actor)
{
    mobj_t *fmo, *cmo;

    fmo = P_SpawnPlayerMissile(actor, MT_LIGHTNING_FLOOR);
    cmo = P_SpawnPlayerMissile(actor, MT_LIGHTNING_CEILING);
    if(fmo)
    {
        fmo->special1 = 0;
        fmo->lastenemy = cmo;
        A_LightningZap(fmo);
    }
    if(cmo)
    {
        cmo->tracer = NULL;      // mobj that it will track
        cmo->lastenemy = fmo;
        A_LightningZap(cmo);
    }
    S_StartSound(SFX_MAGE_LIGHTNING_FIRE, actor);
}

void C_DECL A_MLightningAttack(player_t *player, pspdef_t * psp)
{
    A_MLightningAttack2(player->plr->mo);
    P_ShotAmmo(player);
}

void C_DECL A_ZapMimic(mobj_t *actor)
{
    mobj_t *mo;

    mo = actor->lastenemy;
    if(mo)
    {
        if(mo->state >= &states[mo->info->deathstate] ||
           mo->state == &states[S_FREETARGMOBJ])
        {
            P_ExplodeMissile(actor);
        }
        else
        {
            actor->momx = mo->momx;
            actor->momy = mo->momy;
        }
    }
}

void C_DECL A_LastZap(mobj_t *actor)
{
    mobj_t *mo;

    mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_LIGHTNING_ZAP);
    if(mo)
    {
        P_SetMobjState(mo, S_LIGHTNING_ZAP_X1);
        mo->momz = 40 * FRACUNIT;
    }
}

void C_DECL A_LightningRemove(mobj_t *actor)
{
    mobj_t *mo;

    mo = actor->lastenemy;
    if(mo)
    {
        mo->lastenemy = NULL;
        P_ExplodeMissile(mo);
    }
}

void MStaffSpawn(mobj_t *pmo, angle_t angle)
{
    mobj_t *mo;

    mo = P_SPMAngle(pmo, MT_MSTAFF_FX2, angle);
    if(mo)
    {
        mo->target = pmo;
        mo->tracer = P_RoughMonsterSearch(mo, 10);
    }
}

void C_DECL A_MStaffAttack(player_t *player, pspdef_t * psp)
{
    angle_t angle;
    mobj_t *pmo;

    P_ShotAmmo(player);
    pmo = player->plr->mo;
    angle = pmo->angle;

    MStaffSpawn(pmo, angle);
    MStaffSpawn(pmo, angle - ANGLE_1 * 5);
    MStaffSpawn(pmo, angle + ANGLE_1 * 5);
    S_StartSound(SFX_MAGE_STAFF_FIRE, player->plr->mo);
    if(player == &players[consoleplayer])
    {
        player->damagecount = 0;
        player->bonuscount = 0;

        R_SetFilter(STARTSCOURGEPAL);
    }
}

void C_DECL A_MStaffPalette(player_t *player, pspdef_t * psp)
{
    int     pal;

    if(player == &players[consoleplayer])
    {
        pal = STARTSCOURGEPAL + psp->state - (&states[S_MSTAFFATK_2]);
        if(pal == STARTSCOURGEPAL + 3)
        {                       // reset back to original playpal
            pal = 0;
        }

        R_SetFilter(pal);
    }
}

void C_DECL A_MStaffWeave(mobj_t *actor)
{
    fixed_t newX, newY;
    int     weaveXY, weaveZ;
    int     angle;

    weaveXY = actor->special2 >> 16;
    weaveZ = actor->special2 & 0xFFFF;
    angle = (actor->angle + ANG90) >> ANGLETOFINESHIFT;
    newX =
        actor->pos[VX] - FixedMul(finecosine[angle], FloatBobOffsets[weaveXY] << 2);
    newY = actor->pos[VY] - FixedMul(finesine[angle], FloatBobOffsets[weaveXY] << 2);
    weaveXY = (weaveXY + 6) & 63;
    newX += FixedMul(finecosine[angle], FloatBobOffsets[weaveXY] << 2);
    newY += FixedMul(finesine[angle], FloatBobOffsets[weaveXY] << 2);
    P_TryMove(actor, newX, newY);
    actor->pos[VZ] -= FloatBobOffsets[weaveZ] << 1;
    weaveZ = (weaveZ + 3) & 63;
    actor->pos[VZ] += FloatBobOffsets[weaveZ] << 1;
    if(actor->pos[VZ] <= actor->floorz)
    {
        actor->pos[VZ] = actor->floorz + FRACUNIT;
    }
    actor->special2 = weaveZ + (weaveXY << 16);
}

void C_DECL A_MStaffTrack(mobj_t *actor)
{
    if((actor->tracer == 0) && (P_Random() < 50))
    {
        actor->tracer = P_RoughMonsterSearch(actor, 10);
    }
    P_SeekerMissile(actor, ANGLE_1 * 2, ANGLE_1 * 10);
}

/*
 * for use by mage class boss
 */
void MStaffSpawn2(mobj_t *actor, angle_t angle)
{
    mobj_t *mo;

    mo = P_SpawnMissileAngle(actor, MT_MSTAFF_FX2, angle, 0);
    if(mo)
    {
        mo->target = actor;
        mo->tracer = P_RoughMonsterSearch(mo, 10);
    }
}

/*
 * for use by mage class boss
 */
void C_DECL A_MStaffAttack2(mobj_t *actor)
{
    angle_t angle;

    angle = actor->angle;
    MStaffSpawn2(actor, angle);
    MStaffSpawn2(actor, angle - ANGLE_1 * 5);
    MStaffSpawn2(actor, angle + ANGLE_1 * 5);
    S_StartSound(SFX_MAGE_STAFF_FIRE, actor);
}

void C_DECL A_FPunchAttack(player_t *player, pspdef_t * psp)
{
    angle_t angle;
    int     damage;
    int     slope;
    mobj_t *pmo = player->plr->mo;
    fixed_t power;
    int     i;

    damage = 40 + (P_Random() & 15);
    power = 2 * FRACUNIT;
    PuffType = MT_PUNCHPUFF;
    for(i = 0; i < 16; i++)
    {
        angle = pmo->angle + i * (ANG45 / 16);
        slope = P_AimLineAttack(pmo, angle, 2 * MELEERANGE);
        if(linetarget)
        {
            player->plr->mo->special1++;
            if(pmo->special1 == 3)
            {
                damage <<= 1;
                power = 6 * FRACUNIT;
                PuffType = MT_HAMMERPUFF;
            }
            P_LineAttack(pmo, angle, 2 * MELEERANGE, slope, damage);
            if(linetarget->flags & MF_COUNTKILL || linetarget->player)
            {
                P_ThrustMobj(linetarget, angle, power);
            }
            AdjustPlayerAngle(pmo);
            goto punchdone;
        }
        angle = pmo->angle - i * (ANG45 / 16);
        slope = P_AimLineAttack(pmo, angle, 2 * MELEERANGE);
        if(linetarget)
        {
            pmo->special1++;
            if(pmo->special1 == 3)
            {
                damage <<= 1;
                power = 6 * FRACUNIT;
                PuffType = MT_HAMMERPUFF;
            }
            P_LineAttack(pmo, angle, 2 * MELEERANGE, slope, damage);
            if(linetarget->flags & MF_COUNTKILL || linetarget->player)
            {
                P_ThrustMobj(linetarget, angle, power);
            }
            AdjustPlayerAngle(pmo);
            goto punchdone;
        }
    }
    // didn't find any creatures, so try to strike any walls
    pmo->special1 = 0;

    angle = pmo->angle;
    slope = P_AimLineAttack(pmo, angle, MELEERANGE);
    P_LineAttack(pmo, angle, MELEERANGE, slope, damage);

  punchdone:
    if(pmo->special1 == 3)
    {
        pmo->special1 = 0;
        P_SetPsprite(player, ps_weapon, S_PUNCHATK2_1);
        S_StartSound(SFX_FIGHTER_GRUNT, pmo);
    }
    return;
}

void C_DECL A_FAxeAttack(player_t *player, pspdef_t * psp)
{
    angle_t angle;
    mobj_t *pmo = player->plr->mo;
    fixed_t power;
    int     damage;
    int     slope;
    int     i;
    int     useMana;

    damage = 40 + (P_Random() & 15) + (P_Random() & 7);
    power = 0;
    if(player->ammo[MANA_1] > 0)
    {
        damage <<= 1;
        power = 6 * FRACUNIT;
        PuffType = MT_AXEPUFF_GLOW;
        useMana = 1;
    }
    else
    {
        PuffType = MT_AXEPUFF;
        useMana = 0;
    }
    for(i = 0; i < 16; i++)
    {
        angle = pmo->angle + i * (ANG45 / 16);
        slope = P_AimLineAttack(pmo, angle, AXERANGE);
        if(linetarget)
        {
            P_LineAttack(pmo, angle, AXERANGE, slope, damage);
            if(linetarget->flags & MF_COUNTKILL || linetarget->player)
            {
                P_ThrustMobj(linetarget, angle, power);
            }
            AdjustPlayerAngle(pmo);
            useMana++;
            goto axedone;
        }
        angle = pmo->angle - i * (ANG45 / 16);
        slope = P_AimLineAttack(pmo, angle, AXERANGE);
        if(linetarget)
        {
            P_LineAttack(pmo, angle, AXERANGE, slope, damage);
            if(linetarget->flags & MF_COUNTKILL)
            {
                P_ThrustMobj(linetarget, angle, power);
            }
            AdjustPlayerAngle(pmo);
            useMana++;
            goto axedone;
        }
    }
    // didn't find any creatures, so try to strike any walls
    pmo->special1 = 0;

    angle = pmo->angle;
    slope = P_AimLineAttack(pmo, angle, MELEERANGE);
    P_LineAttack(pmo, angle, MELEERANGE, slope, damage);

  axedone:
    if(useMana == 2)
    {
        P_ShotAmmo(player);
        if(player->ammo[MANA_1] <= 0)
            P_SetPsprite(player, ps_weapon, S_FAXEATK_5);
    }
    return;
}

void C_DECL A_CMaceAttack(player_t *player, pspdef_t * psp)
{
    angle_t angle;
    int     damage;
    int     slope;
    int     i;

    damage = 25 + (P_Random() & 15);
    PuffType = MT_HAMMERPUFF;
    for(i = 0; i < 16; i++)
    {
        angle = player->plr->mo->angle + i * (ANG45 / 16);
        slope = P_AimLineAttack(player->plr->mo, angle, 2 * MELEERANGE);
        if(linetarget)
        {
            P_LineAttack(player->plr->mo, angle, 2 * MELEERANGE, slope,
                         damage);
            AdjustPlayerAngle(player->plr->mo);
            goto macedone;
        }
        angle = player->plr->mo->angle - i * (ANG45 / 16);
        slope = P_AimLineAttack(player->plr->mo, angle, 2 * MELEERANGE);
        if(linetarget)
        {
            P_LineAttack(player->plr->mo, angle, 2 * MELEERANGE, slope,
                         damage);
            AdjustPlayerAngle(player->plr->mo);
            goto macedone;
        }
    }
    // didn't find any creatures, so try to strike any walls
    player->plr->mo->special1 = 0;

    angle = player->plr->mo->angle;
    slope = P_AimLineAttack(player->plr->mo, angle, MELEERANGE);
    P_LineAttack(player->plr->mo, angle, MELEERANGE, slope, damage);
  macedone:
    return;
}

void C_DECL A_CStaffCheck(player_t *player, pspdef_t * psp)
{
    mobj_t *pmo;
    int     damage;
    int     newLife;
    angle_t angle;
    int     slope;
    int     i;

    pmo = player->plr->mo;
    damage = 20 + (P_Random() & 15);
    PuffType = MT_CSTAFFPUFF;
    for(i = 0; i < 3; i++)
    {
        angle = pmo->angle + i * (ANG45 / 16);
        slope = P_AimLineAttack(pmo, angle, 1.5 * MELEERANGE);
        if(linetarget)
        {
            P_LineAttack(pmo, angle, 1.5 * MELEERANGE, slope, damage);
            pmo->angle = R_PointToAngle2(pmo->pos[VX], pmo->pos[VY],
                                         linetarget->pos[VX], linetarget->pos[VY]);
            if((linetarget->player || linetarget->flags & MF_COUNTKILL) &&
               (!(linetarget->flags2 & (MF2_DORMANT + MF2_INVULNERABLE))))
            {
                newLife = player->health + (damage >> 3);
                newLife = newLife > 100 ? 100 : newLife;
                pmo->health = player->health = newLife;
                P_SetPsprite(player, ps_weapon, S_CSTAFFATK2_1);
            }
            P_ShotAmmo(player);
            break;
        }
        angle = pmo->angle - i * (ANG45 / 16);
        slope = P_AimLineAttack(player->plr->mo, angle, 1.5 * MELEERANGE);
        if(linetarget)
        {
            P_LineAttack(pmo, angle, 1.5 * MELEERANGE, slope, damage);
            pmo->angle = R_PointToAngle2(pmo->pos[VX], pmo->pos[VY],
                                         linetarget->pos[VX], linetarget->pos[VY]);
            if(linetarget->player || linetarget->flags & MF_COUNTKILL)
            {
                newLife = player->health + (damage >> 4);
                newLife = newLife > 100 ? 100 : newLife;
                pmo->health = player->health = newLife;
                P_SetPsprite(player, ps_weapon, S_CSTAFFATK2_1);
            }
            P_ShotAmmo(player);
            break;
        }
    }
}

void C_DECL A_CStaffAttack(player_t *player, pspdef_t * psp)
{
    mobj_t *mo;
    mobj_t *pmo;

    P_ShotAmmo(player);
    pmo = player->plr->mo;
    mo = P_SPMAngle(pmo, MT_CSTAFF_MISSILE, pmo->angle - (ANG45 / 15));
    if(mo)
    {
        mo->special2 = 32;
    }
    mo = P_SPMAngle(pmo, MT_CSTAFF_MISSILE, pmo->angle + (ANG45 / 15));
    if(mo)
    {
        mo->special2 = 0;
    }
    S_StartSound(SFX_CLERIC_CSTAFF_FIRE, player->plr->mo);
}

void C_DECL A_CStaffMissileSlither(mobj_t *actor)
{
    fixed_t newX, newY;
    int     weaveXY;
    int     angle;

    weaveXY = actor->special2;
    angle = (actor->angle + ANG90) >> ANGLETOFINESHIFT;
    newX = actor->pos[VX] - FixedMul(finecosine[angle], FloatBobOffsets[weaveXY]);
    newY = actor->pos[VY] - FixedMul(finesine[angle], FloatBobOffsets[weaveXY]);
    weaveXY = (weaveXY + 3) & 63;
    newX += FixedMul(finecosine[angle], FloatBobOffsets[weaveXY]);
    newY += FixedMul(finesine[angle], FloatBobOffsets[weaveXY]);
    P_TryMove(actor, newX, newY);
    actor->special2 = weaveXY;
}

void C_DECL A_CStaffInitBlink(player_t *player, pspdef_t * psp)
{
    player->plr->mo->special1 = (P_Random() >> 1) + 20;
}

void C_DECL A_CStaffCheckBlink(player_t *player, pspdef_t * psp)
{
    if(!--player->plr->mo->special1)
    {
        P_SetPsprite(player, ps_weapon, S_CSTAFFBLINK1);
        player->plr->mo->special1 = (P_Random() + 50) >> 2;
    }
}

void C_DECL A_CFlameAttack(player_t *player, pspdef_t * psp)
{
    mobj_t *mo;

    mo = P_SpawnPlayerMissile(player->plr->mo, MT_CFLAME_MISSILE);
    if(mo)
    {
        mo->thinker.function = P_BlasterMobjThinker;
        mo->special1 = 2;
    }

    P_ShotAmmo(player);
    S_StartSound(SFX_CLERIC_FLAME_FIRE, player->plr->mo);
}

void C_DECL A_CFlamePuff(mobj_t *actor)
{
    A_UnHideThing(actor);
    actor->momx = 0;
    actor->momy = 0;
    actor->momz = 0;
    S_StartSound(SFX_CLERIC_FLAME_EXPLODE, actor);
}

void C_DECL A_CFlameMissile(mobj_t *actor)
{
    int     i;
    int     an, an90;
    fixed_t dist;
    mobj_t *mo;

    A_UnHideThing(actor);
    S_StartSound(SFX_CLERIC_FLAME_EXPLODE, actor);
    if(BlockingMobj && BlockingMobj->flags & MF_SHOOTABLE)
    {   // Hit something, so spawn the flame circle around the thing
        dist = BlockingMobj->radius + 18 * FRACUNIT;
        for(i = 0; i < 4; i++)
        {
            an = (i * ANG45) >> ANGLETOFINESHIFT;
            an90 = (i * ANG45 + ANG90) >> ANGLETOFINESHIFT;
            mo = P_SpawnMobj(BlockingMobj->pos[VX] + FixedMul(dist, finecosine[an]),
                             BlockingMobj->pos[VY] + FixedMul(dist, finesine[an]),
                             BlockingMobj->pos[VZ] + 5 * FRACUNIT, MT_CIRCLEFLAME);
            if(mo)
            {
                mo->angle = an << ANGLETOFINESHIFT;
                mo->target = actor->target;
                mo->momx = mo->special1 = FixedMul(FLAMESPEED, finecosine[an]);
                mo->momy = mo->special2 = FixedMul(FLAMESPEED, finesine[an]);
                mo->tics -= P_Random() & 3;
            }
            mo = P_SpawnMobj(BlockingMobj->pos[VX] - FixedMul(dist, finecosine[an]),
                             BlockingMobj->pos[VY] - FixedMul(dist, finesine[an]),
                             BlockingMobj->pos[VZ] + 5 * FRACUNIT, MT_CIRCLEFLAME);
            if(mo)
            {
                mo->angle = ANG180 + (an << ANGLETOFINESHIFT);
                mo->target = actor->target;
                mo->momx = mo->special1 =
                    FixedMul(-FLAMESPEED, finecosine[an]);
                mo->momy = mo->special2 = FixedMul(-FLAMESPEED, finesine[an]);
                mo->tics -= P_Random() & 3;
            }
        }
        P_SetMobjState(actor, S_FLAMEPUFF2_1);
    }
}

void C_DECL A_CFlameRotate(mobj_t *actor)
{
    int     an;

    an = (actor->angle + ANG90) >> ANGLETOFINESHIFT;
    actor->momx = actor->special1 + FixedMul(FLAMEROTSPEED, finecosine[an]);
    actor->momy = actor->special2 + FixedMul(FLAMEROTSPEED, finesine[an]);
    actor->angle += ANG90 / 15;
}

/*
 * Spawns the spirits
 */
void C_DECL A_CHolyAttack3(mobj_t *actor)
{
    P_SpawnMissile(actor, actor->target, MT_HOLY_MISSILE);
    S_StartSound(SFX_CHOLY_FIRE, actor);
}

/*
 * Spawns the spirits
 */
void C_DECL A_CHolyAttack2(mobj_t *actor)
{
    int     j;
    int     i;
    mobj_t *mo;
    mobj_t *tail, *next;

    for(j = 0; j < 4; j++)
    {
        mo = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_HOLY_FX);
        if(!mo)
        {
            continue;
        }
        switch (j)
        {                       // float bob index
        case 0:
            mo->special2 = P_Random() & 7;  // upper-left
            break;
        case 1:
            mo->special2 = 32 + (P_Random() & 7);   // upper-right
            break;
        case 2:
            mo->special2 = (32 + (P_Random() & 7)) << 16;   // lower-left
            break;
        case 3:
            mo->special2 =
                ((32 + (P_Random() & 7)) << 16) + 32 + (P_Random() & 7);
            break;
        }
        mo->pos[VZ] = actor->pos[VZ];
        mo->angle = actor->angle + (ANGLE_45 + ANGLE_45 / 2) - ANGLE_45 * j;
        P_ThrustMobj(mo, mo->angle, mo->info->speed);
        mo->target = actor->target;
        mo->args[0] = 10;       // initial turn value
        mo->args[1] = 0;        // initial look angle
        if(deathmatch)
        {                       // Ghosts last slightly less longer in DeathMatch
            mo->health = 85;
        }
        if(linetarget)
        {
            mo->tracer = linetarget;
            mo->flags |= MF_NOCLIP | MF_SKULLFLY;
            mo->flags &= ~MF_MISSILE;
        }
        tail = P_SpawnMobj(mo->pos[VX], mo->pos[VY], mo->pos[VZ], MT_HOLY_TAIL);
        tail->target = mo;  // parent
        for(i = 1; i < 3; i++)
        {
            next = P_SpawnMobj(mo->pos[VX], mo->pos[VY], mo->pos[VZ], MT_HOLY_TAIL);
            P_SetMobjState(next, next->info->spawnstate + 1);
            tail->tracer = next;
            tail = next;
        }
        tail->tracer = NULL;     // last tail bit
    }
}

void C_DECL A_CHolyAttack(player_t *player, pspdef_t * psp)
{
    mobj_t *mo;

    P_ShotAmmo(player);
    mo = P_SpawnPlayerMissile(player->plr->mo, MT_HOLY_MISSILE);
    if(player == &players[consoleplayer])
    {
        player->damagecount = 0;
        player->bonuscount = 0;

        R_SetFilter(STARTHOLYPAL);
    }
    S_StartSound(SFX_CHOLY_FIRE, player->plr->mo);
}

void C_DECL A_CHolyPalette(player_t *player, pspdef_t * psp)
{
    int     pal;

    if(player == &players[consoleplayer])
    {
        pal = STARTHOLYPAL + psp->state - (&states[S_CHOLYATK_6]);
        if(pal == STARTHOLYPAL + 3)
        {                       // reset back to original playpal
            pal = 0;
        }

        R_SetFilter(pal);
    }
}

static void CHolyFindTarget(mobj_t *actor)
{
    mobj_t *target;

    target = P_RoughMonsterSearch(actor, 6);
    if(target)
    {
        Con_Message("CHolyFindTarget: mobj_t* converted to int! Not 64-bit compatible.\n");
        actor->tracer = target;
        actor->flags |= MF_NOCLIP | MF_SKULLFLY;
        actor->flags &= ~MF_MISSILE;
    }
}

/*
 * Similar to P_SeekerMissile, but seeks to a random Z on the target
 */
static void CHolySeekerMissile(mobj_t *actor, angle_t thresh, angle_t turnMax)
{
    int     dir;
    int     dist;
    angle_t delta;
    angle_t angle;
    mobj_t *target;
    fixed_t newZ;
    fixed_t deltaZ;

    target = actor->tracer;
    if(target == NULL)
    {
        return;
    }
    if(!(target->flags & MF_SHOOTABLE) ||
       (!(target->flags & MF_COUNTKILL) && !target->player))
    {   // Target died/target isn't a player or creature
        actor->tracer = NULL;
        actor->flags &= ~(MF_NOCLIP | MF_SKULLFLY);
        actor->flags |= MF_MISSILE;
        CHolyFindTarget(actor);
        return;
    }
    dir = P_FaceMobj(actor, target, &delta);
    if(delta > thresh)
    {
        delta >>= 1;
        if(delta > turnMax)
        {
            delta = turnMax;
        }
    }
    if(dir)
    {   // Turn clockwise
        actor->angle += delta;
    }
    else
    {   // Turn counter clockwise
        actor->angle -= delta;
    }
    angle = actor->angle >> ANGLETOFINESHIFT;
    actor->momx = FixedMul(actor->info->speed, finecosine[angle]);
    actor->momy = FixedMul(actor->info->speed, finesine[angle]);
    if(!(leveltime & 15) || actor->pos[VZ] > target->pos[VZ] + (target->height) ||
       actor->pos[VZ] + actor->height < target->pos[VZ])
    {
        newZ = target->pos[VZ] + ((P_Random() * target->height) >> 8);
        deltaZ = newZ - actor->pos[VZ];
        if(abs(deltaZ) > 15 * FRACUNIT)
        {
            if(deltaZ > 0)
            {
                deltaZ = 15 * FRACUNIT;
            }
            else
            {
                deltaZ = -15 * FRACUNIT;
            }
        }
        dist = P_ApproxDistance(target->pos[VX] - actor->pos[VX],
                                target->pos[VX] - actor->pos[VY]);
        dist = dist / actor->info->speed;
        if(dist < 1)
        {
            dist = 1;
        }
        actor->momz = deltaZ / dist;
    }
    return;
}

static void CHolyWeave(mobj_t *actor)
{
    fixed_t newX, newY;
    int     weaveXY, weaveZ;
    int     angle;

    weaveXY = actor->special2 >> 16;
    weaveZ = actor->special2 & 0xFFFF;
    angle = (actor->angle + ANG90) >> ANGLETOFINESHIFT;
    newX =
        actor->pos[VX] - FixedMul(finecosine[angle], FloatBobOffsets[weaveXY] << 2);
    newY = actor->pos[VY] - FixedMul(finesine[angle], FloatBobOffsets[weaveXY] << 2);
    weaveXY = (weaveXY + (P_Random() % 5)) & 63;
    newX += FixedMul(finecosine[angle], FloatBobOffsets[weaveXY] << 2);
    newY += FixedMul(finesine[angle], FloatBobOffsets[weaveXY] << 2);
    P_TryMove(actor, newX, newY);
    actor->pos[VZ] -= FloatBobOffsets[weaveZ] << 1;
    weaveZ = (weaveZ + (P_Random() % 5)) & 63;
    actor->pos[VZ] += FloatBobOffsets[weaveZ] << 1;
    actor->special2 = weaveZ + (weaveXY << 16);
}

void C_DECL A_CHolySeek(mobj_t *actor)
{
    actor->health--;
    if(actor->health <= 0)
    {
        actor->momx >>= 2;
        actor->momy >>= 2;
        actor->momz = 0;
        P_SetMobjState(actor, actor->info->deathstate);
        actor->tics -= P_Random() & 3;
        return;
    }
    if(actor->tracer)
    {
        CHolySeekerMissile(actor, actor->args[0] * ANGLE_1,
                           actor->args[0] * ANGLE_1 * 2);
        if(!((leveltime + 7) & 15))
        {
            actor->args[0] = 5 + (P_Random() / 20);
        }
    }
    CHolyWeave(actor);
}

static void CHolyTailFollow(mobj_t *actor, fixed_t dist)
{
    mobj_t *child;
    int     an;
    fixed_t oldDistance, newDistance;

    child = actor->tracer;
    if(child)
    {
        an = R_PointToAngle2(actor->pos[VX], actor->pos[VY],
                             child->pos[VX], child->pos[VY]) >> ANGLETOFINESHIFT;
        oldDistance =
            P_ApproxDistance(child->pos[VX] - actor->pos[VX],
                             child->pos[VY] - actor->pos[VY]);
        if(P_TryMove
           (child, actor->pos[VX] + FixedMul(dist, finecosine[an]),
            actor->pos[VY] + FixedMul(dist, finesine[an])))
        {
            newDistance =
                P_ApproxDistance(child->pos[VX] - actor->pos[VX],
                                 child->pos[VY] - actor->pos[VY]) - FRACUNIT;
            if(oldDistance < FRACUNIT)
            {
                if(child->pos[VZ] < actor->pos[VZ])
                {
                    child->pos[VZ] = actor->pos[VZ] - dist;
                }
                else
                {
                    child->pos[VZ] = actor->pos[VZ] + dist;
                }
            }
            else
            {
                child->pos[VZ] =
                    actor->pos[VZ] + FixedMul(FixedDiv(newDistance, oldDistance),
                                        child->pos[VZ] - actor->pos[VZ]);
            }
        }
        CHolyTailFollow(child, dist - FRACUNIT);
    }
}

static void CHolyTailRemove(mobj_t *actor)
{
    mobj_t *child;

    child = actor->tracer;
    if(child)
    {
        CHolyTailRemove(child);
    }
    P_RemoveMobj(actor);
}

void C_DECL A_CHolyTail(mobj_t *actor)
{
    mobj_t *parent;

    parent = actor->target;

    if(parent)
    {
        if(parent->state >= &states[parent->info->deathstate])
        {   // Ghost removed, so remove all tail parts
            CHolyTailRemove(actor);
            return;
        }
        else if(P_TryMove
                (actor,
                 parent->pos[VX] - FixedMul(14 * FRACUNIT,
                                      finecosine[parent->
                                                 angle >> ANGLETOFINESHIFT]),
                 parent->pos[VY] - FixedMul(14 * FRACUNIT,
                                      finesine[parent->
                                               angle >> ANGLETOFINESHIFT])))
        {
            actor->pos[VZ] = parent->pos[VZ] - 5 * FRACUNIT;
        }
        CHolyTailFollow(actor, 10 * FRACUNIT);
    }
}

void C_DECL A_CHolyCheckScream(mobj_t *actor)
{
    A_CHolySeek(actor);
    if(P_Random() < 20)
    {
        S_StartSound(SFX_SPIRIT_ACTIVE, actor);
    }
    if(!actor->tracer)
    {
        CHolyFindTarget(actor);
    }
}

void C_DECL A_CHolySpawnPuff(mobj_t *actor)
{
    P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_HOLY_MISSILE_PUFF);
}

void C_DECL A_FireConePL1(player_t *player, pspdef_t * psp)
{
    angle_t angle;
    int     damage;
    int     slope;
    int     i;
    mobj_t *pmo, *mo;
    int     conedone = false;

    pmo = player->plr->mo;
    P_ShotAmmo(player);
    S_StartSound(SFX_MAGE_SHARDS_FIRE, pmo);

    damage = 90 + (P_Random() & 15);
    for(i = 0; i < 16; i++)
    {
        angle = pmo->angle + i * (ANG45 / 16);
        slope = P_AimLineAttack(pmo, angle, MELEERANGE);
        if(linetarget)
        {
            pmo->flags2 |= MF2_ICEDAMAGE;
            P_DamageMobj(linetarget, pmo, pmo, damage);
            pmo->flags2 &= ~MF2_ICEDAMAGE;
            conedone = true;
            break;
        }
    }

    // didn't find any creatures, so fire projectiles
    if(!conedone)
    {
        mo = P_SpawnPlayerMissile(pmo, MT_SHARDFX1);
        if(mo)
        {
            mo->special1 =
                SHARDSPAWN_LEFT | SHARDSPAWN_DOWN | SHARDSPAWN_UP |
                SHARDSPAWN_RIGHT;
            mo->special2 = 3;   // Set sperm count (levels of reproductivity)
            mo->target = pmo;
            mo->args[0] = 3;    // Mark Initial shard as super damage
        }
    }
}

void C_DECL A_ShedShard(mobj_t *actor)
{
    mobj_t *mo;
    int     spawndir = actor->special1;
    int     spermcount = actor->special2;

    if(spermcount <= 0)
        return;                 // No sperm left
    actor->special2 = 0;
    spermcount--;

    // every so many calls, spawn a new missile in it's set directions
    if(spawndir & SHARDSPAWN_LEFT)
    {
        mo = P_SpawnMissileAngleSpeed(actor, MT_SHARDFX1,
                                      actor->angle + (ANG45 / 9), 0,
                                      (20 + 2 * spermcount) << FRACBITS);
        if(mo)
        {
            mo->special1 = SHARDSPAWN_LEFT;
            mo->special2 = spermcount;
            mo->momz = actor->momz;
            mo->target = actor->target;
            mo->args[0] = (spermcount == 3) ? 2 : 0;
        }
    }
    if(spawndir & SHARDSPAWN_RIGHT)
    {
        mo = P_SpawnMissileAngleSpeed(actor, MT_SHARDFX1,
                                      actor->angle - (ANG45 / 9), 0,
                                      (20 + 2 * spermcount) << FRACBITS);
        if(mo)
        {
            mo->special1 = SHARDSPAWN_RIGHT;
            mo->special2 = spermcount;
            mo->momz = actor->momz;
            mo->target = actor->target;
            mo->args[0] = (spermcount == 3) ? 2 : 0;
        }
    }
    if(spawndir & SHARDSPAWN_UP)
    {
        mo = P_SpawnMissileAngleSpeed(actor, MT_SHARDFX1, actor->angle, 0,
                                      (15 + 2 * spermcount) << FRACBITS);
        if(mo)
        {
            mo->momz = actor->momz;
            mo->pos[VZ] += 8 * FRACUNIT;
            if(spermcount & 1)  // Every other reproduction
                mo->special1 =
                    SHARDSPAWN_UP | SHARDSPAWN_LEFT | SHARDSPAWN_RIGHT;
            else
                mo->special1 = SHARDSPAWN_UP;
            mo->special2 = spermcount;
            mo->target = actor->target;
            mo->args[0] = (spermcount == 3) ? 2 : 0;
        }
    }
    if(spawndir & SHARDSPAWN_DOWN)
    {
        mo = P_SpawnMissileAngleSpeed(actor, MT_SHARDFX1, actor->angle, 0,
                                      (15 + 2 * spermcount) << FRACBITS);
        if(mo)
        {
            mo->momz = actor->momz;
            mo->pos[VZ] -= 4 * FRACUNIT;
            if(spermcount & 1)  // Every other reproduction
                mo->special1 =
                    SHARDSPAWN_DOWN | SHARDSPAWN_LEFT | SHARDSPAWN_RIGHT;
            else
                mo->special1 = SHARDSPAWN_DOWN;
            mo->special2 = spermcount;
            mo->target = actor->target;
            mo->args[0] = (spermcount == 3) ? 2 : 0;
        }
    }
}

void C_DECL A_Light0(player_t *player, pspdef_t * psp)
{
    player->plr->extralight = 0;
}

/*
 * Called at start of level for each player
 */
void P_SetupPsprites(player_t *player)
{
    int     i;

#ifdef _DEBUG
    Con_Message("P_SetupPsprites: Player %i.\n", player - players);
#endif

    // Remove all psprites
    for(i = 0; i < NUMPSPRITES; i++)
    {
        player->psprites[i].state = NULL;
    }
    // Spawn the ready weapon
    player->pendingweapon = player->readyweapon;
    P_BringUpWeapon(player);
}

/*
 * Called every tic by player thinking routine
 */
void P_MovePsprites(player_t *player)
{
    int     i;
    pspdef_t *psp;
    state_t *state;

    psp = &player->psprites[0];
    for(i = 0; i < NUMPSPRITES; i++, psp++)
    {
        if((state = psp->state) != 0)   // a null state means not active
        {
            // drop tic count and possibly change state
            if(psp->tics != -1) // a -1 tic count never changes
            {
                psp->tics--;
                if(!psp->tics)
                {
                    P_SetPsprite(player, i, psp->state->nextstate);
                }
            }
        }
    }
    player->psprites[ps_flash].sx = player->psprites[ps_weapon].sx;
    player->psprites[ps_flash].sy = player->psprites[ps_weapon].sy;
}
