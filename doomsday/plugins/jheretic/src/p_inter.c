/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
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

// HEADER FILES ------------------------------------------------------------

#include <string.h>

#include "jheretic.h"

#include "am_map.h"
#include "d_net.h"
#include "dmu_lib.h"
#include "p_player.h"
#include "p_inventory.h"
#include "hu_inventory.h"
#include "p_tick.h"
#include "p_user.h"
#include "p_mapsetup.h"

// MACROS ------------------------------------------------------------------

#define BONUSADD            (6)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int maxAmmo[NUM_AMMO_TYPES] = {
    100, // gold wand
    50, // crossbow
    200, // blaster
    200, // skull rod
    20, // phoenix rod
    150 // mace
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int getWeaponAmmo[NUM_WEAPON_TYPES] = {
    0, // staff
    25, // gold wand
    10, // crossbow
    30, // blaster
    50, // skull rod
    2, // phoenix rod
    50, // mace
    0 // gauntlets
};

// CODE --------------------------------------------------------------------

/**
 * Returns true if the player accepted the ammo, false if it was
 * refused (player has maxammo[ammo]).
 */
boolean P_GiveAmmo(player_t *player, ammotype_t ammo, int num)
{
    if(ammo == AT_NOAMMO)
        return false;

    if(ammo < 0 || ammo > NUM_AMMO_TYPES)
        Con_Error("P_GiveAmmo: bad type %i", ammo);

    if(!(player->ammo[ammo].owned < player->ammo[ammo].max))
        return false;

    if(gameSkill == SM_BABY || gameSkill == SM_NIGHTMARE)
    {   // Extra ammo in baby mode and nightmare mode.
        num += num / 1;
    }

    // We are about to receive some more ammo. Does the player want to
    // change weapon automatically?
    P_MaybeChangeWeapon(player, WT_NOCHANGE, ammo, false);

    if(player->ammo[ammo].owned + num > player->ammo[ammo].max)
        player->ammo[ammo].owned = player->ammo[ammo].max;
    else
        player->ammo[ammo].owned += num;
    player->update |= PSF_AMMO;

    // Maybe unhide the HUD?
    ST_HUDUnHide(player - players, HUE_ON_PICKUP_AMMO);

    return true;
}

/**
 * @return              @c true, if the weapon or its ammo was accepted.
 */
boolean P_GiveWeapon(player_t *player, weapontype_t weapon, const char* pickupMessage, int pickupSound)
{
    int                 i;
    int                 lvl = (player->powers[PT_WEAPONLEVEL2]? 1 : 0);
    boolean             gaveAmmo = false;
    boolean             gaveWeapon = false;

    if(IS_NETGAME && !deathmatch)
    {
        // Leave placed weapons forever on net games.
        if(player->weapons[weapon].owned)
            return false;

        player->bonusCount += BONUSADD;
        player->weapons[weapon].owned = true;
        player->update |= PSF_OWNED_WEAPONS;

        // Give some of each of the ammo types used by this weapon.
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
        {
            if(!weaponInfo[weapon][player->class_].mode[lvl].ammoType[i])
                continue;   // Weapon does not take this type of ammo.

            if(P_GiveAmmo(player, i, getWeaponAmmo[weapon]))
                gaveAmmo = true; // At least ONE type of ammo was given.
        }

        // Should we change weapon automatically?
        P_MaybeChangeWeapon(player, weapon, AT_NOAMMO, false);

        // Maybe unhide the HUD?
        ST_HUDUnHide(player - players, HUE_ON_PICKUP_WEAPON);

        // Notify the player.
        S_ConsoleSound(pickupSound, NULL, player - players);
        if(pickupMessage)
        {
            P_SetMessage(player, 0, pickupMessage);
        }
        return false;
    }
    else
    {
        // Give some of each of the ammo types used by this weapon.
        for(i = 0; i < NUM_AMMO_TYPES; ++i)
        {
            if(!weaponInfo[weapon][player->class_].mode[lvl].ammoType[i])
                continue;   // Weapon does not take this type of ammo.

            if(P_GiveAmmo(player, i, getWeaponAmmo[weapon]))
                gaveAmmo = true; // At least ONE type of ammo was given.
        }

        if(player->weapons[weapon].owned)
            gaveWeapon = false;
        else
        {
            gaveWeapon = true;
            player->weapons[weapon].owned = true;
            player->update |= PSF_OWNED_WEAPONS;

            // Should we change weapon automatically?
            P_MaybeChangeWeapon(player, weapon, AT_NOAMMO, false);
        }

        // Maybe unhide the HUD?
        if(gaveWeapon)
        {
            ST_HUDUnHide(player - players, HUE_ON_PICKUP_WEAPON);
        }

        if(gaveWeapon || gaveAmmo)
        {
            // Notify the player.
            S_ConsoleSound(pickupSound, NULL, player - players);
            if(pickupMessage)
            {
                P_SetMessage(player, 0, pickupMessage);
            }
        }
        return (gaveWeapon || gaveAmmo);
    }
}

/**
 * @return              @c false, if the body isn't needed at all.
 */
boolean P_GiveBody(player_t *player, int num)
{
    int                 max;

    if(player->morphTics)
        max = MAXCHICKENHEALTH;
    else
        max = maxHealth;

    if(player->health >= max)
    {
        return false;
    }

    player->health += num;
    if(player->health > max)
    {
        player->health = max;
    }

    player->update |= PSF_HEALTH;
    player->plr->mo->health = player->health;

    // Maybe unhide the HUD?
    ST_HUDUnHide(player - players, HUE_ON_PICKUP_HEALTH);

    return true;
}

/**
 * @return              @c true, iff the armor was given.
 */
boolean P_GiveArmor(player_t* plr, int type, int points)
{
    if(plr->armorPoints >= points)
        return false;

    P_PlayerSetArmorType(plr, type);
    P_PlayerGiveArmorBonus(plr, points - plr->armorPoints);

    // Maybe unhide the HUD?
    ST_HUDUnHide(plr - players, HUE_ON_PICKUP_ARMOR);

    return true;
}

void P_GiveKey(player_t* player, keytype_t key)
{
    if(player->keys[key])
        return;

    player->bonusCount = BONUSADD;
    player->keys[key] = true;
    player->update |= PSF_KEYS;

    // Maybe unhide the HUD?
    ST_HUDUnHide(player - players, HUE_ON_PICKUP_KEY);
}

/**
 * @return              @c true, if power accepted.
 */
boolean P_GivePower(player_t* player, powertype_t power)
{
    mobj_t*             plrmo = player->plr->mo;
    boolean             retval = false;

    player->update |= PSF_POWERS;
    switch(power)
    {
    case PT_INVULNERABILITY:
        if(!(player->powers[power] > BLINKTHRESHOLD))
        {
            player->powers[power] = INVULNTICS;
            retval = true;
        }
        break;

    case PT_WEAPONLEVEL2:
        if(!(player->powers[power] > BLINKTHRESHOLD))
        {
            player->powers[power] = WPNLEV2TICS;
            retval = true;
        }
        break;

    case PT_INVISIBILITY:
        if(!(player->powers[power] > BLINKTHRESHOLD))
        {
            player->powers[power] = INVISTICS;
            plrmo->flags |= MF_SHADOW;
            retval = true;
        }
        break;

    case PT_FLIGHT:
        if(!(player->powers[power] > BLINKTHRESHOLD))
        {
            player->powers[power] = FLIGHTTICS;
            plrmo->flags2 |= MF2_FLY;
            plrmo->flags |= MF_NOGRAVITY;
            if(plrmo->origin[VZ] <= plrmo->floorZ)
            {
                player->flyHeight = 10; // Thrust the player in the air a bit.
                player->plr->flags |= DDPF_FIXMOM;
            }
            retval = true;
        }
        break;

    case PT_INFRARED:
        if(!(player->powers[power] > BLINKTHRESHOLD))
        {
            player->powers[power] = INFRATICS;
            retval = true;
        }
        break;

    default:
        if(!player->powers[power])
        {
            player->powers[power] = 1;
            retval = true;
        }
        break;
    }

    if(retval)
    {
        if(power == PT_ALLMAP)
            ST_RevealAutomap(player - players, true);
    }

    return retval;
}

/**
 * Removes the MF_SPECIAL flag, and initiates the item pickup animation.
 */
void P_SetDormantItem(mobj_t* mo)
{
    mo->flags &= ~MF_SPECIAL;
    if(deathmatch && (mo->type != MT_ARTIINVULNERABILITY) &&
       (mo->type != MT_ARTIINVISIBILITY))
    {
        P_MobjChangeState(mo, S_DORMANTARTI1);
    }
    else
    {   // Don't respawn.
        P_MobjChangeState(mo, S_DEADARTI1);
    }

    S_StartSound(SFX_ARTIUP, mo);
}

void C_DECL A_RestoreArtifact(mobj_t* mo)
{
    mo->flags |= MF_SPECIAL;
    P_MobjChangeState(mo, P_GetState(mo->type, SN_SPAWN));
    S_StartSound(SFX_RESPAWN, mo);
}

void P_HideSpecialThing(mobj_t* thing)
{
    thing->flags &= ~MF_SPECIAL;
    thing->flags2 |= MF2_DONTDRAW;
    P_MobjChangeState(thing, S_HIDESPECIAL1);
}

/**
 * Make a special thing visible again.
 */
void C_DECL A_RestoreSpecialThing1(mobj_t* thing)
{
    if(thing->type == MT_WMACE)
    {   // Do random mace placement.
        P_RepositionMace(thing);
    }

    thing->flags2 &= ~MF2_DONTDRAW;
    S_StartSound(SFX_RESPAWN, thing);
}

void C_DECL A_RestoreSpecialThing2(mobj_t* thing)
{
    thing->flags |= MF_SPECIAL;
    P_MobjChangeState(thing, P_GetState(thing->type, SN_SPAWN));
}

typedef enum {
    IT_NONE = 0,
    IT_HEALTH_POTION,
    IT_SHIELD1,
    IT_SHIELD2,
    IT_BAGOFHOLDING,
    IT_ALLMAP,
    IT_KEY_BLUE,
    IT_KEY_YELLOW,
    IT_KEY_GREEN,
    IT_ITEM_HEALTHPOTION,
    IT_ITEM_WINGS,
    IT_ITEM_INVUL,
    IT_ITEM_TOMB,
    IT_ITEM_INVIS,
    IT_ITEM_EGG,
    IT_ITEM_HEALTHSUPER,
    IT_ITEM_TORCH,
    IT_ITEM_FIREBOMB,
    IT_ITEM_TELEPORT,
    IT_AMMO_WAND,
    IT_AMMO_WAND_LARGE,
    IT_AMMO_MACE,
    IT_AMMO_MACE_LARGE,
    IT_AMMO_CROSSBOW,
    IT_AMMO_CROSSBOW_LARGE,
    IT_AMMO_BLASTER,
    IT_AMMO_BLASTER_LARGE,
    IT_AMMO_SKULL,
    IT_AMMO_SKULL_LARGE,
    IT_AMMO_PHOENIX,
    IT_AMMO_PHOENIX_LARGE,
    IT_WEAPON_MACE,
    IT_WEAPON_CROSSBOW,
    IT_WEAPON_BLASTER,
    IT_WEAPON_SKULLROD,
    IT_WEAPON_PHOENIXROD,
    IT_WEAPON_GAUNTLETS
} itemtype_t;

static itemtype_t getItemTypeBySprite(spritetype_e sprite)
{
    static const struct item_s {
        itemtype_t      type;
        spritetype_e    sprite;
    } items[] = {
        { IT_HEALTH_POTION, SPR_PTN1 },
        { IT_SHIELD1, SPR_SHLD },
        { IT_SHIELD2, SPR_SHD2 },
        { IT_BAGOFHOLDING, SPR_BAGH },
        { IT_ALLMAP, SPR_SPMP },
        { IT_KEY_BLUE, SPR_BKYY },
        { IT_KEY_YELLOW, SPR_CKYY },
        { IT_KEY_GREEN, SPR_AKYY },
        { IT_ITEM_HEALTHPOTION, SPR_PTN2 },
        { IT_ITEM_WINGS, SPR_SOAR },
        { IT_ITEM_INVUL, SPR_INVU },
        { IT_ITEM_TOMB, SPR_PWBK },
        { IT_ITEM_INVIS, SPR_INVS },
        { IT_ITEM_EGG, SPR_EGGC },
        { IT_ITEM_HEALTHSUPER, SPR_SPHL },
        { IT_ITEM_TORCH, SPR_TRCH },
        { IT_ITEM_FIREBOMB, SPR_FBMB },
        { IT_ITEM_TELEPORT, SPR_ATLP },
        { IT_AMMO_WAND, SPR_AMG1 },
        { IT_AMMO_WAND_LARGE, SPR_AMG2 },
        { IT_AMMO_MACE, SPR_AMM1 },
        { IT_AMMO_MACE_LARGE, SPR_AMM2 },
        { IT_AMMO_CROSSBOW, SPR_AMC1 },
        { IT_AMMO_CROSSBOW_LARGE, SPR_AMC2 },
        { IT_AMMO_BLASTER, SPR_AMB1 },
        { IT_AMMO_BLASTER_LARGE, SPR_AMB2 },
        { IT_AMMO_SKULL, SPR_AMS1 },
        { IT_AMMO_SKULL_LARGE, SPR_AMS2 },
        { IT_AMMO_PHOENIX, SPR_AMP1 },
        { IT_AMMO_PHOENIX_LARGE, SPR_AMP2 },
        { IT_WEAPON_MACE, SPR_WMCE },
        { IT_WEAPON_CROSSBOW, SPR_WBOW },
        { IT_WEAPON_BLASTER, SPR_WBLS },
        { IT_WEAPON_SKULLROD, SPR_WSKL },
        { IT_WEAPON_PHOENIXROD, SPR_WPHX },
        { IT_WEAPON_GAUNTLETS, SPR_WGNT },
        { IT_NONE, 0 },
    };
    uint                i;

    for(i = 0; items[i].type != IT_NONE; ++i)
        if(items[i].sprite == sprite)
            return items[i].type;

    return IT_NONE;
}

static boolean giveItem(player_t* plr, itemtype_t item, int quantity)
{
    if(!plr)
        return false;

    switch(item)
    {
    case IT_HEALTH_POTION:
        if(!P_GiveBody(plr, 10))
            return false;

        P_SetMessage(plr, 0, TXT_ITEMHEALTH);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_SHIELD1:
        if(!P_GiveArmor(plr, 1, 1 * 100))
            return false;

        P_SetMessage(plr, 0, TXT_ITEMSHIELD1);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_SHIELD2:
        if(!P_GiveArmor(plr, 2, 2 * 100))
            return false;

        P_SetMessage(plr, 0, TXT_ITEMSHIELD2);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_BAGOFHOLDING:
        if(!plr->backpack)
        {
            int i;
            for(i = 0; i < NUM_AMMO_TYPES; ++i)
            {
                plr->ammo[i].max *= 2;
            }
            plr->backpack = true;
        }

        P_GiveAmmo(plr, AT_CRYSTAL, AMMO_GWND_WIMPY);
        P_GiveAmmo(plr, AT_ORB, AMMO_BLSR_WIMPY);
        P_GiveAmmo(plr, AT_ARROW, AMMO_CBOW_WIMPY);
        P_GiveAmmo(plr, AT_RUNE, AMMO_SKRD_WIMPY);
        P_GiveAmmo(plr, AT_FIREORB, AMMO_PHRD_WIMPY);
        P_SetMessage(plr, 0, TXT_ITEMBAGOFHOLDING);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_ALLMAP:
        if(!P_GivePower(plr, PT_ALLMAP))
            return false;

        // Maybe unhide the HUD?
        ST_HUDUnHide(plr - players, HUE_ON_PICKUP_POWER);

        P_SetMessage(plr, 0, TXT_ITEMSUPERMAP);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_KEY_BLUE:
        if(!plr->keys[KT_BLUE])
        {
            P_SetMessage(plr, 0, TXT_GOTBLUEKEY);
            P_GiveKey(plr, KT_BLUE);
            if(!mapSetup)
                S_ConsoleSound(SFX_KEYUP, NULL, plr - players);
        }
        if(IS_NETGAME)
            return false;
        break;

    case IT_KEY_YELLOW:
        if(!plr->keys[KT_YELLOW])
        {
            P_SetMessage(plr, 0, TXT_GOTYELLOWKEY);
            P_GiveKey(plr, KT_YELLOW);
            if(!mapSetup)
                S_ConsoleSound(SFX_KEYUP, NULL, plr - players);
        }
        if(IS_NETGAME)
            return false;
        break;

    case IT_KEY_GREEN:
        if(!plr->keys[KT_GREEN])
        {
            P_SetMessage(plr, 0, TXT_GOTGREENKEY);
            P_GiveKey(plr, KT_GREEN);
            if(!mapSetup)
                S_ConsoleSound(SFX_KEYUP, NULL, plr - players);
        }
        if(IS_NETGAME)
            return false;
        break;

    case IT_ITEM_HEALTHPOTION:
        if(!P_InventoryGive(plr - players, IIT_HEALTH, false))
            return false;

        P_SetMessage(plr, 0, TXT_INV_HEALTH);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_ITEM_WINGS:
        if(!P_InventoryGive(plr - players, IIT_FLY, false))
            return false;

        P_SetMessage(plr, 0, TXT_INV_FLY);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_ITEM_INVUL:
        if(!P_InventoryGive(plr - players, IIT_INVULNERABILITY, false))
            return false;

        P_SetMessage(plr, 0, TXT_INV_INVULNERABILITY);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_ITEM_TOMB:
        if(!P_InventoryGive(plr - players, IIT_TOMBOFPOWER, false))
            return false;

        P_SetMessage(plr, 0, TXT_INV_TOMEOFPOWER);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_ITEM_INVIS:
        if(!P_InventoryGive(plr - players, IIT_INVISIBILITY, false))
            return false;

        P_SetMessage(plr, 0, TXT_INV_INVISIBILITY);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_ITEM_EGG:
        if(!P_InventoryGive(plr - players, IIT_EGG, false))
            return false;

        P_SetMessage(plr, 0, TXT_INV_EGG);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_ITEM_HEALTHSUPER:
        if(!P_InventoryGive(plr - players, IIT_SUPERHEALTH, false))
            return false;

        P_SetMessage(plr, 0, TXT_INV_SUPERHEALTH);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_ITEM_TORCH:
        if(!P_InventoryGive(plr - players, IIT_TORCH, false))
            return false;

        P_SetMessage(plr, 0, TXT_INV_TORCH);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_ITEM_FIREBOMB:
        if(!P_InventoryGive(plr - players, IIT_FIREBOMB, false))
            return false;

        P_SetMessage(plr, 0, TXT_INV_FIREBOMB);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_ITEM_TELEPORT:
        if(!P_InventoryGive(plr - players, IIT_TELEPORT, false))
            return false;

        P_SetMessage(plr, 0, TXT_INV_TELEPORT);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_WAND:
        if(!P_GiveAmmo(plr, AT_CRYSTAL, quantity))
            return false;

        P_SetMessage(plr, 0, TXT_AMMOGOLDWAND1);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_WAND_LARGE:
        if(!P_GiveAmmo(plr, AT_CRYSTAL, quantity))
            return false;

        P_SetMessage(plr, 0, TXT_AMMOGOLDWAND2);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_MACE:
        if(!P_GiveAmmo(plr, AT_MSPHERE, quantity))
            return false;

        P_SetMessage(plr, 0, TXT_AMMOMACE1);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_MACE_LARGE:
        if(!P_GiveAmmo(plr, AT_MSPHERE, quantity))
            return false;

        P_SetMessage(plr, 0, TXT_AMMOMACE2);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_CROSSBOW:
        if(!P_GiveAmmo(plr, AT_ARROW, quantity))
            return false;

        P_SetMessage(plr, 0, TXT_AMMOCROSSBOW1);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_CROSSBOW_LARGE:
        if(!P_GiveAmmo(plr, AT_ARROW, quantity))
            return false;

        P_SetMessage(plr, 0, TXT_AMMOCROSSBOW2);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_BLASTER:
        if(!P_GiveAmmo(plr, AT_ORB, quantity))
            return false;

        P_SetMessage(plr, 0, TXT_AMMOBLASTER1);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_BLASTER_LARGE:
        if(!P_GiveAmmo(plr, AT_ORB, quantity))
            return false;

        P_SetMessage(plr, 0, TXT_AMMOBLASTER2);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_SKULL:
        if(!P_GiveAmmo(plr, AT_RUNE, quantity))
            return false;

        P_SetMessage(plr, 0, TXT_AMMOSKULLROD1);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_SKULL_LARGE:
        if(!P_GiveAmmo(plr, AT_RUNE, quantity))
            return false;

        P_SetMessage(plr, 0, TXT_AMMOSKULLROD2);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_PHOENIX:
        if(!P_GiveAmmo(plr, AT_FIREORB, quantity))
            return false;

        P_SetMessage(plr, 0, TXT_AMMOPHOENIXROD1);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_AMMO_PHOENIX_LARGE:
        if(!P_GiveAmmo(plr, AT_FIREORB, quantity))
            return false;

        P_SetMessage(plr, 0, TXT_AMMOPHOENIXROD2);
        if(!mapSetup)
            S_ConsoleSound(SFX_ITEMUP, NULL, plr - players);
        break;

    case IT_WEAPON_MACE:
        if(!P_GiveWeapon(plr, WT_SEVENTH, TXT_WPNMACE, !mapSetup? SFX_WPNUP : 0))
            return false;
        break;

    case IT_WEAPON_CROSSBOW:
        if(!P_GiveWeapon(plr, WT_THIRD, TXT_WPNCROSSBOW, !mapSetup? SFX_WPNUP : 0))
            return false;
        break;

    case IT_WEAPON_BLASTER:
        if(!P_GiveWeapon(plr, WT_FOURTH, TXT_WPNBLASTER, !mapSetup? SFX_WPNUP : 0))
            return false;
        break;

    case IT_WEAPON_SKULLROD:
        if(!P_GiveWeapon(plr, WT_FIFTH, TXT_WPNSKULLROD, !mapSetup? SFX_WPNUP : 0))
            return false;
        break;

    case IT_WEAPON_PHOENIXROD:
        if(!P_GiveWeapon(plr, WT_SIXTH, TXT_WPNPHOENIXROD, !mapSetup? SFX_WPNUP : 0))
            return false;
        break;

    case IT_WEAPON_GAUNTLETS:
        if(!P_GiveWeapon(plr, WT_EIGHTH, TXT_WPNGAUNTLETS, !mapSetup? SFX_WPNUP : 0))
            return false;
        break;

    default:
        Con_Error("giveItem: Unknown item %i.", (int) item);
    }

    return true;
}

void P_TouchSpecialMobj(mobj_t* special, mobj_t* toucher)
{
    player_t* player;
    coord_t delta;
    itemtype_t item;

    delta = special->origin[VZ] - toucher->origin[VZ];
    if(delta > toucher->height || delta < -32)
    {
        // Out of reach.
        return;
    }

    // Dead thing touching (can happen with a sliding player corpse).
    if(toucher->health <= 0) return;

    player = toucher->player;

    // Identify by sprite.
    if((item = getItemTypeBySprite(special->sprite)) != IT_NONE)
    {
        if(!giveItem(player, item, special->health))
            return; // Don't destroy the item.
    }
    else
    {
        Con_Message("P_TouchSpecialMobj: Unknown gettable thing %i.\n",
                    (int) special->type);
    }

    if(special->flags & MF_COUNTITEM)
        player->itemCount++;

    switch(item)
    {
    case IT_ITEM_HEALTHPOTION:
    case IT_ITEM_WINGS:
    case IT_ITEM_INVUL:
    case IT_ITEM_TOMB:
    case IT_ITEM_INVIS:
    case IT_ITEM_EGG:
    case IT_ITEM_HEALTHSUPER:
    case IT_ITEM_TORCH:
    case IT_ITEM_FIREBOMB:
    case IT_ITEM_TELEPORT:
        P_SetDormantItem(special);
        break;

    default:
        if(deathmatch && !(special->flags & MF_DROPPED))
            P_HideSpecialThing(special);
        else
            P_MobjRemove(special, false);

        if(!mapSetup)
            player->bonusCount += BONUSADD;
        break;
    }
}

void P_KillMobj(mobj_t* source, mobj_t* target)
{
    statenum_t          state;

    if(!target) // Nothing to kill.
        return;

    target->flags &= ~(MF_SHOOTABLE | MF_FLOAT | MF_SKULLFLY | MF_NOGRAVITY);
    target->flags |= MF_CORPSE | MF_DROPOFF;
    target->flags2 &= ~MF2_PASSMOBJ;
    target->corpseTics = 0;
    target->height /= 2*2;
    if(source && source->player)
    {
        if(target->flags & MF_COUNTKILL)
        {   // Count for intermission.
            source->player->killCount++;
        }

        if(target->player)
        {   // Frag stuff.
            source->player->update |= PSF_FRAGS;
            if(target == source)
            {   // Self-frag.
                target->player->frags[target->player - players]--;
                NetSv_FragsForAll(target->player);
            }
            else
            {
                source->player->frags[target->player - players]++;
                NetSv_FragsForAll(source->player);

                if(source->player->morphTics)
                {   // Make a super chicken.
                    P_GivePower(source->player, PT_WEAPONLEVEL2);
                }
            }
        }
    }
    else if(!IS_NETGAME && (target->flags & MF_COUNTKILL))
    {   // Count all monster deaths.
        players[0].killCount++;
    }

    if(target->player)
    {
        if(!source)
        {   // Self-frag.
            target->player->frags[target->player - players]--;
            NetSv_FragsForAll(target->player);
        }

        target->flags &= ~MF_SOLID;
        target->flags2 &= ~MF2_FLY;
        target->player->powers[PT_FLIGHT] = 0;
        target->player->powers[PT_WEAPONLEVEL2] = 0;
        target->player->playerState = PST_DEAD;
        target->player->rebornWait = PLAYER_REBORN_TICS;
        target->player->plr->flags |= DDPF_DEAD;
        target->player->update |= PSF_STATE;
        P_DropWeapon(target->player);

        if(target->flags2 & MF2_FIREDAMAGE)
        {   // Player flame death.
            P_MobjChangeState(target, S_PLAY_FDTH1);
            return;
        }

        // Don't die with the automap open.
        ST_AutomapOpen(target->player - players, false, false);
#if __JHERETIC__ || __JHEXEN__
        Hu_InventoryOpen(target->player - players, false);
#endif
    }

    if((state = P_GetState(target->type, SN_XDEATH)) != S_NULL &&
       target->health < -(target->info->spawnHealth / 2))
    {   // Extreme death.
        P_MobjChangeState(target, state);
    }
    else
    {   // Normal death.
        P_MobjChangeState(target, P_GetState(target->type, SN_DEATH));
    }

    target->tics -= P_Random() & 3;
}

/**
 * @return              @c true, if the player is morphed.
 */
boolean P_MorphPlayer(player_t* player)
{
    mobj_t* pmo, *fog, *chicken;
    coord_t pos[3];
    angle_t angle;
    int oldFlags2;

#ifdef _DEBUG
    Con_Message("P_MorphPlayer: Player %i.\n", (int)(player - players));
#endif

    if(player->morphTics)
    {
        if((player->morphTics < CHICKENTICS - TICSPERSEC) &&
           !player->powers[PT_WEAPONLEVEL2])
        {   // Make a super chicken.
            P_GivePower(player, PT_WEAPONLEVEL2);
        }
        return false;
    }

    if(player->powers[PT_INVULNERABILITY])
    {   // Immune when invulnerable.
        return false;
    }

    pmo = player->plr->mo;
    memcpy(pos, pmo->origin, sizeof(pos));
    angle = pmo->angle;
    oldFlags2 = pmo->flags2;

    if(!(chicken = P_SpawnMobj(MT_CHICPLAYER, pos, angle, 0)))
        return false;

    P_MobjChangeState(pmo, S_FREETARGMOBJ);

    if((fog = P_SpawnMobjXYZ(MT_TFOG, pos[VX], pos[VY], pos[VZ] + TELEFOGHEIGHT,
                            angle + ANG180, 0)))
        S_StartSound(SFX_TELEPT, fog);

    chicken->special1 = player->readyWeapon;
    chicken->player = player;
    chicken->dPlayer = player->plr;

    player->class_ = PCLASS_CHICKEN;
    player->health = chicken->health = MAXCHICKENHEALTH;
    player->plr->mo = chicken;
    player->armorPoints = player->armorType = 0;
    player->powers[PT_INVISIBILITY] = 0;
    player->powers[PT_WEAPONLEVEL2] = 0;

    if(oldFlags2 & MF2_FLY)
        chicken->flags2 |= MF2_FLY;

    player->morphTics = CHICKENTICS;
    player->plr->flags |= DDPF_FIXORIGIN | DDPF_FIXMOM;
    player->update |= PSF_MORPH_TIME | PSF_HEALTH | PSF_POWERS | PSF_ARMOR_POINTS;

    P_ActivateMorphWeapon(player);
    return true;
}

boolean P_MorphMonster(mobj_t* actor)
{
    mobj_t* fog, *chicken, *target;
    mobjtype_t moType;
    coord_t pos[3];
    angle_t angle;
    int ghost;

    if(actor->player)
        return false;

    moType = actor->type;
    switch(moType)
    {
    case MT_POD:
    case MT_CHICKEN:
    case MT_HEAD:
    case MT_MINOTAUR:
    case MT_SORCERER1:
    case MT_SORCERER2:
        return false;

    default:
        break;
    }

    memcpy(pos, actor->origin, sizeof(pos));
    angle = actor->angle;
    ghost = actor->flags & MF_SHADOW;
    target = actor->target;

    if((chicken = P_SpawnMobj(MT_CHICKEN, pos, angle, 0)))
    {
        P_MobjChangeState(actor, S_FREETARGMOBJ);

        if((fog = P_SpawnMobjXYZ(MT_TFOG, pos[VX], pos[VY], pos[VZ] + TELEFOGHEIGHT,
                                angle + ANG180, 0)))
            S_StartSound(SFX_TELEPT, fog);

        chicken->special2 = moType;
        chicken->special1 = CHICKENTICS + P_Random();
        chicken->flags |= ghost;
        chicken->target = target;
    }

    return true;
}

boolean P_AutoUseChaosDevice(player_t* player)
{
    int                 plrnum = player - players;

    //// \todo Do this in the inventory code?
    if(P_InventoryCount(plrnum, IIT_TELEPORT))
    {
        P_InventoryUse(plrnum, IIT_TELEPORT, false);
        P_DamageMobj(player->plr->mo, NULL, NULL,
                     player->health - (player->health + 1) / 2, false);
        return true;
    }

    return false;
}

void P_AutoUseHealth(player_t* player, int saveHealth)
{
    uint                i, count;
    int                 plrnum = player - players;
    int                 normalCount = P_InventoryCount(plrnum, IIT_HEALTH);
    int                 superCount = P_InventoryCount(plrnum, IIT_SUPERHEALTH);

    if(!player->plr->mo) return;

    //// \todo Do this in the inventory code?
    if(gameSkill == SM_BABY && normalCount * 25 >= saveHealth)
    {
        // Use quartz flasks.
        count = (saveHealth + 24) / 25;
        for(i = 0; i < count; ++i)
        {
            player->health += 25;
            P_InventoryTake(plrnum, IIT_HEALTH, false);
        }
    }
    else if(superCount * 100 >= saveHealth)
    {
        // Use mystic urns.
        count = (saveHealth + 99) / 100;
        for(i = 0; i < count; ++i)
        {
            player->health += 100;
            P_InventoryTake(plrnum, IIT_SUPERHEALTH, false);
        }
    }
    else if(gameSkill == SM_BABY &&
            superCount * 100 + normalCount * 25 >= saveHealth)
    {
        // Use mystic urns and quartz flasks.
        count = (saveHealth + 24) / 25;
        saveHealth -= count * 25;
        for(i = 0; i < count; ++i)
        {
            player->health += 25;
            P_InventoryTake(plrnum, IIT_HEALTH, false);
        }

        count = (saveHealth + 99) / 100;
        for(i = 0; i < count; ++i)
        {
            player->health += 100;
            P_InventoryTake(plrnum, IIT_SUPERHEALTH, false);
        }
    }

    player->plr->mo->health = player->health;
}

int P_DamageMobj(mobj_t* target, mobj_t* inflictor, mobj_t* source,
                 int damageP, boolean stomping)
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
 * @return              Actual amount of damage done.
 */
int P_DamageMobj2(mobj_t* target, mobj_t* inflictor, mobj_t* source,
                  int damageP, boolean stomping, boolean skipNetworkCheck)
{
    angle_t             angle;
    int                 saved, originalHealth;
    player_t*           player;
    int                 temp, damage;

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

#ifdef _DEBUG
    Con_Message("P_DamageMobj2: Damaging %i with %i points.\n", target->thinker.id, damage);
#endif

    if(!(target->flags & MF_SHOOTABLE))
    {
#ifdef _DEBUG
        Con_Message("P_DamageMobj2: Target %i is not shootable!\n", target->thinker.id);
#endif
        return 0; // Shouldn't happen...
    }

    if(target->health <= 0)
        return 0;

    if(target->player)
    {   // Player specific.
        // Check if player-player damage is disabled.
        if(source && source->player && source->player != target->player)
        {
            // Co-op damage disabled?
            if(IS_NETGAME && !deathmatch && cfg.noCoopDamage)
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
        if(target->type == MT_MINOTAUR)
        {   // Minotaur is invulnerable during charge attack.
            return 0;
        }

        target->mom[MX] = target->mom[MY] = target->mom[MZ] = 0;
    }

    player = target->player;
    if(player && gameSkill == SM_BABY)
        damage /= 2; // Take half damage in trainer mode.

    // Use the cvar damage multiplier netMobDamageModifier only if the
    // inflictor is not a player.
    if(inflictor && !inflictor->player &&
       (!source || (source && !source->player)))
    {
        // damage = (int) ((float) damage * netMobDamageModifier);
        if(IS_NETGAME)
            damage *= cfg.netMobDamageModifier;
    }

    // Special damage types.
    if(inflictor)
    {
        switch(inflictor->type)
        {
        case MT_EGGFX:
            if(player)
            {
                P_MorphPlayer(player);
            }
            else
            {
                P_MorphMonster(target);
            }
            return 0; // Does no actual "damage" but health IS modified.

        case MT_WHIRLWIND:
            {
            int                 randVal;

            target->angle += (P_Random() - P_Random()) << 20;
            target->mom[MX] += FIX2FLT((P_Random() - P_Random()) << 10);
            target->mom[MY] += FIX2FLT((P_Random() - P_Random()) << 10);

            if((mapTime & 16) && !(target->flags2 & MF2_BOSS))
            {
                randVal = P_Random();
                if(randVal > 160)
                {
                    randVal = 160;
                }

                target->mom[MZ] += FIX2FLT(randVal << 10);
                if(target->mom[MZ] > 12)
                {
                    target->mom[MZ] = 12;
                }
            }

            if(!(mapTime & 7))
            {
                return P_DamageMobj(target, NULL, NULL, 3, false);
            }
            return 0;
            }

        case MT_MINOTAUR:
            if(inflictor->flags & MF_SKULLFLY)
            {
                // Slam only when in charge mode.
                angle_t angle;
                uint an;
                coord_t thrust;
                int damageDone;

                angle = M_PointToAngle2(inflictor->origin, target->origin);
                an = angle >> ANGLETOFINESHIFT;
                thrust = 16 + FIX2FLT(P_Random() << 10);
                target->mom[MX] += thrust * FIX2FLT(finecosine[an]);
                target->mom[MY] += thrust * FIX2FLT(finesine[an]);

                damageDone = P_DamageMobj(target, NULL, NULL, HITDICE(6), false);
                if(target->player)
                {
                    target->reactionTime = 14 + (P_Random() & 7);
                }

                return damageDone;
            }
            break;

        case MT_MACEFX4:
            // Death ball.
            if((target->flags2 & MF2_BOSS) || target->type == MT_HEAD)
            {
                // Don't allow cheap boss kills.
                break;
            }
            else if(target->player)
            {
                // Player specific checks.

                // Is player invulnerable?
                if(target->player->powers[PT_INVULNERABILITY])
                    break;

                // Does the player have a Chaos Device he can use to get
                // him out of trouble?
                if(P_AutoUseChaosDevice(target->player))
                    return originalHealth - target->health; // He's lucky... this time.
            }

            // Something's gonna die.
            damage = 10000;
            break;

        case MT_PHOENIXFX2:
            // Flame thrower.
            if(target->player && P_Random() < 128)
            {
                // Freeze player for a bit.
                target->reactionTime += 4;
            }
            break;

        case MT_RAINPLR1:
        case MT_RAINPLR2:
        case MT_RAINPLR3:
        case MT_RAINPLR4:
            // Rain missiles.
            if(target->flags2 & MF2_BOSS)
            {
                // Decrease damage for bosses.
                damage = (P_Random() & 7) + 1;
            }
            break;

        case MT_HORNRODFX2:
        case MT_PHOENIXFX1:
            if(target->type == MT_SORCERER2 && P_Random() < 96)
            {   // D'Sparil teleports away, without taking damage.
                P_DSparilTeleport(target);
                return 0;
            }
            break;

        case MT_BLASTERFX1:
        case MT_RIPPER:
            if(target->type == MT_HEAD)
            {   // Less damage to Ironlich bosses.
                damage = P_Random() & 1;
                if(!damage)
                    return 0;
            }
            break;

        default:
            break;
        }
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

        if(source && source->player && (source == inflictor) &&
           source->player->powers[PT_WEAPONLEVEL2] &&
           source->player->readyWeapon == WT_FIRST)
        {
            // Staff power level 2.
            thrust = 10;

            if(!(target->flags & MF_NOGRAVITY))
                target->mom[MZ] += 5;
        }

        an = angle >> ANGLETOFINESHIFT;
        target->mom[MX] += thrust * FIX2FLT(finecosine[an]);
        target->mom[MY] += thrust * FIX2FLT(finesine[an]);
        NetSv_PlayerMobjImpulse(target, thrust * FIX2FLT(finecosine[an]), thrust * FIX2FLT(finesine[an]), 0);

        // $dropoff_fix: thrust objects hanging off ledges.
        if((target->intFlags & MIF_FALLING) && target->gear >= MAXGEAR)
            target->gear = 0;
    }

    // Player specific.
    if(player)
    {
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
                saved = damage / 2;
            else
                saved = damage / 2 + damage / 4;

            if(player->armorPoints <= saved)
            {   // Armor is used up.
                saved = player->armorPoints;
                player->armorType = 0;
            }

            player->armorPoints -= saved;
            player->update |= PSF_ARMOR_POINTS;
            damage -= saved;
        }

        if(damage >= player->health &&
           ((gameSkill == SM_BABY) || deathmatch) && !player->morphTics)
        {   // Try to use some inventory health.
            P_AutoUseHealth(player, damage - player->health + 1);
        }

        player->health -= damage;
        if(player->health < 0)
            player->health = 0;

        player->update |= PSF_HEALTH;
        player->attacker = source;

        player->damageCount += damage; // Add damage after armor / invuln.
        if(player->damageCount > 100)
            player->damageCount = 100; // Teleport stomp does 10k points...

        temp = damage < 100 ? damage : 100;

        // Maybe unhide the HUD?
        ST_HUDUnHide(player - players, HUE_ON_DAMAGE);
    }

    // How about some particles, yes?
    // Only works when both target and inflictor are real mobjs.
    P_SpawnDamageParticleGen(target, inflictor, damage);

    // Do the damage.
    target->health -= damage;
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
           !target->threshold && !(source->flags3 & MF3_NOINFIGHT) &&
           !(target->type == MT_SORCERER2 && source->type == MT_WIZARD))
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
        target->special1 = damage;
        if(target->type == MT_POD && source && source->type != MT_POD)
        {
            // Make sure players get frags for chain-reaction kills.
            target->target = source;
        }

        if(player && inflictor && !player->morphTics)
        {
            // Check for flame death.
            if((inflictor->flags2 & MF2_FIREDAMAGE) ||
               ((inflictor->type == MT_PHOENIXFX1) && (target->health > -50) &&
                (damage > 25)))
            {
                target->flags2 |= MF2_FIREDAMAGE;
            }
        }

        P_KillMobj(source, target);
    }

    return originalHealth - target->health;

#undef BASETHRESHOLD
}
