/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
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
 * d_items.c:
 */

// HEADER FILES ------------------------------------------------------------

#include <string.h>
#include <stdio.h>

#include "jdoom64.h"

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
     { S_PUNCHUP, S_PUNCHDOWN, S_PUNCH, S_PUNCH1, S_NULL },
     0,            // raise sound id
     0             // ready sound
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
     0             // ready sound
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
     0             // ready sound
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
     0             // ready sound
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
     0             // ready sound
    }
   },
   {
    { // plasma rifle
     GM_ANY,
     {0, 0, 1, 0}, // type: clip | shell | cell | misl
     {0, 0, 1, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     { S_PLASMAUP, S_PLASMADOWN, S_PLASMA, S_PLASMA1, S_PLASMASHOCK1 },
     0,            // raise sound id
     0             // ready sound
    }
   },
   {
    { // bfg 9000
     GM_ANY,
     {0, 0, 1, 0},  // type: clip | shell | cell | misl
     {0, 0, 40, 0}, // pershot: clip | shell | cell | misl
     false,         // autofire when raised if fire held
     { S_BFGUP, S_BFGDOWN, S_BFG, S_BFG1, S_BFGFLASH1 },
     0,            // raise sound id
     0             // ready sound
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
     SFX_SAWIDL    // ready sound
    }
   },
   {
    { // super shotgun
     GM_ANY,
     {0, 1, 0, 0}, // type: clip | shell | cell | misl
     {0, 2, 0, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     { S_DSGUNUP, S_DSGUNDOWN, S_DSGUN, S_DSGUN1, S_DSGUNFLASH1 },
     0,            // raise sound id
     0             // ready sound
    }
   },
   {
    { // UNMAKER
     GM_ANY,
     {0, 0, 1, 0}, // type: clip | shell | cell | misl
     {0, 0, 1, 0}, // pershot: clip | shell | cell | misl
     true,         // autofire when raised if fire held
     { S_UNKFUP, S_UNKFDOWN, S_UNKF, S_UNKF1, S_UNKFLASH1 },
     0,            // raise sound id
     0             // ready sound
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
    char                buf[80];
    char*               data;

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        //// \todo Only allows for one type of ammo per weapon.
        sprintf(buf, WPINF "%i|Type", i);
        if(Def_Get(DD_DEF_VALUE, buf, &data))
        {
            memset(weaponInfo[i][pclass].mode[0].ammoType, 0,
                   sizeof(int) * NUM_AMMO_TYPES);
            memset(weaponInfo[i][pclass].mode[0].perShot, 0,
                   sizeof(int) * NUM_AMMO_TYPES);

            if(stricmp(data, "noammo"))
            {
                ammotype_t          k;

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
        GetDefState(buf, &weaponInfo[i][pclass].mode[0].states[WSN_UP]);
        sprintf(buf, WPINF "%i|Down", i);
        GetDefState(buf, &weaponInfo[i][pclass].mode[0].states[WSN_DOWN]);
        sprintf(buf, WPINF "%i|Ready", i);
        GetDefState(buf, &weaponInfo[i][pclass].mode[0].states[WSN_READY]);
        sprintf(buf, WPINF "%i|Atk", i);
        GetDefState(buf, &weaponInfo[i][pclass].mode[0].states[WSN_ATTACK]);
        sprintf(buf, WPINF "%i|Flash", i);
        GetDefState(buf, &weaponInfo[i][pclass].mode[0].states[WSN_FLASH]);
        sprintf(buf, WPINF "%i|Static", i);
        weaponInfo[i][pclass].mode[0].staticSwitch = GetDefInt(buf, 0);
    }

    /// \todo Get this info from values.
    P_InitWeaponSlots();

    P_SetWeaponSlot(WT_FIRST, 1);
    P_SetWeaponSlot(WT_EIGHTH, 1);
    P_SetWeaponSlot(WT_SECOND, 2);
    P_SetWeaponSlot(WT_THIRD, 3);
    P_SetWeaponSlot(WT_NINETH, 3);
    P_SetWeaponSlot(WT_FOURTH, 4);
    P_SetWeaponSlot(WT_FIFTH, 5);
    P_SetWeaponSlot(WT_SIXTH, 6);
    P_SetWeaponSlot(WT_SEVENTH, 7);
    P_SetWeaponSlot(WT_TENTH, 8);
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
