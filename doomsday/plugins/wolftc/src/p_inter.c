/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Martin Eyre <martineyre@btinternet.com>
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

/**
 * p_inter.c: Handling interactions (i.e., collisions).
 */

#ifdef MSVC
#  pragma optimize("g", off)
#endif

// HEADER FILES ------------------------------------------------------------

#include "wolftc.h"

#include "am_map.h"
#include "d_net.h"
#include "dmu_lib.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

#define BONUSADD            (6)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// A weapon is found with two clip loads, a big item has five clip loads.
int maxAmmo[NUM_AMMO_TYPES] = { 200, 50, 300, 50 };
int clipAmmo[NUM_AMMO_TYPES] = { 10, 4, 20, 1 };

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/**
 * @param num           Number of clip loads, not the individual count.
 *
 * @return              @c false, if the ammo can't be picked up at all.
 */
boolean P_GiveAmmo(player_t* player, ammotype_t ammo, int num)
{
    if(ammo == AT_NOAMMO)
        return false;

    if(ammo < 0 || ammo > NUM_AMMO_TYPES)
        Con_Error("P_GiveAmmo: bad type %i", ammo);

    if(player->ammo[ammo] == player->maxAmmo[ammo])
        return false;

    if(num)
        num *= clipAmmo[ammo];
    else
        num = clipAmmo[ammo] / 2;

    if(gameskill == SM_BABY || gameskill == SM_NIGHTMARE)
    {
        // give double ammo in trainer mode,
        // you'll need in nightmare
        num <<= 1;
    }

    // We are about to receive some more ammo. Does the player want to
    // change weapon automatically?
    P_MaybeChangeWeapon(player, WT_NOCHANGE, ammo, false);

    player->ammo[ammo] += num;
    player->update |= PSF_AMMO;

    if(player->ammo[ammo] > player->maxAmmo[ammo])
        player->ammo[ammo] = player->maxAmmo[ammo];

    // Maybe unhide the HUD?
    if(player == &players[CONSOLEPLAYER])
        ST_HUDUnHide(HUE_ON_PICKUP_AMMO);

    return true;
}

/**
 * The weapon name may have a MF_DROPPED flag ored in.
 */
boolean P_GiveWeapon(player_t *player, weapontype_t weapon, boolean dropped)
{
    ammotype_t          i;
    boolean             gaveammo = false;
    boolean             gaveweapon;
    int                 numClips;

    if(IS_NETGAME && (deathmatch != 2) && !dropped)
    {
        // leave placed weapons forever on net games
        if(player->weaponOwned[weapon])
            return false;

        player->bonusCount += BONUSADD;
        player->weaponOwned[weapon] = true;
        player->update |= PSF_OWNED_WEAPONS;

        // Give some of each of the ammo types used by this weapon.
        for(i=0; i < NUM_AMMO_TYPES; ++i)
        {
            if(!weaponInfo[weapon][player->class].mode[0].ammoType[i])
                continue;   // Weapon does not take this type of ammo.

            if(deathmatch)
                numClips = 5;
            else
                numClips = 2;

            if(P_GiveAmmo(player, i, numClips))
                gaveammo = true; // at least ONE type of ammo was given.
        }

        // Should we change weapon automatically?
        P_MaybeChangeWeapon(player, weapon, AT_NOAMMO, deathmatch == 1);

        // Maybe unhide the HUD?
        if(player == &players[CONSOLEPLAYER])
            ST_HUDUnHide(HUE_ON_PICKUP_WEAPON);

        S_ConsoleSound(SFX_WPNUP, NULL, player - players);
        return false;
    }
    else
    {
        // Give some of each of the ammo types used by this weapon.
        for(i=0; i < NUM_AMMO_TYPES; ++i)
        {
            if(!weaponInfo[weapon][player->class].mode[0].ammoType[i])
                continue;   // Weapon does not take this type of ammo.

            // give one clip with a dropped weapon,
            // two clips with a found weapon
            if(dropped)
                numClips = 1;
            else
                numClips = 2;

            if(P_GiveAmmo(player, i, numClips))
                gaveammo = true; // at least ONE type of ammo was given.
        }

        if(player->weaponOwned[weapon])
            gaveweapon = false;
        else
        {
            gaveweapon = true;
            player->weaponOwned[weapon] = true;
            player->update |= PSF_OWNED_WEAPONS;

            // Should we change weapon automatically?
            P_MaybeChangeWeapon(player, weapon, AT_NOAMMO, false);
        }

        // Maybe unhide the HUD?
        if(gaveweapon && player == &players[CONSOLEPLAYER])
            ST_HUDUnHide(HUE_ON_PICKUP_WEAPON);

        return (gaveweapon || gaveammo);
    }
}

/**
 * @return          @c false, if the body isn't needed at all
 */
boolean P_GiveBody(player_t *player, int num)
{
    if(player->health >= maxhealth)
        return false;

    player->health += num;
    if(player->health > maxhealth)
        player->health = maxhealth;

    player->plr->mo->health = player->health;
    player->update |= PSF_HEALTH;

    // Maybe unhide the HUD?
    if(player == &players[CONSOLEPLAYER])
        ST_HUDUnHide(HUE_ON_PICKUP_HEALTH);

    return true;
}

/**
 * @return          @c false, if the armor is worse than the
 *                  current armor.
 */
boolean P_GiveArmor(player_t *player, int armortype)
{
    int     hits, i;

    //hits = armortype*100;
    i = armortype - 1;
    if(i < 0)
        i = 0;
    if(i > 1)
        i = 1;
    hits = armorpoints[i];
    if(player->armorPoints >= hits)
        return false;           // don't pick up

    player->armorType = armortype;
    player->armorPoints = hits;
    player->update |= PSF_ARMOR_TYPE | PSF_ARMOR_POINTS;

    // Maybe unhide the HUD?
    if(player == &players[CONSOLEPLAYER])
        ST_HUDUnHide(HUE_ON_PICKUP_ARMOR);

    return true;
}

void P_GiveKey(player_t *player, keytype_t card)
{
    if(player->keys[card])
        return;

    player->bonusCount = BONUSADD;
    player->keys[card] = 1;
    player->update |= PSF_KEYS;

    // Maybe unhide the HUD?
    if(player == &players[CONSOLEPLAYER])
        ST_HUDUnHide(HUE_ON_PICKUP_KEY);
}

void P_GiveBackpack(player_t *player)
{
    int     i;

    if(!player->backpack)
    {
        player->update |= PSF_MAX_AMMO;
        for(i = 0; i < NUM_AMMO_TYPES; i++)
            player->maxAmmo[i] *= 2;
        player->backpack = true;
    }
    for(i = 0; i < NUM_AMMO_TYPES; i++)
        P_GiveAmmo(player, i, 1);
    P_SetMessage(player, GOTBACKPACK, false);
}

boolean P_GivePower(player_t *player, int power)
{
    player->update |= PSF_POWERS;

    switch(power)
    {
    case PT_INVULNERABILITY:
        player->powers[power] = INVULNTICS;
        break;

    case PT_INVISIBILITY:
        player->powers[power] = INVISTICS;
        player->plr->mo->flags |= MF_SHADOW;
        break;

    case PT_FLIGHT:
        player->powers[power] = 1;
        player->plr->mo->flags2 |= MF2_FLY;
        player->plr->mo->flags |= MF_NOGRAVITY;
        if(player->plr->mo->pos[VZ] <= FLT2FIX(player->plr->mo->floorZ))
        {
            player->flyHeight = 10; // thrust the player in the air a bit
            player->plr->mo->flags |= DDPF_FIXMOM;
        }
        break;

    case PT_INFRARED:
        player->powers[power] = INFRATICS;
        break;

    case PT_IRONFEET:
        player->powers[power] = IRONTICS;
        break;

    case PT_STRENGTH:
        P_GiveBody(player, maxhealth);
        player->powers[power] = 1;
        break;

    default:
        if(player->powers[power])
            return false;           // already got it

        player->powers[power] = 1;
        break;
    }

    // Maybe unhide the HUD?
    if(player == &players[CONSOLEPLAYER])
        ST_HUDUnHide(HUE_ON_PICKUP_POWER);
    return true;
}

boolean P_TakePower(player_t *player, int power)
{
    mobj_t *plrmo = player->plr->mo;

    player->update |= PSF_POWERS;
    if(player->powers[PT_FLIGHT])
    {
        if(plrmo->pos[VZ] != plrmo->floorZ && cfg.lookSpring)
        {
            player->centering = true;
        }

        plrmo->flags2 &= ~MF2_FLY;
        plrmo->flags &= ~MF_NOGRAVITY;
        player->powers[power] = 0;
        return true;
    }

    if(!player->powers[power])
        return false;           // dont got it

    player->powers[power] = 0;
    return true;
}

void P_TouchSpecialMobj(mobj_t *special, mobj_t *toucher)
{
    player_t *player;
    fixed_t delta;
    int     sound;

    delta = special->pos[VZ] - toucher->pos[VZ];

    if(delta > FLT2FIX(toucher->height) || delta < -8 * FRACUNIT)
    {
        // out of reach
        return;
    }

    sound = SFX_ITEMUP;
    player = toucher->player;

    // Dead thing touching.
    // Can happen with a sliding player corpse.
    if(toucher->health <= 0)
        return;

    // Identify by sprite.
    switch (special->sprite)
    {
        // armor
    case SPR_ARM1:
        if(!P_GiveArmor(player, armorclass[0]))
            return;
        P_SetMessage(player, GOTARMOR, false);
        break;

    case SPR_ARM2:
        if(!P_GiveArmor(player, armorclass[1]))
            return;
        P_SetMessage(player, GOTMEGA, false);
        break;

        // bonus items
    case SPR_BON1:
        player->health++;       // can go over 100%
        if(player->health > healthlimit)
            player->health = healthlimit;
        player->plr->mo->health = player->health;
        player->update |= PSF_HEALTH;
        P_SetMessage(player, GOTHTHBONUS, false);

        // Maybe unhide the HUD?
        if(player == &players[CONSOLEPLAYER])
            ST_HUDUnHide(HUE_ON_PICKUP_HEALTH);
        break;

    case SPR_BON2:
        player->armorPoints++;  // can go over 100%
        if(player->armorPoints > armorpoints[1])    // 200
            player->armorPoints = armorpoints[1];
        if(!player->armorType)
            player->armorType = armorclass[0];
        player->update |= PSF_ARMOR_TYPE | PSF_ARMOR_POINTS;
        P_SetMessage(player, GOTARMBONUS, false);

        // Maybe unhide the HUD?
        if(player == &players[CONSOLEPLAYER])
            ST_HUDUnHide(HUE_ON_PICKUP_ARMOR);
        break;

    case SPR_SOUL:
        player->health += soulSphereHealth;
        if(player->health > soulSphereLimit)
            player->health = soulSphereLimit;
        player->plr->mo->health = player->health;
        player->update |= PSF_HEALTH;
        P_SetMessage(player, GOTSUPER, false);
        sound = SFX_GETPOW;

        // Maybe unhide the HUD?
        if(player == &players[CONSOLEPLAYER])
            ST_HUDUnHide(HUE_ON_PICKUP_HEALTH);
        break;

    case SPR_MEGA:
        if(gamemode != commercial)
            return;
        player->health = megaspherehealth;
        player->plr->mo->health = player->health;
        player->update |= PSF_HEALTH;
        P_GiveArmor(player, armorclass[1]);
        P_SetMessage(player, GOTMSPHERE, false);
        sound = SFX_GETPOW;

        // Maybe unhide the HUD?
        if(player == &players[CONSOLEPLAYER])
            ST_HUDUnHide(HUE_ON_PICKUP_HEALTH);
        break;

        // cards
        // leave cards for everyone
    case SPR_BKEY:
        if(!player->keys[KT_BLUECARD])
            P_SetMessage(player, GOTBLUECARD, false);
        P_GiveKey(player, KT_BLUECARD);
        if(!IS_NETGAME)
            break;
        return;

    case SPR_YKEY:
        if(!player->keys[KT_YELLOWCARD])
            P_SetMessage(player, GOTYELWCARD, false);
        P_GiveKey(player, KT_YELLOWCARD);
        if(!IS_NETGAME)
            break;
        return;

    case SPR_RKEY:
        if(!player->keys[KT_REDCARD])
            P_SetMessage(player, GOTREDCARD, false);
        P_GiveKey(player, KT_REDCARD);
        if(!IS_NETGAME)
            break;
        return;

    case SPR_BSKU:
        if(!player->keys[KT_BLUESKULL])
            P_SetMessage(player, GOTBLUESKUL, false);
        P_GiveKey(player, KT_BLUESKULL);
        if(!IS_NETGAME)
            break;
        return;

    case SPR_YSKU:
        if(!player->keys[KT_YELLOWSKULL])
            P_SetMessage(player, GOTYELWSKUL, false);
        P_GiveKey(player, KT_YELLOWSKULL);
        if(!IS_NETGAME)
            break;
        return;

    case SPR_RSKU:
        if(!player->keys[KT_REDSKULL])
            P_SetMessage(player, GOTREDSKULL, false);
        P_GiveKey(player, KT_REDSKULL);
        if(!IS_NETGAME)
            break;
        return;

        // medikits, heals
    case SPR_STIM:
        if(!P_GiveBody(player, 10))
            return;
        P_SetMessage(player, GOTSTIM, false);
        break;

    case SPR_MEDI:
    {
        int msg;
        // DOOM bug
        // The following test was originaly placed AFTER the call to
        // P_GiveBody thereby making the first outcome impossible as
        // the medikit gives 25 points of health. This resulted that
        // the GOTMEDINEED "Picked up a medikit that you REALLY need"
        // was never used.
        if(player->health < 25)
            msg = TXT_GOTMEDINEED;
        else
            msg = TXT_GOTMEDIKIT;

        if(!P_GiveBody(player, 25))
            return;

        P_SetMessage(player, GET_TXT(msg), false);
        break;
    }
    // power ups
    case SPR_PINV:
        if(!P_GivePower(player, PT_INVULNERABILITY))
            return;
        P_SetMessage(player, GOTINVUL, false);
        sound = SFX_GETPOW;
        break;

    case SPR_PSTR:
        if(!P_GivePower(player, PT_STRENGTH))
            return;
        P_SetMessage(player, GOTBERSERK, false);
        if(player->readyWeapon != WT_FIRST && cfg.berserkAutoSwitch)
        {
            player->pendingWeapon = WT_FIRST;
            player->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;
        }
        sound = SFX_GETPOW;
        break;

    case SPR_PINS:
        if(!P_GivePower(player, PT_INVISIBILITY))
            return;
        P_SetMessage(player, GOTINVIS, false);
        sound = SFX_GETPOW;
        break;

    case SPR_SUIT:
        if(!P_GivePower(player, PT_IRONFEET))
            return;
        P_SetMessage(player, GOTSUIT, false);
        sound = SFX_GETPOW;
        break;

    case SPR_PMAP:
        if(!P_GivePower(player, PT_ALLMAP))
            return;
        P_SetMessage(player, GOTMAP, false);
        sound = SFX_GETPOW;
        break;

    case SPR_PVIS:
        if(!P_GivePower(player, PT_INFRARED))
            return;
        P_SetMessage(player, GOTVISOR, false);
        sound = SFX_GETPOW;
        break;

        // ammo
    case SPR_CLIP:
        if(special->flags & MF_DROPPED)
        {
            if(!P_GiveAmmo(player, AT_CLIP, 0))
                return;
        }
        else
        {
            if(!P_GiveAmmo(player, AT_CLIP, 1))
                return;
        }
        P_SetMessage(player, GOTCLIP, false);
        break;

    case SPR_AMMO:
        if(!P_GiveAmmo(player, AT_CLIP, 5))
            return;
        P_SetMessage(player, GOTCLIPBOX, false);
        break;

    case SPR_ROCK:
        if(!P_GiveAmmo(player, AT_MISSILE, 1))
            return;
        P_SetMessage(player, GOTROCKET, false);
        break;

    case SPR_BROK:
        if(!P_GiveAmmo(player, AT_MISSILE, 5))
            return;
        P_SetMessage(player, GOTROCKBOX, false);
        break;

    case SPR_CELL:
        if(!P_GiveAmmo(player, AT_CELL, 1))
            return;
        P_SetMessage(player, GOTCELL, false);
        break;

    case SPR_CELP:
        if(!P_GiveAmmo(player, AT_CELL, 5))
            return;
        P_SetMessage(player, GOTCELLBOX, false);
        break;

    case SPR_SHEL:
        if(!P_GiveAmmo(player, AT_SHELL, 1))
            return;
        P_SetMessage(player, GOTSHELLS, false);
        break;

    case SPR_SBOX:
        if(!P_GiveAmmo(player, AT_SHELL, 5))
            return;
        P_SetMessage(player, GOTSHELLBOX, false);
        break;

    case SPR_BPAK:
        P_GiveBackpack(player);
        break;

        // weapons
    case SPR_BFUG:
        if(!P_GiveWeapon(player, WT_SEVENTH, false))
            return;
        P_SetMessage(player, GOTBFG9000, false);
        sound = SFX_WPNUP;
        break;

    case SPR_MGUN:
        if(!P_GiveWeapon(player, WT_FOURTH, special->flags & MF_DROPPED))
            return;
        P_SetMessage(player, GOTCHAINGUN, false);
        sound = SFX_WPNUP;
        break;

    case SPR_CSAW:
        if(!P_GiveWeapon(player, WT_EIGHTH, false))
            return;
        P_SetMessage(player, GOTCHAINSAW, false);
        sound = SFX_WPNUP;
        break;

    case SPR_LAUN:
        if(!P_GiveWeapon(player, WT_FIFTH, false))
            return;
        P_SetMessage(player, GOTLAUNCHER, false);
        sound = SFX_WPNUP;
        break;

    case SPR_PLAS:
        if(!P_GiveWeapon(player, WT_SIXTH, false))
            return;
        P_SetMessage(player, GOTPLASMA, false);
        sound = SFX_WPNUP;
        break;

    case SPR_SHOT:
        if(!P_GiveWeapon(player, WT_THIRD, special->flags & MF_DROPPED))
            return;
        P_SetMessage(player, GOTSHOTGUN, false);
        sound = SFX_WPNUP;
        break;

    case SPR_SGN2:
        if(!P_GiveWeapon(player, WT_NINETH, special->flags & MF_DROPPED))
            return;
        P_SetMessage(player, GOTSHOTGUN2, false);
        sound = SFX_WPNUP;
        break;

// START WolfTC objects

    case SPR_DOGF:
        if(!P_GiveBody(player, 2))
            return;
        P_SetMessage(player, GOTSHOTGUN2, false);
        sound = SFX_HELTH1;
        break;

    case SPR_DINR:
        if(!P_GiveBody(player, 10))
            return;
        P_SetMessage(player, GOTSHOTGUN2, false);
        sound = SFX_HELTH2;
        break;

    case SPR_FAID:
        if(!P_GiveBody(player, 25))
            return;
        P_SetMessage(player, GOTSHOTGUN2, false);
        sound = SFX_HELTH3;
        break;

    case SPR_MEGH:
        player->health += 50;
        if(player->health > healthlimit)
            player->health = healthlimit;
        player->plr->mo->health = player->health;
        player->update |= PSF_HEALTH;
        P_SetMessage(player, GOTSHOTGUN2, false);
        sound = SFX_GETMEG;

        // Maybe unhide the HUD?
        if(player == &players[CONSOLEPLAYER]);
            ST_HUDUnHide(HUE_ON_PICKUP_HEALTH);
        break;

    case SPR_HVIA:
        if(!P_GiveBody(player, 25))
            return;
        P_SetMessage(player, GOTSHOTGUN2, false);
        break;

    case SPR_MINI:
        if(!P_GiveBody(player, 2))
            return;
        P_SetMessage(player, GOTSHOTGUN2, false);
        break;

    case SPR_WCLP:
        if(special->flags & MF_DROPPED)
        {
            if(!P_GiveAmmo(player, AT_CLIP, 0))
                return;
        }
        else
        {
            if(!P_GiveAmmo(player, AT_CLIP, 1))
                return;
        }
        P_SetMessage(player, GOTCLIP, false);
        sound = SFX_AMMOCL;
        break;

    case SPR_WABX:
        if(!P_GiveAmmo(player, AT_CLIP, 5))
            return;
        P_SetMessage(player, GOTCLIPBOX, false);
        sound = SFX_AMMOBX;
        break;

    case SPR_WRKT:
        if(!P_GiveAmmo(player, AT_MISSILE, 1))
            return;
        P_SetMessage(player, GOTROCKET, false);
        sound = SFX_AMMOCL;
        break;

    case SPR_WRBX:
        if(!P_GiveAmmo(player, AT_MISSILE, 5))
            return;
        P_SetMessage(player, GOTROCKBOX, false);
        sound = SFX_AMMOBX;
        break;

    case SPR_ORBS:
        if(!P_GiveAmmo(player, AT_CELL, 1))
            return;
        P_SetMessage(player, GOTCELL, false);
        sound = SFX_AMMOCL;
        break;

    case SPR_ORBL:
        if(!P_GiveAmmo(player, AT_CELL, 5))
            return;
        P_SetMessage(player, GOTCELLBOX, false);
        sound = SFX_AMMOCL;
        break;

    case SPR_WFUL:
        if(!P_GiveAmmo(player, AT_SHELL, 1))
            return;
        P_SetMessage(player, GOTSHELLS, false);
        sound = SFX_AMMOCL;
        break;

    case SPR_WFUB:
        if(!P_GiveAmmo(player, AT_SHELL, 5))
            return;
        P_SetMessage(player, GOTSHELLBOX, false);
        sound = SFX_AMMOBX;
        break;

    case SPR_WSKY:
        if(!player->keys[KT_BLUESKULL])
            P_SetMessage(player, GOTBLUESKUL, false);
        P_GiveKey(player, KT_BLUESKULL);
        if(!IS_NETGAME)
        sound = SFX_KEYUP;
            break;
        return;

    case SPR_WGKY:
        if(!player->keys[KT_YELLOWSKULL])
            P_SetMessage(player, GOTYELWSKUL, false);
        P_GiveKey(player, KT_YELLOWSKULL);
        if(!IS_NETGAME)
        sound = SFX_KEYUP;
            break;
        return;

    case SPR_SSKY:
        if(!player->keys[KT_BLUECARD])
            P_SetMessage(player, GOTBLUECARD, false);
        P_GiveKey(player, KT_BLUECARD);
        if(!IS_NETGAME)
        sound = SFX_KEYUP;
            break;
        return;

    case SPR_SGKY:
        if(!player->keys[KT_YELLOWCARD])
            P_SetMessage(player, GOTYELWCARD, false);
        P_GiveKey(player, KT_YELLOWCARD);
        if(!IS_NETGAME)
        sound = SFX_KEYUP;
            break;
        return;

    case SPR_CKYB:
        if(!player->keys[KT_REDSKULL])
            P_SetMessage(player, GOTBLUESKUL, false);
        P_GiveKey(player, KT_BLUESKULL);
        if(!IS_NETGAME)
        sound = SFX_CKEYUP;
            break;
        return;

    case SPR_CKYG:
        if(!player->keys[KT_REDSKULL])
            P_SetMessage(player, GOTREDSKULL, false);
        P_GiveKey(player, KT_REDCARD);
        if(!IS_NETGAME)
        sound = SFX_CKEYUP;
            break;
        return;

    case SPR_CKYR:
        if(!player->keys[KT_REDSKULL])
            P_SetMessage(player, GOTREDSKULL, false);
        P_GiveKey(player, KT_REDSKULL);
        if(!IS_NETGAME)
        sound = SFX_CKEYUP;
            break;
        return;

    case SPR_CKYY:
        if(!player->keys[KT_REDSKULL])
            P_SetMessage(player, GOTYELWSKUL, false);
        P_GiveKey(player, KT_YELLOWSKULL);
        if(!IS_NETGAME)
        sound = SFX_CKEYUP;
            break;
        return;

    case SPR_TRS1:
        player->armorPoints++;  // can go over 100%
        if(player->armorPoints > armorpoints[1])    // 200
            player->armorPoints = armorpoints[1];
        if(!player->armorType)
            player->armorType = 1;
        player->update |= PSF_ARMOR_TYPE | PSF_ARMOR_POINTS;
        P_SetMessage(player, GOTARMBONUS, false);
        sound = SFX_TREAS1;

        // Maybe unhide the HUD?
        if(player == &players[CONSOLEPLAYER]);
            ST_HUDUnHide(HUE_ON_PICKUP_ARMOR);
        break;

    case SPR_TRS2:
        player->armorPoints++;  // can go over 100%
        if(player->armorPoints > armorpoints[1])    // 200
            player->armorPoints = armorpoints[1];
        if(!player->armorType)
            player->armorType = 1;
        player->update |= PSF_ARMOR_TYPE | PSF_ARMOR_POINTS;
        P_SetMessage(player, GOTARMBONUS, false);
        sound = SFX_TREAS2;

        // Maybe unhide the HUD?
        if(player == &players[CONSOLEPLAYER]);
            ST_HUDUnHide(HUE_ON_PICKUP_ARMOR);
        break;

    case SPR_TRS3:
        player->armorPoints++;  // can go over 100%
        if(player->armorPoints > armorpoints[1])    // 200
            player->armorPoints = armorpoints[1];
        if(!player->armorType)
            player->armorType = 1;
        player->update |= PSF_ARMOR_TYPE | PSF_ARMOR_POINTS;
        P_SetMessage(player, GOTARMBONUS, false);
        sound = SFX_TREAS3;

        // Maybe unhide the HUD?
        if(player == &players[CONSOLEPLAYER]);
            ST_HUDUnHide(HUE_ON_PICKUP_ARMOR);
        break;

    case SPR_ATRS:
        player->armorPoints++;  // can go over 100%
        if(player->armorPoints > armorpoints[1])    // 200
            player->armorPoints = armorpoints[1];
        if(!player->armorType)
            player->armorType = 1;
        player->update |= PSF_ARMOR_TYPE | PSF_ARMOR_POINTS;
        P_SetMessage(player, GOTARMBONUS, false);
        sound = SFX_ATREAS;

        // Maybe unhide the HUD?
        if(player == &players[CONSOLEPLAYER]);
            ST_HUDUnHide(HUE_ON_PICKUP_ARMOR);
        break;

    case SPR_PSPE:
        player->health += 25;
        if(player->health > healthlimit)
            player->health = healthlimit;
        player->plr->mo->health = player->health;
        player->update |= PSF_HEALTH;
        P_SetMessage(player, GOTSHOTGUN2, false);
        sound = SFX_GETPOW;

        // Maybe unhide the HUD?
        if(player == &players[CONSOLEPLAYER]);
            ST_HUDUnHide(HUE_ON_PICKUP_HEALTH);
        break;

        // scrolls

    case SPR_CSR1:
        break;

    case SPR_CSR2:
        break;

    case SPR_CSR3:
        break;

    case SPR_CSR4:
        break;

    case SPR_CSR5:
        break;

    case SPR_CSR6:
        break;

    case SPR_CSR7:
        break;

    case SPR_CSR8:
        break;

        // weapons

    case SPR_MGNP:
        if(!P_GiveWeapon(player, WT_THIRD, special->flags & MF_DROPPED))
            return;
        P_SetMessage(player, GOTSHOTGUN, false);
        sound = SFX_GETMAC;
        break;

    case SPR_FLTP:
        if(!P_GiveWeapon(player, WT_NINETH, false))
            return;
        P_SetMessage(player, GOTSHOTGUN2, false);
        sound = SFX_GETGAT;
        break;

    case SPR_CGNP:
        if(!P_GiveWeapon(player, WT_FOURTH, false))
            return;
        P_SetMessage(player, GOTCHAINGUN, false);
        sound = SFX_GETGAT;
        break;

    case SPR_RLAP:
        if(!P_GiveWeapon(player, WT_FIFTH, false))
            return;
        P_SetMessage(player, GOTLAUNCHER, false);
        sound = SFX_GETGAT;
        break;

    case SPR_CWP1:
        if(!P_GiveWeapon(player, WT_SIXTH, false))
            return;
        P_SetMessage(player, GOTPLASMA, false);
        sound = SFX_SPELUP;
        break;

    case SPR_CWP2:
        if(!P_GiveWeapon(player, WT_SEVENTH, false))
            return;
        P_SetMessage(player, GOTBFG9000, false);
        sound = SFX_SPELUP;
        break;

    case SPR_LFLP:
        if(!P_GiveWeapon(player, WT_NINETH, false))
            return;
        P_SetMessage(player, GOTSHOTGUN2, false);
        sound = SFX_LGETGT;
        break;

    case SPR_LCGP:
        if(!P_GiveWeapon(player, WT_FOURTH, false))
            return;
        P_SetMessage(player, GOTCHAINGUN, false);
        sound = SFX_LGETGT;
        break;

    case SPR_LRLP:
        if(!P_GiveWeapon(player, WT_FIFTH, false))
            return;
        P_SetMessage(player, GOTLAUNCHER, false);
        sound = SFX_LGETGT;
        break;

// END WolfTC objects

    default:
        Con_Error("P_SpecialThing: Unknown gettable thing");
    }

    if(special->flags & MF_COUNTITEM)
        player->itemCount++;
    P_MobjRemove(special, false);
    player->bonusCount += BONUSADD;
    /*if (player == &players[CONSOLEPLAYER])
       S_StartSound (NULL, sound); */
    S_ConsoleSound(sound, NULL, player - players);
}

void P_KillMobj(mobj_t *source, mobj_t *target, boolean stomping)
{
    mobjtype_t item;
    mobj_t *mo;
    angle_t angle;

    if(!target) // nothing to kill
        return;

    target->flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY);

    if(target->type != MT_SKULL)
        target->flags &= ~MF_NOGRAVITY;

    target->flags |= MF_CORPSE | MF_DROPOFF;
    target->flags2 &= ~MF2_PASSMOBJ;
    target->corpseTics = 0;
    target->height /= 2*2;

    if(source && source->player)
    {
        // Count for intermission.
        if(target->flags & MF_COUNTKILL)
            source->player->killCount++;

        if(target->player)
        {
            source->player->frags[target->player - players]++;
            NetSv_FragsForAll(source->player);
            NetSv_KillMessage(source->player, target->player, stomping);
        }
    }
    else if(!IS_NETGAME && (target->flags & MF_COUNTKILL))
    {
        // Count all monster deaths, even those caused by other monsters.
        players[0].killCount++;
    }

    if(target->player)
    {
        // Count environment kills against you.
        if(!source)
        {
            target->player->frags[target->player - players]++;
            NetSv_FragsForAll(target->player);
            NetSv_KillMessage(target->player, target->player, stomping);
        }

        target->flags &= ~MF_SOLID;
        target->flags2 &= ~MF2_FLY;
        target->player->powers[PT_FLIGHT] = 0;
        target->player->playerState = PST_DEAD;
        target->player->update |= PSF_STATE;
        target->player->plr->flags |= DDPF_DEAD;
        P_DropWeapon(target->player);

        // Don't die in auto map.
        AM_Stop(target->player - players);
    }

    if(target->health < -target->info->spawnHealth &&
       target->info->xDeathState)
    {
        P_MobjChangeState(target, target->info->xDeathState);
    }
    else
        P_MobjChangeState(target, target->info->deathState);
    target->tics -= P_Random() & 3;

    if(target->tics < 1)
        target->tics = 1;

    //  I_StartSound (&actor->r, actor->info->deathSound);

    // Drop stuff.
    // This determines the kind of object spawned
    // during the death frame of a thing.
    switch (target->type)
    {
    case MT_WOLFSS:
    case MT_POSSESSED:
        item = MT_CLIP;
        break;

    case MT_SHOTGUY:
        item = MT_SHOTGUN;
        break;

    case MT_CHAINGUY:
        item = MT_CHAINGUN;
        break;

    default:
        return;
    }

    // Don't drop at the exact same place, causes Z flickering with
    // 3D sprites.
    angle = M_Random() << (24 - ANGLETOFINESHIFT);
    mo = P_SpawnMobj3f(target->pos[VX] + 3 * finecosine[angle],
                     target->pos[VY] + 3 * finesine[angle], ONFLOORZ, item);
    mo->flags |= MF_DROPPED;    // special versions of items
}

void P_DamageMobj(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                  int damage)
{
    P_DamageMobj2(target, inflictor, source, damage, false);
}

/*
 * Damages both enemies and players
 * Source and inflictor are the same for melee attacks.
 * Source can be NULL for slime, barrel explosions
 * and other environmental stuff.
 *
 * @parm inflictor: is the thing that caused the damage
 *                  creature or missile, can be NULL (slime, etc)
 * @parm source: is the thing to target after taking damage
 *               creature or NULL
 */
void P_DamageMobj2(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                   int damageP, boolean stomping)
{
    unsigned ang;
    int     saved;
    player_t *player;
    fixed_t thrust;
    int     temp;

    // the actual damage (== damageP * netMobDamageModifier for
    // any non-player mob)
    int     damage = damageP;

    // Clients can't harm anybody.
    if(IS_CLIENT)
        return;

    if(!(target->flags & MF_SHOOTABLE))
        return;                 // shouldn't happen...

    if(target->health <= 0)
        return;

    if(target->flags & MF_SKULLFLY)
    {
        target->mom[MX] = target->mom[MY] = target->mom[MZ] = 0;
    }

    player = target->player;
    if(player && gameskill == SM_BABY)
        damage >>= 1;           // take half damage in trainer mode

    // Use the cvar damage multiplier netMobDamageModifier only if the
    // inflictor is not a player.
    if(inflictor && !inflictor->player &&
       (!source || (source && !source->player)))
    {
        // damage = (int) ((float) damage * netMobDamageModifier);
        if(IS_NETGAME)
            damage *= cfg.netMobDamageModifier;
    }

    // Some close combat weapons should not inflict thrust and push the
    // victim out of reach, thus kick away unless using the chainsaw.
    if(inflictor && !(target->flags & MF_NOCLIP) &&
       (!source || !source->player ||
        source->player->readyWeapon != WT_EIGHTH) &&
       !(inflictor->flags2 & MF2_NODMGTHRUST))
    {
        ang =
            R_PointToAngle2(inflictor->pos[VX], inflictor->pos[VY],
                            target->pos[VX], target->pos[VY]);

        thrust = FIX2FLT(damage * (FRACUNIT>>3) * 100 / target->info->mass);

        // make fall forwards sometimes.
        if(damage < 40 && damage > target->health &&
           target->pos[VZ] - inflictor->pos[VZ] > 64 && (P_Random() & 1))
        {
            ang += ANG180;
            thrust *= 4;
        }

        ang >>= ANGLETOFINESHIFT;
        target->mom[MX] += thrust * FIX2FLT(finecosine[ang]);
        target->mom[MY] += thrust * FIX2FLT(finesine[ang]);
        if(target->dPlayer)
        {
            // Only fix momentum. Otherwise clients will find it difficult
            // to escape from the damage inflictor.
            target->dPlayer->flags |= DDPF_FIXMOM;
        }

        // $dropoff_fix: thrust objects hanging off ledges.
        if(target->intFlags & MIF_FALLING && target->gear >= MAXGEAR)
            target->gear = 0;
    }

    // player specific
    if(player)
    {
        // Check if player-player damage is disabled.
        if(source && source->player && source->player != player)
        {
            // Co-op damage disabled?
            if(IS_NETGAME && !deathmatch && cfg.noCoopDamage)
                return;
            // Same color, no damage?
            if(cfg.noTeamDamage &&
               cfg.playerColor[player - players] ==
               cfg.playerColor[source->player - players])
                return;
        }

        // end of game hell hack
        if(P_ToXSectorOfSubsector(target->subsector)->special == 11
           && damage >= target->health)
        {
            damage = target->health - 1;
        }

        // Below certain threshold,
        // ignore damage in GOD mode, or with INVUL power.
        if(damage < 1000 &&
           ((P_GetPlayerCheats(player) & CF_GODMODE) ||
            player->powers[PT_INVULNERABILITY]))
        {
            return;
        }

        if(player->armorType)
        {
            if(player->armorType == 1)
                saved = damage / 3;
            else
                saved = damage / 2;

            if(player->armorPoints <= saved)
            {
                // armor is used up
                saved = player->armorPoints;
                player->armorType = 0;
            }
            player->armorPoints -= saved;
            player->update |= PSF_ARMOR_POINTS;
            damage -= saved;
        }
        player->health -= damage;   // mirror mobj health here for Dave
        if(player->health < 0)
            player->health = 0;
        player->update |= PSF_HEALTH;

        player->attacker = source;
        player->damageCount += damage;  // add damage after armor / invuln

        if(player->damageCount > 100)
            player->damageCount = 100;  // teleport stomp does 10k points...

        temp = damage < 100 ? damage : 100;

        // Maybe unhide the HUD?
        if(player == &players[CONSOLEPLAYER]);
            ST_HUDUnHide(HUE_ON_DAMAGE);
    }

    // How about some particles, yes?
    // Only works when both target and inflictor are real mobjs.
    P_SpawnDamageParticleGen(target, inflictor, damage);

    // do the damage
    target->health -= damage;
    if(target->health <= 0)
    {
        P_KillMobj(source, target, stomping);
        return;
    }

    if((P_Random() < target->info->painChance) &&
       !(target->flags & MF_SKULLFLY))
    {
        target->flags |= MF_JUSTHIT;    // fight back!

        P_MobjChangeState(target, target->info->painState);
    }

    target->reactionTime = 0;   // we're awake now...

    if(source &&
       ((!target->threshold && !(source->flags3 & MF3_NOINFIGHT))|| target->type == MT_VILE) &&
       source != target && source->type != MT_VILE)
    {
        // if not intent on another player,
        // chase after this one
        target->target = source;
        target->threshold = BASETHRESHOLD;
        if(target->state == &states[target->info->spawnState] &&
           target->info->seeState != S_NULL)
            P_MobjChangeState(target, target->info->seeState);
    }
}
