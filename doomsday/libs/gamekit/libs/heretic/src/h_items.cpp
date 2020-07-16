/** @file d_items.cpp  Weapons, ammos, healthpacks etc, etc...
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Activision
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

#include "jheretic.h"

#include <de/string.h>
#include "g_defs.h"
#include "player.h"

using namespace de;

/*
    AT_CRYSTAL,
    AT_ARROW,
    AT_ORB,
    AT_RUNE,
    AT_FIREORB,
    AT_MSPHERE,
*/
weaponinfo_t weaponInfo[NUM_WEAPON_TYPES][NUM_PLAYER_CLASSES] = {
  {
   {
    {
    { // Staff
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_STAFFUP,  S_STAFFDOWN, S_STAFFREADY, S_STAFFATK1_1,  S_STAFFATK1_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // Staff lvl2
    {
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_STAFFUP2, S_STAFFDOWN2, S_STAFFREADY2_1, S_STAFFATK2_1, S_STAFFATK2_1, S_NULL },
     0,                  // raise sound id
     SFX_STFCRK,         // readysound
     0                   // static switch
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK1_1, S_BEAKATK1_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // lvl2
    {
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK2_1, S_BEAKATK2_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    }
    }
   }
  },
  {
   {
    {
    { // Gold wand
     GM_ANY,             // gamemodebits
     {1, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {USE_GWND_AMMO_1, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_GOLDWANDUP, S_GOLDWANDDOWN, S_GOLDWANDREADY, S_GOLDWANDATK1_1, S_GOLDWANDATK1_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // lvl2
    {
     GM_ANY,             // gamemodebits
     {1, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {USE_GWND_AMMO_2, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_GOLDWANDUP, S_GOLDWANDDOWN, S_GOLDWANDREADY, S_GOLDWANDATK2_1, S_GOLDWANDATK2_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK1_1, S_BEAKATK1_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // lvl2
    {
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK2_1, S_BEAKATK2_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    }
    }
   }
  },
  {
   {
    {
    { // Crossbow
     GM_ANY,             // gamemodebits
     {0, 1, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, USE_CBOW_AMMO_1, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_CRBOWUP, S_CRBOWDOWN, S_CRBOW1, S_CRBOWATK1_1, S_CRBOWATK1_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // lvl2
    {
     GM_ANY,             // gamemodebits
     {0, 1, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, USE_CBOW_AMMO_2, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_CRBOWUP, S_CRBOWDOWN, S_CRBOW1, S_CRBOWATK2_1, S_CRBOWATK2_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK1_1, S_BEAKATK1_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // lvl2
    {
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK2_1, S_BEAKATK2_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    }
    }
   }
  },
  {
   {
    {
    { // Blaster
     GM_ANY,             // gamemodebits
     {0, 0, 1, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, USE_BLSR_AMMO_1, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BLASTERUP, S_BLASTERDOWN, S_BLASTERREADY, S_BLASTERATK1_1, S_BLASTERATK1_3, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // lvl2
    {
     GM_ANY,             // gamemodebits
     {0, 0, 1, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, USE_BLSR_AMMO_2, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BLASTERUP, S_BLASTERDOWN, S_BLASTERREADY, S_BLASTERATK2_1, S_BLASTERATK2_3, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK1_1, S_BEAKATK1_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // lvl2
    {
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK2_1, S_BEAKATK2_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    }
    }
   }
  },
  {
   {
    {
    { // Skull rod
     GM_NOT_SHAREWARE,   // gamemodebits
     {0, 0, 0, 1, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, USE_SKRD_AMMO_1, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_HORNRODUP, S_HORNRODDOWN, S_HORNRODREADY, S_HORNRODATK1_1, S_HORNRODATK1_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // lvl2
    {
     GM_NOT_SHAREWARE,   // gamemodebits
     {0, 0, 0, 1, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, USE_SKRD_AMMO_2, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_HORNRODUP, S_HORNRODDOWN, S_HORNRODREADY, S_HORNRODATK2_1, S_HORNRODATK2_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK1_1, S_BEAKATK1_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // lvl2
    {
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK2_1, S_BEAKATK2_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    }
    }
   }
  },
  {
   {
    {
    { // Phoenix rod
     GM_NOT_SHAREWARE,   // gamemodebits
     {0, 0, 0, 0, 1, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, USE_PHRD_AMMO_1, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     false,              // autofire when raised if fire held
     { S_PHOENIXUP, S_PHOENIXDOWN, S_PHOENIXREADY, S_PHOENIXATK1_1, S_PHOENIXATK1_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // lvl2
    {
     GM_NOT_SHAREWARE,   // gamemodebits
     {0, 0, 0, 0, 1, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, USE_PHRD_AMMO_2, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     false,              // autofire when raised if fire held
     { S_PHOENIXUP, S_PHOENIXDOWN, S_PHOENIXREADY, S_PHOENIXATK2_1, S_PHOENIXATK2_2, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK1_1, S_BEAKATK1_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // lvl2
    {
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK2_1, S_BEAKATK2_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    }
    }
   }
  },
  {
   {
    {
    { // Mace
     GM_NOT_SHAREWARE,   // gamemodebits
     {0, 0, 0, 0, 0, 1}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, USE_MACE_AMMO_1}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_MACEUP, S_MACEDOWN, S_MACEREADY, S_MACEATK1_1, S_MACEATK1_2, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // lvl2
    {
     GM_NOT_SHAREWARE,   // gamemodebits
     {0, 0, 0, 0, 0, 1}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, USE_MACE_AMMO_2}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_MACEUP, S_MACEDOWN, S_MACEREADY, S_MACEATK2_1, S_MACEATK2_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK1_1, S_BEAKATK1_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // lvl2
    {
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK2_1, S_BEAKATK2_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    }
    }
   }
  },
  {
   {
    {
    { // Gauntlets
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_GAUNTLETUP, S_GAUNTLETDOWN, S_GAUNTLETREADY, S_GAUNTLETATK1_1, S_GAUNTLETATK1_3, S_NULL },
     SFX_GNTACT,         // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // lvl2
    {
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_GAUNTLETUP2, S_GAUNTLETDOWN2, S_GAUNTLETREADY2_1, S_GAUNTLETATK2_1, S_GAUNTLETATK2_3, S_NULL },
     SFX_GNTACT,         // raise sound id
     0,                  // readysound
     0                   // static switch
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK1_1, S_BEAKATK1_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    },
    // lvl2
    {
     GM_ANY,             // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     { S_BEAKUP, S_BEAKDOWN, S_BEAKREADY, S_BEAKATK2_1, S_BEAKATK2_1, S_NULL },
     0,                  // raise sound id
     0,                  // readysound
     0                   // static switch
    }
    }
   }
  }
};

const AmmoDef *P_AmmoDef(ammotype_t type)
{
    static AmmoDef const ammoDefs[NUM_AMMO_TYPES] = {
        /*AT_CRYSTAL*/  { GM_ANY,           "INAMGLD" },
        /*AT_ARROW*/    { GM_ANY,           "INAMBOW" },
        /*AT_ORB*/      { GM_ANY,           "INAMBST" },
        /*AT_RUNE*/     { GM_NOT_SHAREWARE, "INAMRAM" },
        /*AT_FIREORB*/  { GM_NOT_SHAREWARE, "INAMPNX" },
        /*AT_MSPHERE*/  { GM_NOT_SHAREWARE, "INAMLOB" }
    };
    if(type >= AT_FIRST && type < AT_FIRST + NUM_AMMO_TYPES)
        return &ammoDefs[type];
    return nullptr;  // Not found.
}

/**
 * Initialize weapon info, maxammo and clipammo.
 */
void P_InitWeaponInfo()
{
    for (auto i = int(WT_FIRST); i < NUM_WEAPON_TYPES; ++i)
    {
        const String id = String::asText(i);

        for (int k = 0; k < 2; ++k) // Each firing mode.
        {
            // Firing modes other than @c 0 use a sublevel.
            const String mode = (k == 0 ? "" : "|" + String::asText(k + 1));
            const String key = "Weapon Info|" + id + mode + "|";

            weaponmodeinfo_t *wminfo = WEAPON_INFO(i, PCLASS_PLAYER, k);
            DE_ASSERT(wminfo);

            // Per shot ammo.
            {
                int definedAmmoTypes = 0;
                for (int a = AT_FIRST; a < NUM_AMMO_TYPES; ++a)
                {
                    if (const ded_value_t *perShot = Defs().getValueById(key + "Per shot|" + ammoName[a]))
                    {
                        wminfo->perShot[a] = String(perShot->text).toInt();
                        definedAmmoTypes |= 1 << a;
                    }
                }
                if (definedAmmoTypes)
                {
                    // Clear the other ammo amounts.
                    for (int a = AT_FIRST; a < NUM_AMMO_TYPES; ++a)
                    {
                        if ((definedAmmoTypes & (1 << a)) == 0)
                        {
                            wminfo->perShot[a] = 0;
                        }
                    }
                }
            }

            if (const ded_value_t *staticSwitch = Defs().getValueById(key + "Static"))
            {
                wminfo->staticSwitch = String(staticSwitch->text).toInt();
            }
        }
    }

    /// @todo Get this info from values.
    P_InitWeaponSlots();

    P_SetWeaponSlot(WT_FIRST, 1);
    P_SetWeaponSlot(WT_EIGHTH, 1);
    P_SetWeaponSlot(WT_SECOND, 2);
    P_SetWeaponSlot(WT_THIRD, 3);
    P_SetWeaponSlot(WT_FOURTH, 4);
    P_SetWeaponSlot(WT_FIFTH, 5);
    P_SetWeaponSlot(WT_SIXTH, 6);
    P_SetWeaponSlot(WT_SEVENTH, 7);
}
