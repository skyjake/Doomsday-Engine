/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#include "wolftc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

//
// These are used if other definitions are not found.
//
weaponinfo_t weaponinfo[NUM_WEAPON_TYPES][NUM_PLAYER_CLASSES] = {
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
     sfx_sawup,    // raise sound id
     S_SAWDOWN,
     S_SAW,
     sfx_sawidl,   // ready sound
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

// CODE --------------------------------------------------------------------

/*
 * Return the default for a value (retrieved from Doomsday)
 */
int GetDefInt(char *def, int *returned_value)
{
    char   *data;
    int     val;

    // Get the value.
    if(!Def_Get(DD_DEF_VALUE, def, &data))
        return 0;               // No such value...
    // Convert to integer.
    val = strtol(data, 0, 0);
    if(returned_value)
        *returned_value = val;
    return val;
}

void GetDefState(char *def, int *val)
{
    char   *data;

    // Get the value.
    if(!Def_Get(DD_DEF_VALUE, def, &data))
        return;
    // Get the state number.
    *val = Def_Get(DD_DEF_STATE, data, 0);
    if(*val < 0)
        *val = 0;
}

/*
 *Initialize weapon info, maxammo and clipammo.
 */
void P_InitWeaponInfo()
{
#define PLMAX "Player|Max ammo|"
#define PLCLP "Player|Clip ammo|"
#define WPINF "Weapon Info|"

    int     i;
    int     pclass = PCLASS_PLAYER;
    ammotype_t k;
    char    buf[80];
    char   *data;
    char   *ammotypes[NUM_AMMO_TYPES] = { "clip", "shell", "cell", "misl"};

    // Max ammo.
    GetDefInt(PLMAX "Clip", &maxammo[AT_CLIP]);
    GetDefInt(PLMAX "Shell", &maxammo[AT_SHELL]);
    GetDefInt(PLMAX "Cell", &maxammo[AT_CELL]);
    GetDefInt(PLMAX "Misl", &maxammo[AT_MISSILE]);

    // Clip ammo.
    GetDefInt(PLCLP "Clip", &clipammo[AT_CLIP]);
    GetDefInt(PLCLP "Shell", &clipammo[AT_SHELL]);
    GetDefInt(PLCLP "Cell", &clipammo[AT_CELL]);
    GetDefInt(PLCLP "Misl", &clipammo[AT_MISSILE]);

    for(i = 0; i < NUM_WEAPON_TYPES; i++)
    {
        // TODO: Only allows for one type of ammo per weapon.
        sprintf(buf, WPINF "%i|Type", i);
        if(Def_Get(DD_DEF_VALUE, buf, &data))
        {
            // Set the right types of ammo.
            if(!stricmp(data, "noammo"))
            {
                for(k = 0; k < NUM_AMMO_TYPES; ++k)
                {
                    weaponinfo[i][pclass].mode[0].ammotype[k] = false;
                    weaponinfo[i][pclass].mode[0].pershot[k] = 0;
                }
            }
            else
            {
                for(k = 0; k < NUM_AMMO_TYPES; ++k)
                {
                    if(!stricmp(data, ammotypes[k]))
                    {
                        weaponinfo[i][pclass].mode[0].ammotype[k] = true;

                        sprintf(buf, WPINF "%i|Per shot", i);
                        GetDefInt(buf, &weaponinfo[i][pclass].mode[0].pershot[k]);
                        break;
                    }
                }
            }
        }
        // end todo

        sprintf(buf, WPINF "%i|Up", i);
        GetDefState(buf, &weaponinfo[i][pclass].mode[0].upstate);
        sprintf(buf, WPINF "%i|Down", i);
        GetDefState(buf, &weaponinfo[i][pclass].mode[0].downstate);
        sprintf(buf, WPINF "%i|Ready", i);
        GetDefState(buf, &weaponinfo[i][pclass].mode[0].readystate);
        sprintf(buf, WPINF "%i|Atk", i);
        GetDefState(buf, &weaponinfo[i][pclass].mode[0].atkstate);
        sprintf(buf, WPINF "%i|Flash", i);
        GetDefState(buf, &weaponinfo[i][pclass].mode[0].flashstate);
        sprintf(buf, WPINF "%i|Static", i);
        weaponinfo[i][pclass].mode[0].static_switch = GetDefInt(buf, 0);
    }
}

void P_InitPlayerValues(player_t *p)
{
#define PLINA "Player|Init ammo|"
    int     i;
    char    buf[20];

    GetDefInt("Player|Health", &p->health);
    GetDefInt("Player|Weapon", (int *) &p->readyweapon);
    p->pendingweapon = p->readyweapon;
    for(i = 0; i < NUM_WEAPON_TYPES; i++)
    {
        sprintf(buf, "Weapon Info|%i|Owned", i);
        GetDefInt(buf, (int *) &p->weaponowned[i]);
    }
    GetDefInt(PLINA "Clip", &p->ammo[AT_CLIP]);
    GetDefInt(PLINA "Shell", &p->ammo[AT_SHELL]);
    GetDefInt(PLINA "Cell", &p->ammo[AT_CELL]);
    GetDefInt(PLINA "Misl", &p->ammo[AT_MISSILE]);
}
