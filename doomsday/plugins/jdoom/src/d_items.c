/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * d_items.c: Weapons, ammos, healthpacks etc, etc...
 */

// HEADER FILES ------------------------------------------------------------

#include "jdoom.h"

#include "g_defs.h"
#include "p_player.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

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
     S_PUNCHUP,
     0,            // raise sound id
     S_PUNCHDOWN,
     S_PUNCH,
     0,            // ready sound
     S_PUNCH1,
     S_NULL
    }
   },
   {
    { // pistol
     GM_ANY,
     {1, 0, 0, 0}, // type: clip | shell | cell | misl
     {1, 0, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     S_PISTOLUP,
     0,            // raise sound id
     S_PISTOLDOWN,
     S_PISTOL,
     0,            // ready sound
     S_PISTOL1,
     S_PISTOLFLASH
    }
   },
   {
    { // shotgun
     GM_ANY,
     {0, 1, 0, 0}, // type: clip | shell | cell | misl
     {0, 1, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     S_SGUNUP,
     0,            // raise sound id
     S_SGUNDOWN,
     S_SGUN,
     0,            // ready sound
     S_SGUN1,
     S_SGUNFLASH1
    }
   },
   {
    { // chaingun
     GM_ANY,
     {1, 0, 0, 0}, // type: clip | shell | cell | misl
     {1, 0, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     S_CHAINUP,
     0,            // raise sound id
     S_CHAINDOWN,
     S_CHAIN,
     0,            // ready sound
     S_CHAIN1,
     S_CHAINFLASH1
    }
   },
   {
    { // missile launcher
     GM_ANY,
     {0, 0, 0, 1}, // type: clip | shell | cell | misl
     {0, 0, 0, 1}, // pershot: clip | shell | cell | misl
     false,        // autofire when raised if fire held
     S_MISSILEUP,
     0,            // raise sound id
     S_MISSILEDOWN,
     S_MISSILE,
     0,            // ready sound
     S_MISSILE1,
     S_MISSILEFLASH1
    }
   },
   {
    { // plasma rifle
     GM_NOTSHAREWARE,
     {0, 0, 1, 0}, // type: clip | shell | cell | misl
     {0, 0, 1, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     S_PLASMAUP,
     0,            // raise sound id
     S_PLASMADOWN,
     S_PLASMA,
     0,            // ready sound
     S_PLASMA1,
     S_PLASMAFLASH1
    }
   },
   {
    { // bfg 9000
     GM_NOTSHAREWARE,
     {0, 0, 1, 0},  // type: clip | shell | cell | misl
     {0, 0, 40, 0}, // pershot: clip | shell | cell | misl
     false,         // autofire when raised if fire held
     S_BFGUP,
     0,            // raise sound id
     S_BFGDOWN,
     S_BFG,
     0,            // ready sound
     S_BFG1,
     S_BFGFLASH1
    }
   },
   {
    { // chainsaw
     GM_ANY,
     {0, 0, 0, 0}, // type: clip | shell | cell | misl
     {0, 0, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     S_SAWUP,
     SFX_SAWUP,    // raise sound id
     S_SAWDOWN,
     S_SAW,
     SFX_SAWIDL,   // ready sound
     S_SAW1,
     S_NULL
    }
   },
   {
    { // super shotgun
     GM_COMMERCIAL,
     {0, 1, 0, 0}, // type: clip | shell | cell | misl
     {0, 2, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     S_DSGUNUP,
     0,            // raise sound id
     S_DSGUNDOWN,
     S_DSGUN,
     0,            // ready sound
     S_DSGUN1,
     S_DSGUNFLASH1
    }
   }
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char* ammoTypeNames[NUM_AMMO_TYPES] =
    {"clip", "shell", "cell", "misl"};

// CODE --------------------------------------------------------------------

/**
 * Initialize ammo info.
 */
void P_InitAmmoInfo(void)
{
    uint                i;
    char                buf[40];

    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        // Max ammo.
        sprintf(buf, "Player|Max ammo|%s", ammoTypeNames[i]);
        GetDefInt(buf, &maxAmmo[i]);

        // Clip ammo.
        sprintf(buf, "Player|Clip ammo|%s", ammoTypeNames[i]);
        GetDefInt(buf, &clipAmmo[i]);
    }
}

/**
 * Initialize weapon info.
 */
void P_InitWeaponInfo(void)
{
#define WPINF               "Weapon Info|"

    int                 i;
    int                 pclass = PCLASS_PLAYER;
    ammotype_t          k;
    char                buf[80];
    char*               data;

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        //// \todo Only allows for one type of ammo per weapon.
        sprintf(buf, WPINF "%i|Type", i);
        if(Def_Get(DD_DEF_VALUE, buf, &data))
        {
            // Set the right types of ammo.
            if(!stricmp(data, "noammo"))
            {
                for(k = 0; k < NUM_AMMO_TYPES; ++k)
                {
                    weaponInfo[i][pclass].mode[0].ammoType[k] = false;
                    weaponInfo[i][pclass].mode[0].perShot[k] = 0;
                }
            }
            else
            {
                for(k = 0; k < NUM_AMMO_TYPES; ++k)
                {
                    if(!stricmp(data, ammoTypeNames[k]))
                    {
                        weaponInfo[i][pclass].mode[0].ammoType[k] = true;

                        sprintf(buf, WPINF "%i|Per shot", i);
                        GetDefInt(buf, &weaponInfo[i][pclass].mode[0].perShot[k]);
                        break;
                    }
                }
            }
        }
        // end todo

        sprintf(buf, WPINF "%i|Up", i);
        GetDefState(buf, &weaponInfo[i][pclass].mode[0].upState);
        sprintf(buf, WPINF "%i|Down", i);
        GetDefState(buf, &weaponInfo[i][pclass].mode[0].downState);
        sprintf(buf, WPINF "%i|Ready", i);
        GetDefState(buf, &weaponInfo[i][pclass].mode[0].readyState);
        sprintf(buf, WPINF "%i|Atk", i);
        GetDefState(buf, &weaponInfo[i][pclass].mode[0].attackState);
        sprintf(buf, WPINF "%i|Flash", i);
        GetDefState(buf, &weaponInfo[i][pclass].mode[0].flashState);
        sprintf(buf, WPINF "%i|Static", i);
        weaponInfo[i][pclass].mode[0].staticSwitch = GetDefInt(buf, 0);
    }

    /// \todo Get this info from values.
    P_InitWeaponSlots();

    P_SetWeaponSlot(WT_FIRST, 1);
    P_SetWeaponSlot(WT_EIGHTH, 1);
    P_SetWeaponSlot(WT_SECOND, 2);
    P_SetWeaponSlot(WT_THIRD, 3);
    if(gameMode == commercial)
        P_SetWeaponSlot(WT_NINETH, 3);
    P_SetWeaponSlot(WT_FOURTH, 4);
    P_SetWeaponSlot(WT_FIFTH, 5);
    P_SetWeaponSlot(WT_SIXTH, 6);
    P_SetWeaponSlot(WT_SEVENTH, 7);

#undef WPINF
}

void P_InitPlayerValues(player_t *p)
{
    int                 i;
    char                buf[40];

    GetDefInt("Player|Health", &p->health);
    GetDefInt("Player|Weapon", (int *) &p->readyWeapon);
    p->pendingWeapon = p->readyWeapon;

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        sprintf(buf, "Weapon Info|%i|Owned", i);
        GetDefInt(buf, (int *) &p->weapons[i].owned);
    }

    for(i = 0; i < NUM_AMMO_TYPES; ++i)
    {
        sprintf(buf, "Player|Init ammo|%s", ammoTypeNames[i]);
        GetDefInt(buf, &p->ammo[i].owned);
    }
}
