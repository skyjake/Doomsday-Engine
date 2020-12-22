/** @file d_items.cpp  Weapons, ammos, healthpacks etc, etc...
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "jdoom.h"

#include <de/string.h>
#include "g_defs.h"
#include "player.h"

using namespace de;

/**
 * Default weapon definitions.
 * These are used if other (external) definitions are not found.
 */
weaponinfo_t weaponInfo[NUM_WEAPON_TYPES][NUM_PLAYER_CLASSES] = {
   {
    { // fist
     GM_ANY,
     {0, 0, 0, 0}, // type: clip | shell | cell | misl
     {0, 0, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     { S_PUNCHUP, S_PUNCHDOWN, S_PUNCH, S_PUNCH1, S_NULL },
     0,            // raise sound id
     0,            // ready sound
     0             // static switch
    }
   },
   {
    { // pistol
     GM_ANY,
     {1, 0, 0, 0}, // type: clip | shell | cell | misl
     {1, 0, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     { S_PISTOLUP, S_PISTOLDOWN, S_PISTOL, S_PISTOL1, S_PISTOLFLASH },
     0,            // raise sound id
     0,            // ready sound
     0             // static switch
    }
   },
   {
    { // shotgun
     GM_ANY,
     {0, 1, 0, 0}, // type: clip | shell | cell | misl
     {0, 1, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     { S_SGUNUP, S_SGUNDOWN, S_SGUN, S_SGUN1, S_SGUNFLASH1 },
     0,            // raise sound id
     0,            // ready sound
     0             // static switch
    }
   },
   {
    { // chaingun
     GM_ANY,
     {1, 0, 0, 0}, // type: clip | shell | cell | misl
     {1, 0, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     { S_CHAINUP, S_CHAINDOWN, S_CHAIN, S_CHAIN1, S_CHAINFLASH1 },
     0,            // raise sound id
     0,            // ready sound
     0             // static switch
    }
   },
   {
    { // missile launcher
     GM_ANY,
     {0, 0, 0, 1}, // type: clip | shell | cell | misl
     {0, 0, 0, 1}, // pershot: clip | shell | cell | misl
     false,        // autofire when raised if fire held
     { S_MISSILEUP, S_MISSILEDOWN, S_MISSILE, S_MISSILE1, S_MISSILEFLASH1 },
     0,            // raise sound id
     0,            // ready sound
     0             // static switch
    }
   },
   {
    { // plasma rifle
     GM_ANY & ~GM_DOOM_SHAREWARE,
     {0, 0, 1, 0}, // type: clip | shell | cell | misl
     {0, 0, 1, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     { S_PLASMAUP, S_PLASMADOWN, S_PLASMA, S_PLASMA1, S_PLASMAFLASH1 },
     0,            // raise sound id
     0,            // ready sound
     0             // static switch
    }
   },
   {
    { // bfg 9000
     GM_ANY & ~GM_DOOM_SHAREWARE,
     {0, 0, 1, 0},  // type: clip | shell | cell | misl
     {0, 0, 40, 0}, // pershot: clip | shell | cell | misl
     false,         // autofire when raised if fire held
     { S_BFGUP, S_BFGDOWN, S_BFG, S_BFG1, S_BFGFLASH1 },
     0,            // raise sound id
     0,            // ready sound
     0             // static switch
    }
   },
   {
    { // chainsaw
     GM_ANY,
     {0, 0, 0, 0}, // type: clip | shell | cell | misl
     {0, 0, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     { S_SAWUP, S_SAWDOWN, S_SAW, S_SAW1, S_NULL },
     SFX_SAWUP,    // raise sound id
     SFX_SAWIDL,   // ready sound
     0             // static switch
    }
   },
   {
    { // super shotgun
     GM_ANY_DOOM2,
     {0, 1, 0, 0}, // type: clip | shell | cell | misl
     {0, 2, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     { S_DSGUNUP, S_DSGUNDOWN, S_DSGUN, S_DSGUN1, S_DSGUNFLASH1 },
     0,            // raise sound id
     0,            // ready sound
     0             // static switch
    }
   }
};

static String ammoTypeName(int ammoType)
{
    static String names[NUM_AMMO_TYPES] = {
        /*AT_CLIP*/     "clip",
        /*AT_SHELL*/    "shell",
        /*AT_CELL*/     "cell",
        /*AT_MISSILE*/  "misl"
    };
    if(ammoType >= AT_FIRST && ammoType < NUM_AMMO_TYPES)
        return names[ammoType - AT_FIRST];
    throw Error("ammoTypeName", "Unknown ammo type " + String::asText(ammoType));
}

static String weaponStateName(int weaponState)
{
    static String names[NUM_WEAPON_STATE_NAMES] = {
        /*WSN_UP*/      "Up",
        /*WSN_DOWN*/    "Down",
        /*WSN_READY*/   "Ready",
        /*WSN_ATTACK*/  "Atk",
        /*WSN_FLASH*/   "Flash"
    };
    if(weaponState >= WSN_UP && weaponState < NUM_WEAPON_STATE_NAMES)
        return names[weaponState - WSN_UP];
    throw Error("weaponStateName", "Unknown weapon state " + String::asText(weaponState));
}

/**
 * Initialize ammo info.
 */
void P_InitAmmoInfo()
{
    for(auto i = int( AT_FIRST ); i < NUM_AMMO_TYPES; ++i)
    {
        const String name = ammoTypeName(i);

        if(const ded_value_t *maxAmmo = Defs().getValueById("Player|Max ammo|" + name))
        {
            ::maxAmmo[i] = String(maxAmmo->text).toInt();
        }

        if(const ded_value_t *clipAmmo = Defs().getValueById("Player|Clip ammo|" + name))
        {
            ::clipAmmo[i] = String(clipAmmo->text).toInt();
        }
    }
}

/**
 * Initialize weapon info.
 */
void P_InitWeaponInfo()
{
    for(auto i = int( WT_FIRST ); i < NUM_WEAPON_TYPES; ++i)
    {
        const auto id = String::asText(i);

        weaponmodeinfo_t *wminfo = WEAPON_INFO(i, PCLASS_PLAYER, 0);
        DE_ASSERT(wminfo);

        /// @todo Only allows for one type of ammo per weapon.
        if(const ded_value_t *ammo = Defs().getValueById("Weapon Info|" + id + "|Type"))
        {
            de::zap(wminfo->ammoType);
            de::zap(wminfo->perShot);

            if(String(ammo->text).compareWithoutCase("noammo"))
            {
                for(auto k = int( AT_FIRST ); k < NUM_AMMO_TYPES; ++k)
                {
                    if(!ammoTypeName(k).compareWithoutCase(ammo->text))
                    {
                        wminfo->ammoType[k] = true;
                        if(const ded_value_t *perShot = Defs().getValueById("Weapon Info|" + id + "|Per shot"))
                        {
                            wminfo->perShot[k] = String(perShot->text).toInt();
                        }
                        break;
                    }
                }
            }
        }
        // end todo

        for(auto k = int( WSN_UP ); k < NUM_WEAPON_STATE_NAMES; ++k)
        {
            if(const ded_value_t *state = Defs().getValueById("Weapon Info|" + id + "|" + weaponStateName(k)))
            {
                wminfo->states[k] = de::max<int>(S_NULL, Defs().getStateNum(state->text));
            }
        }

        if(const ded_value_t *staticSwitch = Defs().getValueById("Weapon Info|" + id + "|Static"))
        {
            wminfo->staticSwitch = String(staticSwitch->text).toInt();
        }
    }

    /// @todo Get this info from values.
    P_InitWeaponSlots();

    P_SetWeaponSlot(WT_FIRST, 1);
    P_SetWeaponSlot(WT_EIGHTH, 1);
    P_SetWeaponSlot(WT_SECOND, 2);
    P_SetWeaponSlot(WT_THIRD, 3);
    if(::gameModeBits & GM_ANY_DOOM2)
        P_SetWeaponSlot(WT_NINETH, 3);
    P_SetWeaponSlot(WT_FOURTH, 4);
    P_SetWeaponSlot(WT_FIFTH, 5);
    P_SetWeaponSlot(WT_SIXTH, 6);
    P_SetWeaponSlot(WT_SEVENTH, 7);
}

void P_InitPlayerValues(player_t *plr)
{
    DE_ASSERT(plr);

    if(const ded_value_t *health = Defs().getValueById("Player|Health"))
    {
        plr->health = String(health->text).toInt();
    }

    if(const ded_value_t *weapon = Defs().getValueById("Player|Weapon"))
    {
        plr->readyWeapon = weapontype_t( String(weapon->text).toInt() );
    }
    plr->pendingWeapon = plr->readyWeapon;

    for(auto i = int( WT_FIRST ); i < NUM_WEAPON_TYPES; ++i)
    {
        if(const ded_value_t *owned = Defs().getValueById("Weapon Info|" + String::asText(i) + "|Owned"))
        {
            plr->weapons[i].owned = String(owned->text).toInt();
        }
    }

    for(auto i = int( AT_FIRST ); i < NUM_AMMO_TYPES; ++i)
    {
        if(const ded_value_t *owned = Defs().getValueById("Player|Init ammo|" + ammoTypeName(i)))
        {
            plr->ammo[i].owned = String(owned->text).toInt();
        }
    }
}
