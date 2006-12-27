/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1993-1996 by id Software, Inc.
 *
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
 * Handling interactions (i.e., collisions).
 */

#ifdef WIN32
#  pragma optimize("g", off)
#endif

// HEADER FILES ------------------------------------------------------------

#include "doom64tc.h"

#include "am_map.h"
#include "d_net.h"
#include "dmu_lib.h"
#include "p_player.h"
#include "p_map.h"

// MACROS ------------------------------------------------------------------

#define BONUSADD    6

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// a weapon is found with two clip loads,
// a big item has five clip loads
int     maxammo[NUMAMMO] = { 200, 50, 300, 50 };
int     clipammo[NUMAMMO] = { 10, 4, 20, 1 };

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * @param Num:      Number of clip loads, not the individual count.
 * @return boolean  (FALSE) if the ammo can't be picked up at all.
 */
boolean P_GiveAmmo(player_t *player, ammotype_t ammo, int num)
{
    if(ammo == AM_NOAMMO)
        return false;

    if(ammo < 0 || ammo > NUMAMMO)
        Con_Error("P_GiveAmmo: bad type %i", ammo);

    if(player->ammo[ammo] == player->maxammo[ammo])
        return false;

    if(num)
        num *= clipammo[ammo];
    else
        num = clipammo[ammo] / 2;

    if(gameskill == sk_baby || gameskill == sk_nightmare)
    {
        // give double ammo in trainer mode,
        // you'll need in nightmare
        num <<= 1;
    }

    // We are about to receive some more ammo. Does the player want to
    // change weapon automatically?
    P_MaybeChangeWeapon(player, WP_NOCHANGE, ammo, false);

    player->ammo[ammo] += num;
    player->update |= PSF_AMMO;

    if(player->ammo[ammo] > player->maxammo[ammo])
        player->ammo[ammo] = player->maxammo[ammo];

    // Maybe unhide the HUD?
    ST_HUDUnHide(HUE_ON_PICKUP_AMMO);

    return true;
}

/*
 * The weapon name may have a MF_DROPPED flag ored in.
 */
boolean P_GiveWeapon(player_t *player, weapontype_t weapon, boolean dropped)
{
    ammotype_t i;
    boolean gaveammo = false;
    boolean gaveweapon;
    int     numClips;

    if(IS_NETGAME && (deathmatch != 2) && !dropped)
    {
        // leave placed weapons forever on net games
        if(player->weaponowned[weapon])
            return false;

        player->bonuscount += BONUSADD;
        player->weaponowned[weapon] = true;
        player->update |= PSF_OWNED_WEAPONS;

        // Give some of each of the ammo types used by this weapon.
        for(i=0; i < NUMAMMO; ++i)
        {
            if(!weaponinfo[weapon][player->class].mode[0].ammotype[i])
                continue;   // Weapon does not take this type of ammo.

            if(deathmatch)
                numClips = 5;
            else
                numClips = 2;

            if(P_GiveAmmo(player, i, numClips))
                gaveammo = true; // at least ONE type of ammo was given.
        }

        // Should we change weapon automatically?
        P_MaybeChangeWeapon(player, weapon, AM_NOAMMO, deathmatch == 1);

        S_ConsoleSound(sfx_wpnup, NULL, player - players);
        return false;
    }
    else
    {
        // Give some of each of the ammo types used by this weapon.
        for(i=0; i < NUMAMMO; ++i)
        {
            if(!weaponinfo[weapon][player->class].mode[0].ammotype[i])
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

        if(player->weaponowned[weapon])
            gaveweapon = false;
        else
        {
            gaveweapon = true;
            player->weaponowned[weapon] = true;
            player->update |= PSF_OWNED_WEAPONS;

            // Should we change weapon automatically?
            P_MaybeChangeWeapon(player, weapon, AM_NOAMMO, false);
        }

        // Maybe unhide the HUD?
        if(gaveweapon)
            ST_HUDUnHide(HUE_ON_PICKUP_WEAPON);

        return (gaveweapon || gaveammo);
    }
}

/*
 * Returns false if the body isn't needed at all
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
    ST_HUDUnHide(HUE_ON_PICKUP_HEALTH);

    return true;
}

/*
 * Returns false if the armor is worse than the current armor.
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
    if(player->armorpoints >= hits)
        return false;           // don't pick up

    player->armortype = armortype;
    player->armorpoints = hits;
    player->update |= PSF_ARMOR_TYPE | PSF_ARMOR_POINTS;

    // Maybe unhide the HUD?
    ST_HUDUnHide(HUE_ON_PICKUP_ARMOR);

    return true;
}

void P_GiveKey(player_t *player, card_t card)
{
    if(player->keys[card])
        return;

    player->bonuscount = BONUSADD;
    player->keys[card] = 1;
    player->update |= PSF_KEYS;

    // Maybe unhide the HUD?
    ST_HUDUnHide(HUE_ON_PICKUP_KEY);
}

/**
 * d64tc
 */
boolean P_GiveArtifact(player_t *player, laserpw_t artifact)
{
    if(player->artifacts[artifact])
        return false;

    player->bonuscount = BONUSADD;
    player->artifacts[artifact] = 1;
    return true;
}

void P_GiveBackpack(player_t *player)
{
    int     i;

    if(!player->backpack)
    {
        player->update |= PSF_MAX_AMMO;
        for(i = 0; i < NUMAMMO; i++)
            player->maxammo[i] *= 2;
        player->backpack = true;
    }
    for(i = 0; i < NUMAMMO; i++)
        P_GiveAmmo(player, i, 1);
    P_SetMessage(player, GOTBACKPACK, false);
}

boolean P_GivePower(player_t *player, int power)
{
    player->update |= PSF_POWERS;

    switch (power)
    {
    case pw_invulnerability:
        player->powers[power] = INVULNTICS;
        break;

    case pw_invisibility:
        player->powers[power] = INVISTICS;
        player->plr->mo->flags |= MF_SHADOW;;
        break;

    case pw_flight:
        player->powers[power] = 1;
        player->plr->mo->flags2 |= MF2_FLY;
        player->plr->mo->flags |= MF_NOGRAVITY;
        if(player->plr->mo->pos[VZ] <= player->plr->mo->floorz)
        {
            player->flyheight = 10; // thrust the player in the air a bit
            player->plr->mo->flags |= DDPF_FIXMOM;
        }
        break;

    case pw_infrared:
        player->powers[power] = INFRATICS;
        break;

    case pw_ironfeet:
        player->powers[power] = IRONTICS;
        break;

    case pw_strength:
        P_GiveBody(player, maxhealth);
        player->powers[power] = 1;
        break;

    case pw_unsee: // d64tc
        player->powers[power] = UNSEETICS;
        break;

    default:
        if(player->powers[power])
            return false;           // already got it

        player->powers[power] = 1;
        break;
    }

    // Maybe unhide the HUD?
    ST_HUDUnHide(HUE_ON_PICKUP_POWER);
    return true;
}

boolean P_TakePower(player_t *player, int power)
{
    mobj_t *plrmo = player->plr->mo;

    player->update |= PSF_POWERS;
    if(player->powers[pw_flight])
    {
        if(plrmo->pos[VZ] != plrmo->floorz)
        {
            player->centering = true;
        }

        plrmo->flags2 &= ~MF2_FLY;
        plrmo->flags &= ~MF_NOGRAVITY;
        player->powers[power] = 0;
        //BorderTopRefresh = true; //make sure the sprite's cleared out
        GL_Update(DDUF_TOP);
        return true;
    }

    if(!player->powers[power])
        return false;           // dont got it

    player->powers[power] = 0;
    return true;
}

void P_TouchSpecialThing(mobj_t *special, mobj_t *toucher)
{
    player_t *player;
    fixed_t delta;
    int     sound;

    delta = special->pos[VZ] - toucher->pos[VZ];

    if(delta > toucher->height || delta < -8 * FRACUNIT)
    {
        // out of reach
        return;
    }

    sound = sfx_itemup;
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
        //player->health++;       // can go over 100%
        player->health += 2;      // d64tc can go over 100%
        if(player->health > healthlimit)
            player->health = healthlimit;
        player->plr->mo->health = player->health;
        player->update |= PSF_HEALTH;
        P_SetMessage(player, GOTHTHBONUS, false);

        // Maybe unhide the HUD?
        ST_HUDUnHide(HUE_ON_PICKUP_HEALTH);
        break;

    case SPR_BON2:
        //player->armorpoints++;  // can go over 100%
        player->armorpoints += 2; // d64tc can go over 100%
        if(player->armorpoints > armorpoints[1])    // 200
            player->armorpoints = armorpoints[1];
        if(!player->armortype)
            player->armortype = armorclass[0];
        player->update |= PSF_ARMOR_TYPE | PSF_ARMOR_POINTS;
        P_SetMessage(player, GOTARMBONUS, false);

        // Maybe unhide the HUD?
        ST_HUDUnHide(HUE_ON_PICKUP_ARMOR);
        break;

    case SPR_BON3: // d64tc
        player->health += 2;
        if(player->health > healthlimit)
            player->health = healthlimit;
        player->plr->mo->health = player->health;

        player->armorpoints += 2;
        if(player->armorpoints > armorpoints[1]) // 200
            player->armorpoints = armorpoints[1];
        if(!player->armortype)
            player->armortype = armorclass[0];

        player->update |= PSF_HEALTH | PSF_ARMOR_TYPE | PSF_ARMOR_POINTS;
        P_SetMessage(player, GOTHELLBONUS, false);

        // Maybe unhide the HUD?
        ST_HUDUnHide(HUE_ON_PICKUP_HEALTH);
        ST_HUDUnHide(HUE_ON_PICKUP_ARMOR);
        break;

    case SPR_SOUL:
        player->health += soulspherehealth;
        if(player->health > soulspherelimit)
            player->health = soulspherelimit;
        player->plr->mo->health = player->health;
        player->update |= PSF_HEALTH;
        P_SetMessage(player, GOTSUPER, false);
        sound = sfx_getpow;

        // Maybe unhide the HUD?
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
        sound = sfx_getpow;

        // Maybe unhide the HUD?
        ST_HUDUnHide(HUE_ON_PICKUP_HEALTH);
        break;

        // cards
        // leave cards for everyone
    case SPR_BKEY:
        if(!player->keys[it_bluecard])
            P_SetMessage(player, GOTBLUECARD, false);
        P_GiveKey(player, it_bluecard);
        if(!IS_NETGAME)
            break;
        return;

    case SPR_YKEY:
        if(!player->keys[it_yellowcard])
            P_SetMessage(player, GOTYELWCARD, false);
        P_GiveKey(player, it_yellowcard);
        if(!IS_NETGAME)
            break;
        return;

    case SPR_RKEY:
        if(!player->keys[it_redcard])
            P_SetMessage(player, GOTREDCARD, false);
        P_GiveKey(player, it_redcard);
        if(!IS_NETGAME)
            break;
        return;

    case SPR_BSKU:
        if(!player->keys[it_blueskull])
            P_SetMessage(player, GOTBLUESKUL, false);
        P_GiveKey(player, it_blueskull);
        if(!IS_NETGAME)
            break;
        return;

    case SPR_YSKU:
        if(!player->keys[it_yellowskull])
            P_SetMessage(player, GOTYELWSKUL, false);
        P_GiveKey(player, it_yellowskull);
        if(!IS_NETGAME)
            break;
        return;

    case SPR_RSKU:
        if(!player->keys[it_redskull])
            P_SetMessage(player, GOTREDSKULL, false);
        P_GiveKey(player, it_redskull);
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
        if(!P_GivePower(player, pw_invulnerability))
            return;
        P_SetMessage(player, GOTINVUL, false);
        sound = sfx_getpow;
        break;

    case SPR_PSTR:
        if(!P_GivePower(player, pw_strength))
            return;
        P_SetMessage(player, GOTBERSERK, false);
        if(player->readyweapon != wp_fist && cfg.berserkAutoSwitch)
        {
            player->pendingweapon = wp_fist;
            player->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;
        }
        sound = sfx_getpow;
        break;

    case SPR_PINS:
        if(!P_GivePower(player, pw_invisibility))
            return;
        P_SetMessage(player, GOTINVIS, false);
        sound = sfx_getpow;
        break;

    case SPR_SUIT:
        if(!P_GivePower(player, pw_ironfeet))
            return;
        P_SetMessage(player, GOTSUIT, false);
        sound = sfx_getpow;
        break;

    case SPR_PMAP:
        if(!P_GivePower(player, pw_allmap))
            return;
        P_SetMessage(player, GOTMAP, false);
        sound = sfx_getpow;
        break;

    case SPR_RMAP: // d64tc
        if(!P_GivePower(player, pw_radar))
            return;

        P_SetMessage(player, GOTRADAR, false);
        sound = sfx_getpow;
        break;

    case SPR_DETH: // d64tc
        P_Massacre();
        P_SetMessage(player, ALLALONE, false);
        sound = sfx_getpow;
        break;

    case SPR_SEEA: // d64tc
        if(!P_GivePower(player, pw_unsee))
            return;

        P_SetMessage(player, GOTUNSEE, false);
        sound = sfx_getpow;
        break;

    case SPR_POW4: // d64tc
        if(player->weaponowned[wp_unmaker])
        {
            if(!P_GiveArtifact(player, it_float))
                return;

            P_SetMessage(player, GOTFLOATER, false);
            sound = sfx_getpow;
        }
        else
        {
            if(!(leveltime & 0x1f))
                P_SetMessage(player, NGOTUNMAKER, false);

            return; //Don't destroy item, can be collected later by other players
        }
        break;

    case SPR_POW5: // d64tc
        if(player->weaponowned[wp_unmaker])
        {
            if(!P_GiveArtifact(player, it_helltime))
                return;

            P_SetMessage(player, GOTHELLTIME, false);
            sound = sfx_getpow;
        }
        else
        {
            if(!(leveltime & 0x1f))
                P_SetMessage(player, NGOTUNMAKER, false);

            return; //Don't destroy item, can be collected later by other players
        }
        break;

    case SPR_PVIS:
        if(!P_GivePower(player, pw_infrared))
            return;
        P_SetMessage(player, GOTVISOR, false);
        sound = sfx_getpow;
        break;

        // ammo
    case SPR_CLIP:
        if(special->flags & MF_DROPPED)
        {
            if(!P_GiveAmmo(player, am_clip, 0))
                return;
        }
        else
        {
            if(!P_GiveAmmo(player, am_clip, 1))
                return;
        }
        P_SetMessage(player, GOTCLIP, false);
        break;

    case SPR_AMMO:
        if(!P_GiveAmmo(player, am_clip, 5))
            return;
        P_SetMessage(player, GOTCLIPBOX, false);
        break;

    case SPR_ROCK:
        if(!P_GiveAmmo(player, am_misl, 1))
            return;
        P_SetMessage(player, GOTROCKET, false);
        break;

    case SPR_BROK:
        if(!P_GiveAmmo(player, am_misl, 5))
            return;
        P_SetMessage(player, GOTROCKBOX, false);
        break;

    case SPR_CELL:
        if(!P_GiveAmmo(player, am_cell, 1))
            return;
        P_SetMessage(player, GOTCELL, false);
        break;

    case SPR_CELP:
        if(!P_GiveAmmo(player, am_cell, 5))
            return;
        P_SetMessage(player, GOTCELLBOX, false);
        break;

    case SPR_SHEL:
        if(!P_GiveAmmo(player, am_shell, 1))
            return;
        P_SetMessage(player, GOTSHELLS, false);
        break;

    case SPR_SBOX:
        if(!P_GiveAmmo(player, am_shell, 5))
            return;
        P_SetMessage(player, GOTSHELLBOX, false);
        break;

    case SPR_BPAK:
        P_GiveBackpack(player);
        break;

        // weapons
    case SPR_BFUG:
        if(!P_GiveWeapon(player, wp_bfg, false))
            return;
        P_SetMessage(player, GOTBFG9000, false);
        sound = sfx_wpnup;
        break;

    case SPR_MGUN:
        if(!P_GiveWeapon(player, wp_chaingun, special->flags & MF_DROPPED))
            return;
        P_SetMessage(player, GOTCHAINGUN, false);
        sound = sfx_wpnup;
        break;

    case SPR_CSAW:
        if(!P_GiveWeapon(player, wp_chainsaw, false))
            return;
        P_SetMessage(player, GOTCHAINSAW, false);
        sound = sfx_wpnup;
        break;

    case SPR_LAUN:
        if(!P_GiveWeapon(player, wp_missile, false))
            return;
        P_SetMessage(player, GOTLAUNCHER, false);
        sound = sfx_wpnup;
        break;

    case SPR_PLAS:
        if(!P_GiveWeapon(player, wp_plasma, false))
            return;
        P_SetMessage(player, GOTPLASMA, false);
        sound = sfx_wpnup;
        break;

    case SPR_SHOT:
        if(!P_GiveWeapon(player, wp_shotgun, special->flags & MF_DROPPED))
            return;
        P_SetMessage(player, GOTSHOTGUN, false);
        sound = sfx_wpnup;
        break;

    case SPR_SGN2:
        if(!P_GiveWeapon(player, wp_supershotgun, special->flags & MF_DROPPED))
            return;
        P_SetMessage(player, GOTSHOTGUN2, false);
        sound = sfx_wpnup;
        break;

    case SPR_LGUN: // d64tc
        if(!P_GiveWeapon(player, wp_unmaker, special->flags & MF_DROPPED))
            return;

        P_SetMessage(player, GOTUNMAKER, false);
        sound = sfx_wpnup;
        break;

    case SPR_POW1: // d64tc
        if(player->lasericon1 == 0)
        {
            P_GiveArtifact(player, it_laserpw1);

            player->laserpw += 1;
            player->lasericon1 = 1;
            P_SetMessage(player, GOTPOWERUP1, false);
        }
        else
        {
            if(!(leveltime & 0x1f))
                P_SetMessage(player, NGOTPOWERUP1, false);

            return; //Don't destroy item, can be collected later by other players
        }
        break;

    case SPR_POW2: // d64tc
        if(player->lasericon2 == 0)
        {
            P_GiveArtifact(player, it_laserpw2);

            player->laserpw += 1;
            player->lasericon2 = 1;
            P_SetMessage(player, GOTPOWERUP2, false);
        }
        else
        {
            if(!(leveltime & 0x1f))
                P_SetMessage(player, NGOTPOWERUP2, false);

            return; //Don't destroy item, can be collected later by other players
        }
        break;

    case SPR_POW3: // d64tc
        if(player->lasericon3 == 0)
        {
            P_GiveArtifact(player, it_laserpw3);

            player->laserpw += 1;
            player->lasericon3 = 1;
            P_SetMessage(player, GOTPOWERUP3, false);
        }
        else
        {
            if(!(leveltime & 0x1f))
                P_SetMessage(player, NGOTPOWERUP3, false);

            return; //Don't destroy item, can be collected later by other players
        }
        break;

    default:
        Con_Error("P_SpecialThing: Unknown gettable thing");
    }

    if(special->flags & MF_COUNTITEM)
        player->itemcount++;
    P_RemoveMobj(special);
    player->bonuscount += BONUSADD;
    /*if (player == &players[consoleplayer])
       S_StartSound (NULL, sound); */
    S_ConsoleSound(sound, NULL, player - players);
}

void P_KillMobj(mobj_t *source, mobj_t *target, boolean stomping)
{
    mobjtype_t item;
    mobj_t *mo;
    //angle_t angle; d64tc

    if(!target) // nothing to kill
        return;

    target->flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY);

    if(target->type != MT_SKULL)
        target->flags &= ~MF_NOGRAVITY;

    target->flags |= MF_CORPSE | MF_DROPOFF;
    target->flags2 &= ~MF2_PASSMOBJ;
    target->corpsetics = 0;
    // target->height >>= 2; // d64tc

    if(source && source->player)
    {
        // count for intermission
        if(target->flags & MF_COUNTKILL)
            source->player->killcount++;

        if(target->player)
        {
            source->player->frags[target->player - players]++;
            NetSv_FragsForAll(source->player);
            NetSv_KillMessage(source->player, target->player, stomping);
        }
    }
    else if(!IS_NETGAME && (target->flags & MF_COUNTKILL))
    {
        // count all monster deaths,
        // even those caused by other monsters
        players[0].killcount++;
    }

    if(target->player)
    {
        // count environment kills against you
        if(!source)
        {
            target->player->frags[target->player - players]++;
            NetSv_FragsForAll(target->player);
            NetSv_KillMessage(target->player, target->player, stomping);
        }

        target->flags &= ~MF_SOLID;
        target->flags2 &= ~MF2_FLY;
        target->player->powers[pw_flight] = 0;
        target->player->playerstate = PST_DEAD;
        target->player->update |= PSF_STATE;
        target->player->plr->flags |= DDPF_DEAD;
        P_DropWeapon(target->player);

        if(target->player == &players[consoleplayer] && automapactive)
        {
            // don't die in auto map,
            // switch view prior to dying
            AM_Stop();
        }

    }

    if(target->health < -target->info->spawnhealth &&
       target->info->xdeathstate)
    {
        P_SetMobjState(target, target->info->xdeathstate);
    }
    else
        P_SetMobjState(target, target->info->deathstate);
    target->tics -= P_Random() & 3;

    if(target->tics < 1)
        target->tics = 1;

    //  I_StartSound (&actor->r, actor->info->deathsound);

    // Drop stuff.
    // This determines the kind of object spawned
    // during the death frame of a thing.
    switch (target->type)
    {
    case MT_WOLFSS:
    case MT_POSSESSED:
    //case MT_TROOP: // d64tc
        item = MT_CLIP;
        break;

    case MT_SHOTGUY:
        item = MT_SHOTGUN;
        break;

    //case MT_CHAINGUY:
    case MT_CHAINGUNGUY: // d64tc
        item = MT_CHAINGUN;
        break;

    default:
        return;
    }

    // Don't drop at the exact same place, causes Z flickering with
    // 3D sprites.
    /* d64tc
    angle = M_Random() << (24 - ANGLETOFINESHIFT);
    mo = P_SpawnMobj(target->pos[VX] + 3 * finecosine[angle],
                     target->pos[VY] + 3 * finesine[angle], ONFLOORZ, item);
    */

    mo = P_SpawnMobj(target->pos[VX] + ((M_Random() - M_Random()) << 12),
                     target->pos[VY] + ((M_Random() - M_Random()) << 12),
                     ONFLOORZ, item);

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
        target->momx = target->momy = target->momz = 0;
    }

    player = target->player;
    if(player && gameskill == sk_baby)
        damage >>= 1;           // take half damage in trainer mode

    // d64tc >
#if 0
    if(inflictor && inflictor->type == MT_FIREEND)
    {   // Special for Motherdemon attack
#if 0
        // DJS - This was originally in a sub routine called P_TouchMotherFire
        //       but due to the fact that <code>player</code> was not initialized
        //       this likely does not work the way kaiser expected it to.
        //       What would actually happen is not certain but I would guess it
        //       would most likely simply return without doing anything at all.
        // TODO: SHOULD this be fixed? Or is something implemented elsewhere
        //       which does what this was attempting to do?
        int         damage;
        player_t   *player;

        if(player = target->player)
        {
            damage = ((P_Random() % 10) + 1) * 8;

            P_DamageMobj(target, NULL, NULL, damage);
            player->plr->mo->momz = FRACUNIT * 16;
            player->jumptics = 24;
        }
#endif
        return;
    }
#endif
    // < d64tc

    // use the cvar damage multiplier netMobDamageModifier
    // only if the inflictor is not a player
    if(inflictor && !inflictor->player && (!source || (source && !source->player)))
    {
        //damage = (int) ((float) damage * netMobDamageModifier);
        if(IS_NETGAME)
            damage *= cfg.netMobDamageModifier;
    }

    // Some close combat weapons should not
    // inflict thrust and push the victim out of reach,
    // thus kick away unless using the chainsaw.
    if(inflictor && !(target->flags & MF_NOCLIP) &&
       (!source || !source->player ||
        source->player->readyweapon != wp_chainsaw) &&
       !(inflictor->flags2 & MF2_NODMGTHRUST))
    {
        ang =
            R_PointToAngle2(inflictor->pos[VX], inflictor->pos[VY],
                            target->pos[VX], target->pos[VY]);

        thrust = damage * (FRACUNIT >> 3) * 100 / target->info->mass;

        // make fall forwards sometimes
        if(damage < 40 && damage > target->health &&
           target->pos[VZ] - inflictor->pos[VZ] > 64 * FRACUNIT && (P_Random() & 1))
        {
            ang += ANG180;
            thrust *= 4;
        }

        ang >>= ANGLETOFINESHIFT;
        target->momx += FixedMul(thrust, finecosine[ang]);
        target->momy += FixedMul(thrust, finesine[ang]);
        if(target->dplayer)
        {
            // Only fix momentum. Otherwise clients will find it difficult
            // to escape from the damage inflictor.
            target->dplayer->flags |= DDPF_FIXMOM;
        }

        // killough $dropoff_fix: thrust objects hanging off ledges
        if(target->intflags & MIF_FALLING && target->gear >= MAXGEAR)
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
               cfg.PlayerColor[player - players] ==
               cfg.PlayerColor[source->player - players])
                return;
        }

        // end of game hell hack
        if(P_XSectorOfSubsector(target->subsector)->special == 11
           && damage >= target->health)
        {
            damage = target->health - 1;
        }

        // Below certain threshold,
        // ignore damage in GOD mode, or with INVUL power.
        if(damage < 1000 &&
           ((P_GetPlayerCheats(player) & CF_GODMODE) ||
            player->powers[pw_invulnerability]))
        {
            return;
        }

        if(player->armortype)
        {
            if(player->armortype == 1)
                saved = damage / 3;
            else
                saved = damage / 2;

            if(player->armorpoints <= saved)
            {
                // armor is used up
                saved = player->armorpoints;
                player->armortype = 0;
            }
            player->armorpoints -= saved;
            player->update |= PSF_ARMOR_POINTS;
            damage -= saved;
        }
        player->health -= damage;   // mirror mobj health here for Dave
        if(player->health < 0)
            player->health = 0;
        player->update |= PSF_HEALTH;

        player->attacker = source;
        player->damagecount += damage;  // add damage after armor / invuln

        if(player->damagecount > 100)
            player->damagecount = 100;  // teleport stomp does 10k points...

        temp = damage < 100 ? damage : 100;

        // Maybe unhide the HUD?
        if(player == &players[consoleplayer]);
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

    if((P_Random() < target->info->painchance) &&
       !(target->flags & MF_SKULLFLY))
    {
        target->flags |= MF_JUSTHIT;    // fight back!

        P_SetMobjState(target, target->info->painstate);
    }

    target->reactiontime = 0;   // we're awake now...

    if(source &&
       (!target->threshold && !(source->flags3 & MF3_NOINFIGHT)) && source != target)
    {
        // if not intent on another player,
        // chase after this one
        target->target = source;
        target->threshold = BASETHRESHOLD;
        if(target->state == &states[target->info->spawnstate] &&
           target->info->seestate != S_NULL)
            P_SetMobjState(target, target->info->seestate);
    }
}
