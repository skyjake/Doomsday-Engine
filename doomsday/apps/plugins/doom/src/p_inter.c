/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * p_inter.c: Handling mobj vs mobj interactions (i.e., collisions).
 */

#ifdef MSVC
#  pragma optimize("g", off)
#endif

#include "jdoom.h"
#include "p_inter.h"

#include "d_net.h"
#include "d_netsv.h"
#include "dmu_lib.h"
#include "player.h"
#include "p_user.h"
#include "p_mapsetup.h"

#define BONUSADD            (6)

// Number of rounds per clip for each ammo type.
int clipAmmo[NUM_AMMO_TYPES] = {10, 4, 20, 1};

// Maximum number of rounds for each ammo type.
int maxAmmo[NUM_AMMO_TYPES] = {200, 50, 300, 50};

static dd_bool giveOneAmmo(player_t *plr, ammotype_t ammoType, int numClips)
{
    int numRounds = 0;

    DENG_ASSERT(plr != 0);
    DENG_ASSERT(((int)ammoType >= 0 && ammoType < NUM_AMMO_TYPES) || ammoType == AT_NOAMMO);

    // Giving the special 'unlimited ammo' type always succeeds.
    if(ammoType == AT_NOAMMO)
        return true;

    // Already fully stocked?
    if(plr->ammo[ammoType].owned >= plr->ammo[ammoType].max)
        return false;

    // Translate number of clips to individual rounds.
    if(numClips >= 1)
    {
        numRounds = numClips * clipAmmo[ammoType];
    }
    else if(numClips == 0)
    {
        // Half of one clip.
        numRounds = clipAmmo[ammoType] / 2;
    }
    else
    {
        // Fully replenish.
        numRounds = plr->ammo[ammoType].max;
    }

    // Give double the number of rounds at easy/nightmare skill levels.
    if(gfw_Rule(skill) == SM_BABY ||
       gfw_Rule(skill) == SM_NIGHTMARE)
    {
        numRounds *= 2;
    }

    // Given the new ammo the player may want to change weapon automatically.
    P_MaybeChangeWeapon(plr, WT_NOCHANGE, ammoType, false /*don't force*/);

    // Restock the player.
    plr->ammo[ammoType].owned = MIN_OF(plr->ammo[ammoType].max,
                                       plr->ammo[ammoType].owned + numRounds);
    plr->update |= PSF_AMMO;

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_AMMO);

    return true;
}

dd_bool P_GiveAmmo(player_t *plr, ammotype_t ammoType, int numClips)
{
    int gaveAmmos = 0;

    if(ammoType == NUM_AMMO_TYPES)
    {
        // Give all ammos.
        int i = 0;
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
        {
            gaveAmmos  |= (int)giveOneAmmo(plr, (ammotype_t) i, numClips) << i;
        }
    }
    else
    {
        // Give a single ammo.
        gaveAmmos  |= (int)giveOneAmmo(plr, ammoType, numClips) << (int)ammoType;
    }

    return gaveAmmos  != 0;
}

static dd_bool shouldForceWeaponChange(dd_bool dropped)
{
    return IS_NETGAME && gfw_Rule(deathmatch) == 1 && !dropped;
}

static int numAmmoClipsToGiveWithWeapon(dd_bool dropped)
{
    // Dropped weapons only ever give one clip.
    if(dropped) return 1;

    // Give extra clips in deathmatch.
    return (IS_NETGAME && gfw_Rule(deathmatch) == 1)? 5 : 2;
}

static dd_bool giveOneWeapon(player_t *plr, weapontype_t weaponType, dd_bool dropped)
{
    int numClips = numAmmoClipsToGiveWithWeapon(dropped);
    dd_bool gaveAmmo = false, gaveWeapon = false;
    weaponinfo_t const *wpnInfo;
    ammotype_t i;

    DENG_ASSERT(plr != 0);
    DENG_ASSERT(weaponType >= WT_FIRST && weaponType < NUM_WEAPON_TYPES);

    wpnInfo = &weaponInfo[weaponType][plr->class_];

    // Do not give weapons unavailable for the current mode.
    if(!(wpnInfo->mode[0].gameModeBits & gameModeBits))
        return false;

    // Give some of each of the ammo types used by this weapon.
    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        // Is this ammo type usable?.
        if(!wpnInfo->mode[0].ammoType[i])
            continue;

        if(P_GiveAmmo(plr, i, numClips))
        {
            gaveAmmo = true;
        }
    }

    if(!plr->weapons[weaponType].owned)
    {
        gaveWeapon = true;

        plr->weapons[weaponType].owned = true;
        plr->update |= PSF_OWNED_WEAPONS;

        // Animate a pickup bonus flash?
        if(IS_NETGAME && gfw_Rule(deathmatch) != 2 && !dropped)
        {
            plr->bonusCount += BONUSADD;
        }

        // Given the new weapon the player may want to change automatically.
        P_MaybeChangeWeapon(plr, weaponType, AT_NOAMMO,
                            shouldForceWeaponChange(dropped));

        // Maybe unhide the HUD?
        ST_HUDUnHide(plr - players, HUE_ON_PICKUP_WEAPON);
    }

    return (gaveWeapon || gaveAmmo);
}

dd_bool P_GiveWeapon(player_t *plr, weapontype_t weaponType, dd_bool dropped)
{
    int gaveWeapons = 0;

    if(weaponType == NUM_WEAPON_TYPES)
    {
        // Give all weapons.
        int i = 0;
        for(i = 0; i < NUM_WEAPON_TYPES; ++i)
        {
            gaveWeapons |= (int)giveOneWeapon(plr, (weapontype_t) i, dropped) << i;
        }
    }
    else
    {
        // Give a single weapon.
        gaveWeapons |= (int)giveOneWeapon(plr, weaponType, dropped) << (int)weaponType;
    }

    return gaveWeapons != 0;
}

dd_bool P_GiveHealth(player_t *player, int amount)
{
    if(player->health >= maxHealth)
        return false;

    player->health += amount;
    if(player->health > maxHealth)
        player->health = maxHealth;

    player->plr->mo->health = player->health;
    player->update |= PSF_HEALTH;

    // Maybe unhide the HUD?
    ST_HUDUnHide(player - players, HUE_ON_PICKUP_HEALTH);

    return true;
}

dd_bool P_GiveArmor(player_t* plr, int type, int points)
{
    if(plr->armorPoints >= points)
        return false; // Don't pick up.

    P_PlayerSetArmorType(plr, type);
    P_PlayerGiveArmorBonus(plr, points - plr->armorPoints);

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_ARMOR);

    return true;
}

static dd_bool giveOneKey(player_t *plr, keytype_t keyType)
{
    DENG_ASSERT(plr != 0);
    DENG_ASSERT(keyType >= KT_FIRST && keyType < NUM_KEY_TYPES);

    // Already owned?
    if(plr->keys[keyType]) return false;

    plr->keys[keyType] = 1;
    plr->bonusCount = BONUSADD;
    plr->update |= PSF_KEYS;

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_KEY);

    return true;
}

dd_bool P_GiveKey(player_t *plr, keytype_t keyType)
{
    int gaveKeys = 0;

    if(keyType == NUM_KEY_TYPES)
    {
        // Give all keys.
        int i = 0;
        for(i = 0; i < NUM_KEY_TYPES; ++i)
        {
            gaveKeys |= (int)giveOneKey(plr, (keytype_t) i) << i;
        }
    }
    else
    {
        // Give a single key.
        gaveKeys |= (int)giveOneKey(plr, keyType) << (int)keyType;
    }

    return gaveKeys != 0;
}

void P_GiveBackpack(player_t *plr)
{
    int i;

    if(!plr->backpack)
    {
        plr->update |= PSF_MAX_AMMO;
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
        {
            plr->ammo[i].max *= 2;
        }

        plr->backpack = true;
    }

    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        P_GiveAmmo(plr, i, 1);
    }

    P_SetMessage(plr, GOTBACKPACK);
}

dd_bool P_GivePower(player_t *player, powertype_t powerType)
{
    DENG_ASSERT(player != 0);
    DENG_ASSERT(powerType >= PT_FIRST && powerType < NUM_POWER_TYPES);

    // Powers cannot be given to dead players.
    if(player->health <= 0) return false;

    player->update |= PSF_POWERS;

    switch(powerType)
    {
    case PT_INVULNERABILITY:
        player->powers[powerType] = INVULNTICS;
        break;

    case PT_INVISIBILITY:
        player->powers[powerType] = INVISTICS;
        player->plr->mo->flags |= MF_SHADOW;
        break;

    case PT_FLIGHT:
        player->powers[powerType] = 1;
        player->plr->mo->flags2 |= MF2_FLY;
        player->plr->mo->flags |= MF_NOGRAVITY;
        if(player->plr->mo->origin[VZ] <= player->plr->mo->floorZ)
        {
            player->flyHeight = 10; // Thrust the player in the air a bit.
            player->plr->mo->flags |= DDPF_FIXMOM;
        }
        break;

    case PT_INFRARED:
        player->powers[powerType] = INFRATICS;
        break;

    case PT_IRONFEET:
        player->powers[powerType] = IRONTICS;
        break;

    case PT_STRENGTH:
        P_GiveHealth(player, maxHealth);
        player->powers[powerType] = 1;
        break;

    default:
        if(player->powers[powerType])
            return false; // Already got it.

        player->powers[powerType] = 1;
        break;
    }

    if(powerType == PT_ALLMAP)
        ST_RevealAutomap(player - players, true);

    // Maybe unhide the HUD?
    ST_HUDUnHide(player - players, HUE_ON_PICKUP_POWER);

    return true;
}

dd_bool P_TakePower(player_t *player, powertype_t powerType)
{
    DENG_ASSERT(player != 0);
    DENG_ASSERT(powerType >= PT_FIRST && powerType < NUM_POWER_TYPES);

    if(!player->powers[powerType])
        return false; // Dont got it.

    switch(powerType)
    {
    case PT_ALLMAP:
        ST_RevealAutomap(player - players, false);
        break;

    case PT_FLIGHT: {
        mobj_t *plrmo = player->plr->mo;

        if(plrmo->origin[VZ] != plrmo->floorZ && cfg.common.lookSpring)
        {
            player->centering = true;
        }

        plrmo->flags2 &= ~MF2_FLY;
        plrmo->flags &= ~MF_NOGRAVITY;
        break; }

    default: break;
    }

    player->powers[powerType] = 0;
    player->update |= PSF_POWERS;

    return true;
}

dd_bool P_TogglePower(player_t *player, powertype_t powerType)
{
    DENG_ASSERT(player != 0);
    DENG_ASSERT(powerType >= PT_FIRST && powerType < NUM_POWER_TYPES);

    if(!player->powers[powerType])
    {
        return P_GivePower(player, powerType);
    }
    else
    {
        return P_TakePower(player, powerType);
    }
}

typedef enum {
    IT_NONE = 0,
    IT_HEALTH_PACK,
    IT_HEALTH_KIT,
    IT_HEALTH_BONUS,
    IT_HEALTH_SOULSPHERE,
    IT_ARMOR_GREEN,
    IT_ARMOR_BLUE,
    IT_ARMOR_BONUS,
    IT_WEAPON_BFG,
    IT_WEAPON_CHAINGUN,
    IT_WEAPON_CHAINSAW,
    IT_WEAPON_RLAUNCHER,
    IT_WEAPON_PLASMARIFLE,
    IT_WEAPON_SHOTGUN,
    IT_WEAPON_SSHOTGUN,
    IT_AMMO_CLIP,
    IT_AMMO_CLIP_BOX,
    IT_AMMO_ROCKET,
    IT_AMMO_ROCKET_BOX,
    IT_AMMO_CELL,
    IT_AMMO_CELL_BOX,
    IT_AMMO_SHELL,
    IT_AMMO_SHELL_BOX,
    IT_KEY_BLUE,
    IT_KEY_YELLOW,
    IT_KEY_RED,
    IT_KEY_BLUESKULL,
    IT_KEY_YELLOWSKULL,
    IT_KEY_REDSKULL,
    IT_INVUL,
    IT_BERSERK,
    IT_INVIS,
    IT_SUIT,
    IT_ALLMAP,
    IT_VISOR,
    IT_BACKPACK,
    IT_MEGASPHERE
} itemtype_t;

static itemtype_t getItemTypeBySprite(spritetype_e sprite)
{
    static const struct item_s {
        itemtype_t      type;
        spritetype_e    sprite;
    } items[] = {
        { IT_HEALTH_PACK, SPR_STIM },
        { IT_HEALTH_KIT, SPR_MEDI },
        { IT_HEALTH_BONUS, SPR_BON1 },
        { IT_HEALTH_SOULSPHERE, SPR_SOUL },
        { IT_ARMOR_GREEN, SPR_ARM1 },
        { IT_ARMOR_BLUE, SPR_ARM2 },
        { IT_ARMOR_BONUS, SPR_BON2 },
        { IT_WEAPON_BFG, SPR_BFUG },
        { IT_WEAPON_CHAINGUN, SPR_MGUN },
        { IT_WEAPON_CHAINSAW, SPR_CSAW },
        { IT_WEAPON_RLAUNCHER, SPR_LAUN },
        { IT_WEAPON_PLASMARIFLE, SPR_PLAS },
        { IT_WEAPON_SHOTGUN, SPR_SHOT },
        { IT_WEAPON_SSHOTGUN, SPR_SGN2 },
        { IT_AMMO_CLIP, SPR_CLIP },
        { IT_AMMO_CLIP_BOX, SPR_AMMO },
        { IT_AMMO_ROCKET, SPR_ROCK },
        { IT_AMMO_ROCKET_BOX, SPR_BROK },
        { IT_AMMO_CELL, SPR_CELL },
        { IT_AMMO_CELL_BOX, SPR_CELP },
        { IT_AMMO_SHELL, SPR_SHEL },
        { IT_AMMO_SHELL_BOX, SPR_SBOX },
        { IT_KEY_BLUE, SPR_BKEY },
        { IT_KEY_YELLOW, SPR_YKEY },
        { IT_KEY_RED, SPR_RKEY },
        { IT_KEY_BLUESKULL, SPR_BSKU },
        { IT_KEY_YELLOWSKULL, SPR_YSKU },
        { IT_KEY_REDSKULL, SPR_RSKU },
        { IT_INVUL, SPR_PINV },
        { IT_BERSERK, SPR_PSTR },
        { IT_INVIS, SPR_PINS },
        { IT_SUIT, SPR_SUIT },
        { IT_ALLMAP, SPR_PMAP },
        { IT_VISOR, SPR_PVIS },
        { IT_BACKPACK, SPR_BPAK },
        { IT_MEGASPHERE, SPR_MEGA },
        { IT_NONE, 0 }
    };
    uint                i;

    for(i = 0; items[i].type != IT_NONE; ++i)
        if(items[i].sprite == sprite)
            return items[i].type;

    return IT_NONE;
}

/**
 * Attempt to pickup the found weapon type.
 *
 * @param plr            Player to attempt the pickup.
 * @param weaponType     Weapon type to pickup.
 * @param dropped        @c true= the weapon was dropped by someone.
 * @param pickupMessage  Message to display if picked up.
 *
 * @return  @c true if the player picked up the weapon.
 */
static dd_bool pickupWeapon(player_t *plr, weapontype_t weaponType,
    dd_bool dropped, char const *pickupMessage)
{
    dd_bool pickedWeapon;

    DENG_ASSERT(plr != 0);
    DENG_ASSERT(weaponType >= WT_FIRST && weaponType < NUM_WEAPON_TYPES);

    // Depending on the game rules the player should ignore the weapon.
    if(plr->weapons[weaponType].owned)
    {
        // Leave placed weapons forever on net games.
        if(IS_NETGAME && gfw_Rule(deathmatch) != 2 && !dropped)
            return false;
    }

    // Attempt the pickup.
    pickedWeapon = P_GiveWeapon(plr, weaponType, dropped);
    if(pickedWeapon)
    {
        // Notify the user.
        P_SetMessage(plr, pickupMessage);

        if(!mapSetup) // Pickup sounds are not played during map setup.
        {
            S_ConsoleSound(SFX_WPNUP, NULL, plr - players);
        }
    }

    if(IS_NETGAME && gfw_Rule(deathmatch) != 2 && !dropped)
    {
        // Leave placed weapons forever on net games.
        return false;
    }

    return pickedWeapon;
}

/**
 * @param plr      Player being given item.
 * @param item     Type of item being given.
 * @param dropped  @c true = the item was dropped by some entity.
 *
 * @return  @c true iff the item should be destroyed.
 */
static dd_bool pickupItem(player_t *plr, itemtype_t item, dd_bool dropped)
{
    if(!plr)
        return false;

    switch(item)
    {
    case IT_ARMOR_GREEN:
        if(!P_GiveArmor(plr, armorClass[0],
                        armorPoints[MINMAX_OF(0, armorClass[0] - 1, 1)]))
            return false;

        P_SetMessage(plr, GOTARMOR);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_ARMOR_BLUE:
        if(!P_GiveArmor(plr, armorClass[1],
                        armorPoints[MINMAX_OF(0, armorClass[1] - 1, 1)]))
            return false;
        P_SetMessage(plr, GOTMEGA);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_ARMOR_BONUS:
        if(!plr->armorType)
            P_PlayerSetArmorType(plr, armorClass[0]);
        if(plr->armorPoints < armorPoints[1])
            P_PlayerGiveArmorBonus(plr, 1);

        P_SetMessage(plr, GOTARMBONUS);
        if(!mapSetup)
        {
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
            // Maybe unhide the HUD?
            ST_HUDUnHide(plr - players, HUE_ON_PICKUP_ARMOR);
        }
        break;

    case IT_HEALTH_PACK:
        if(!P_GiveHealth(plr, 10))
            return false;
        P_SetMessage(plr, GOTSTIM);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_HEALTH_KIT: {
        int oldHealth = plr->health;

        /**
         * DOOM bug:
         * The following test was originaly placed AFTER the call to
         * P_GiveHealth thereby making the first outcome impossible as
         * the medikit gives 25 points of health. This resulted that
         * the GOTMEDINEED "Picked up a medikit that you REALLY need"
         * was never used.
         */

        if(!P_GiveHealth(plr, 25)) return false;

        P_SetMessage(plr, GET_TXT((oldHealth < 25)? TXT_GOTMEDINEED : TXT_GOTMEDIKIT));
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break; }

    case IT_HEALTH_BONUS:
        plr->health++; // Can go over 100%.
        if(plr->health > healthLimit)
            plr->health = healthLimit;
        plr->plr->mo->health = plr->health;
        plr->update |= PSF_HEALTH;
        P_SetMessage(plr, GOTHTHBONUS);
        if(!mapSetup)
        {
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
            // Maybe unhide the HUD?
            ST_HUDUnHide(plr - players, HUE_ON_PICKUP_HEALTH);
        }
        break;

    case IT_HEALTH_SOULSPHERE:
        plr->health += soulSphereHealth;
        if(plr->health > soulSphereLimit)
            plr->health = soulSphereLimit;
        plr->plr->mo->health = plr->health;
        plr->update |= PSF_HEALTH;
        P_SetMessage(plr, GOTSUPER);
        if(!mapSetup)
        {
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
            // Maybe unhide the HUD?
            ST_HUDUnHide(plr - players, HUE_ON_PICKUP_HEALTH);
        }
        break;

    case IT_KEY_BLUE:
        if(!plr->keys[KT_BLUECARD])
        {
            P_GiveKey(plr, KT_BLUECARD);
            P_SetMessage(plr, GOTBLUECARD);
            if(!mapSetup)
                S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        }
        if(IS_NETGAME)
            return false;
        break;

    case IT_KEY_YELLOW:
        if(!plr->keys[KT_YELLOWCARD])
        {
            P_GiveKey(plr, KT_YELLOWCARD);
            P_SetMessage(plr, GOTYELWCARD);
            if(!mapSetup)
                S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        }
        if(IS_NETGAME)
            return false;
        break;

    case IT_KEY_RED:
        if(!plr->keys[KT_REDCARD])
        {
            P_GiveKey(plr, KT_REDCARD);
            P_SetMessage(plr, GOTREDCARD);
            if(!mapSetup)
                S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        }
        if(IS_NETGAME)
            return false;
        break;

    case IT_KEY_BLUESKULL:
        if(!plr->keys[KT_BLUESKULL])
        {
            P_GiveKey(plr, KT_BLUESKULL);
            P_SetMessage(plr, GOTBLUESKUL);
            if(!mapSetup)
                S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        }
        if(IS_NETGAME)
            return false;
        break;

    case IT_KEY_YELLOWSKULL:
        if(!plr->keys[KT_YELLOWSKULL])
        {
            P_GiveKey(plr, KT_YELLOWSKULL);
            P_SetMessage(plr, GOTYELWSKUL);
            if(!mapSetup)
                S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        }
        if(IS_NETGAME)
            return false;
        break;

    case IT_KEY_REDSKULL:
        if(!plr->keys[KT_REDSKULL])
        {
            P_GiveKey(plr, KT_REDSKULL);
            P_SetMessage(plr, GOTREDSKULL);
            if(!mapSetup)
                S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        }
        if(IS_NETGAME)
            return false;
        break;

    case IT_MEGASPHERE:
        if(!(gameModeBits & GM_ANY_DOOM2))
            return false;
        plr->health = megaSphereHealth;
        plr->plr->mo->health = plr->health;
        plr->update |= PSF_HEALTH;
        P_GiveArmor(plr, armorClass[1],
                    armorPoints[MINMAX_OF(0, armorClass[1] - 1, 1)]);
        P_SetMessage(plr, GOTMSPHERE);
        if(!mapSetup)
        {
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
            // Maybe unhide the HUD?
            ST_HUDUnHide(plr - players, HUE_ON_PICKUP_HEALTH);
        }
        break;

    case IT_INVUL:
        if(!P_GivePower(plr, PT_INVULNERABILITY))
            return false;

        P_SetMessage(plr, GOTINVUL);
        if(!mapSetup)
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
        break;

    case IT_BERSERK:
        if(!P_GivePower(plr, PT_STRENGTH))
            return false;

        P_SetMessage(plr, GOTBERSERK);
        if(plr->readyWeapon != WT_FIRST && cfg.berserkAutoSwitch)
        {
            plr->pendingWeapon = WT_FIRST;
            plr->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;
        }
        if(!mapSetup)
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
        break;

    case IT_INVIS:
        if(!P_GivePower(plr, PT_INVISIBILITY))
            return false;

        P_SetMessage(plr, GOTINVIS);
        if(!mapSetup)
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
        break;

    case IT_SUIT:
        if(!P_GivePower(plr, PT_IRONFEET))
            return false;

        P_SetMessage(plr, GOTSUIT);
        if(!mapSetup)
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
        break;

    case IT_ALLMAP:
        if(!P_GivePower(plr, PT_ALLMAP))
            return false;

        P_SetMessage(plr, GOTMAP);
        if(!mapSetup)
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
        break;

    case IT_VISOR:
        if(!P_GivePower(plr, PT_INFRARED))
            return false;

        P_SetMessage(plr, GOTVISOR);
        if(!mapSetup)
            S_ConsoleSound(SFX_GETPOW, NULL, plr - players);
        break;

    case IT_BACKPACK:
        P_GiveBackpack(plr);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_CLIP:
        if(!P_GiveAmmo(plr, AT_CLIP, dropped? 0 /*half a clip*/ : 1))
            return false;

        P_SetMessage(plr, GOTCLIP);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_CLIP_BOX:
        if(!P_GiveAmmo(plr, AT_CLIP, 5))
            return false;

        P_SetMessage(plr, GOTCLIPBOX);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_ROCKET:
        if(!P_GiveAmmo(plr, AT_MISSILE, 1))
            return false;

        P_SetMessage(plr, GOTROCKET);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_ROCKET_BOX:
        if(!P_GiveAmmo(plr, AT_MISSILE, 5))
            return false;

        P_SetMessage(plr, GOTROCKBOX);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_CELL:
        if(!P_GiveAmmo(plr, AT_CELL, 1))
            return false;

        P_SetMessage(plr, GOTCELL);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_CELL_BOX:
        if(!P_GiveAmmo(plr, AT_CELL, 5))
            return false;

        P_SetMessage(plr, GOTCELLBOX);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_SHELL:
        if(!P_GiveAmmo(plr, AT_SHELL, 1))
            return false;

        P_SetMessage(plr, GOTSHELLS);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_SHELL_BOX:
        if(!P_GiveAmmo(plr, AT_SHELL, 5))
            return false;

        P_SetMessage(plr, GOTSHELLBOX);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_WEAPON_BFG:
        return pickupWeapon(plr, WT_SEVENTH, dropped, GOTBFG9000);

    case IT_WEAPON_CHAINGUN:
        return pickupWeapon(plr, WT_FOURTH, dropped, GOTCHAINGUN);

    case IT_WEAPON_CHAINSAW:
        return pickupWeapon(plr, WT_EIGHTH, dropped, GOTCHAINSAW);

    case IT_WEAPON_RLAUNCHER:
        return pickupWeapon(plr, WT_FIFTH, dropped, GOTLAUNCHER);

    case IT_WEAPON_PLASMARIFLE:
        return pickupWeapon(plr, WT_SIXTH, dropped, GOTPLASMA);

    case IT_WEAPON_SHOTGUN:
        return pickupWeapon(plr, WT_THIRD, dropped, GOTSHOTGUN);

    case IT_WEAPON_SSHOTGUN:
        return pickupWeapon(plr, WT_NINETH, dropped, GOTSHOTGUN2);

    default:
        Con_Error("pickupItem: Unknown item %i.", (int) item);
    }

    return true;
}

void P_TouchSpecialMobj(mobj_t* special, mobj_t* toucher)
{
    player_t* player;
    coord_t delta;
    itemtype_t item;

    delta = special->origin[VZ] - toucher->origin[VZ];
    if(delta > toucher->height || delta < -8)
    {   // Out of reach.
        return;
    }

    // Dead thing touching (can happen with a sliding player corpse).
    if(toucher->health <= 0)
        return;

    player = toucher->player;

    // Identify by sprite.
    if((item = getItemTypeBySprite(special->sprite)) != IT_NONE)
    {
        if(!pickupItem(player, item, (special->flags & MF_DROPPED)? true : false))
            return; // Don't destroy the item.
    }
    else
    {
        App_Log(DE2_MAP_WARNING, "P_TouchSpecialMobj: Unknown gettable thing %i",
                (int) special->type);
    }

    if(special->flags & MF_COUNTITEM)
    {
        player->itemCount++;
        player->update |= PSF_COUNTERS;
    }

    P_MobjRemove(special, false);

    if(!mapSetup)
        player->bonusCount += BONUSADD;
}

void P_KillMobj(mobj_t *source, mobj_t *target, dd_bool stomping)
{
    mobjtype_t          item;
    mobj_t*             mo;
    unsigned int        an;
    angle_t             angle;

    if(!target)
        return; // Nothing to kill...

    target->flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY);

    if(target->type != MT_SKULL)
        target->flags &= ~MF_NOGRAVITY;

    target->flags |= MF_CORPSE | MF_DROPOFF;
    target->flags2 &= ~MF2_PASSMOBJ;
    target->corpseTics = 0;
    target->height /= 2*2;

    Mobj_RunScriptOnDeath(target, source);

    if(source && source->player)
    {
        // Count for intermission.
        if(target->flags & MF_COUNTKILL)
        {
            source->player->killCount++;
            source->player->update |= PSF_COUNTERS;
        }

        if(target->player)
        {
            source->player->frags[target->player - players]++;
            NetSv_FragsForAll(source->player);
            NetSv_KillMessage(source->player, target->player, stomping);
        }
    }
    else if(!IS_NETGAME && (target->flags & MF_COUNTKILL))
    {
        // Count all monster deaths (even those caused by other monsters).
        players[0].killCount++;
    }

    if(target->player)
    {
        // Count environment kills against the player.
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
        target->player->rebornWait = PLAYER_REBORN_TICS;
        target->player->update |= PSF_STATE;
        target->player->plr->flags |= DDPF_DEAD;
        P_DropWeapon(target->player);

        // Don't die with the automap open.
        ST_CloseAll(target->player - players, false);
    }

    if(target->health < -target->info->spawnHealth &&
       P_GetState(target->type, SN_XDEATH))
    {   // Extreme death.
        P_MobjChangeState(target, P_GetState(target->type, SN_XDEATH));
    }
    else
    {   // Normal death.
        P_MobjChangeState(target, P_GetState(target->type, SN_DEATH));
    }

    target->tics -= P_Random() & 3;

    if(target->tics < 1)
        target->tics = 1;

    // Enemies in Chex Quest don't drop stuff.
    if(gameMode == doom_chex)
        return;

    // Drop stuff.
    // This determines the kind of object spawned during the death frame
    // of a thing.
    switch(target->type)
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
    angle = P_Random() << 24;
    an = angle >> ANGLETOFINESHIFT;
    if((mo = P_SpawnMobjXYZ(item, target->origin[VX] + 3 * FIX2FLT(finecosine[an]),
                            target->origin[VY] + 3 * FIX2FLT(finesine[an]),
                            0, angle, MSF_Z_FLOOR)))
        mo->flags |= MF_DROPPED; // Special versions of items.
}

int P_DamageMobj(mobj_t* target, mobj_t* inflictor, mobj_t* source,
                 int damageP, dd_bool stomping)
{
    return P_DamageMobj2(target, inflictor, source, damageP, stomping, false);
}

/**
 * Damages both enemies and players.
 *
 * @param inflictor     Mobj that caused the damage creature or missile,
 *                      can be NULL (slime, etc)
 * @param source        Mobj to target after taking damage. Can be @c NULL
 *                      for barrel explosions and other environmental stuff.
 *                      Source and inflictor are the same for melee attacks.
 * @param skipNetworkCheck  Allow the damage to be done regardless of netgame status.
 *
 * @return              Actual amount of damage done.
 */
int P_DamageMobj2(mobj_t *target, mobj_t *inflictor, mobj_t *source,
                  int damageP, dd_bool stomping, dd_bool skipNetworkCheck)
{
    angle_t angle;
    int saved, originalHealth;
    player_t *player;
    int /*temp,*/ damage;

    if(!target)
        return 0; // Wha?

    originalHealth = target->health;

    // The actual damage (== damageP * netMobDamageModifier for any
    // non-player mobj).
    damage = damageP;

    if(!skipNetworkCheck)
    {
        if(IS_NETGAME && !stomping && D_NetDamageMobj(target, inflictor, source, damage))
        {   // We're done here.
            return 0;
        }
        // Clients can't harm anybody.
        if(IS_CLIENT)
            return 0;
    }

    if(!(target->flags & MF_SHOOTABLE))
        return 0; // Shouldn't happen...

    if(target->health <= 0)
        return 0;

    if(target->player)
    {   // Player specific.
        // Check if player-player damage is disabled.
        if(source && source->player && source->player != target->player)
        {
            // Co-op damage disabled?
            if(IS_NETGAME && !gfw_Rule(deathmatch) && cfg.noCoopDamage)
                return 0;

            // Same color, no damage?
            if(cfg.noTeamDamage &&
               cfg.playerColor[target->player - players] ==
               cfg.playerColor[source->player - players])
                return 0;
        }
    }

    if(target->flags & MF_SKULLFLY)
    {
        target->mom[MX] = target->mom[MY] = target->mom[MZ] = 0;
    }

    player = target->player;
    if(player && gfw_Rule(skill) == SM_BABY)
        damage /= 2; // Take half damage in trainer mode.

    // Use the cvar damage multiplier netMobDamageModifier only if the
    // inflictor is not a player.
    if(inflictor && !inflictor->player &&
       (!source || (source && !source->player)))
    {
        // damage = (int) ((float) damage * netMobDamageModifier);
        if(IS_NETGAME)
            damage *= cfg.common.netMobDamageModifier;
    }

    // Some close combat weapons should not inflict thrust and push the
    // victim out of reach, thus kick away unless using a melee weapon.
    if(inflictor && !(target->flags & MF_NOCLIP) &&
       (!source || !source->player ||
        source->player->readyWeapon != WT_EIGHTH) &&
       !(inflictor->flags2 & MF2_NODMGTHRUST))
    {
        uint an;
        coord_t thrust;

        angle = M_PointToAngle2(inflictor->origin, target->origin);
        thrust = FIX2FLT(damage * (FRACUNIT>>3) * 100 / target->info->mass);

        // Make fall forwards sometimes.
        if(damage < 40 && damage > target->health &&
           target->origin[VZ] - inflictor->origin[VZ] > 64 && (P_Random() & 1))
        {
            angle += ANG180;
            thrust *= 4;
        }

        an = angle >> ANGLETOFINESHIFT;
        target->mom[MX] += thrust * FIX2FLT(finecosine[an]);
        target->mom[MY] += thrust * FIX2FLT(finesine[an]);
        NetSv_PlayerMobjImpulse(target, thrust * FIX2FLT(finecosine[an]), thrust * FIX2FLT(finesine[an]), 0);

        // $dropoff_fix: thrust objects hanging off ledges.
        if((target->intFlags & MIF_FALLING) && target->gear >= MAXGEAR)
            target->gear = 0;
    }

    if(player)
    {
        // End of game hell hack.
        if(P_ToXSector(Mobj_Sector(target))->special == 11 &&
           damage >= target->health)
        {
            damage = target->health - 1;
        }

        // Below certain threshold, ignore damage in GOD mode, or with
        // INVUL power.
        if(damage < 1000 &&
           ((P_GetPlayerCheats(player) & CF_GODMODE) ||
            player->powers[PT_INVULNERABILITY]))
        {
            return 0;
        }

        if(player->armorType)
        {
            if(player->armorType == 1)
                saved = damage / 3;
            else
                saved = damage / 2;

            if(player->armorPoints <= saved)
            {   // Armor is used up.
                saved = player->armorPoints;
                player->armorType = 0;
            }

            player->armorPoints -= saved;
            player->update |= PSF_ARMOR_POINTS;
            damage -= saved;
        }

        player->health -= damage;
        if(player->health < 0)
            player->health = 0;

        player->update |= PSF_HEALTH;
        player->attacker = source;

        player->damageCount += damage; // Add damage after armor / invuln.
        if(player->damageCount > 100)
            player->damageCount = 100; // Teleport stomp does 10k points...

        // temp = damage < 100 ? damage : 100; Unused?

        // Maybe unhide the HUD?
        ST_HUDUnHide(player - players, HUE_ON_DAMAGE);
    }

    Mobj_InflictDamage(target, inflictor, damage);

    if(target->health > 0)
    {   // Still alive, phew!
        if((P_Random() < target->info->painChance) &&
           !(target->flags & MF_SKULLFLY))
        {
            statenum_t          state;

            target->flags |= MF_JUSTHIT; // Fight back!

            if((state = P_GetState(target->type, SN_PAIN)) != S_NULL)
                P_MobjChangeState(target, state);
        }

        target->reactionTime = 0; // We're awake now...

        if(source &&
           ((!target->threshold && !(source->flags3 & MF3_NOINFIGHT)) || target->type == MT_VILE) &&
           source != target && source->type != MT_VILE)
        {
            statenum_t          state;

            // Target mobj is not intent on another mobj, so make it chase
            // after the source of the damage.
            target->target = source;
            target->threshold = BASETHRESHOLD;

            if((state = P_GetState(target->type, SN_SEE)) != S_NULL &&
               target->state == &STATES[P_GetState(target->type, SN_SPAWN)])
            {
                P_MobjChangeState(target, state);
            }
        }
    }
    else
    {   // Death.
        P_KillMobj(source, target, stomping);
    }

    return originalHealth - target->health;

#undef BASETHRESHOLD
}
