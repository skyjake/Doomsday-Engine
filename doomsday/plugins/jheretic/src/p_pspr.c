/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * p_pspr.c: Weapon sprite animation, weapon objects.
 *
 * Action functions for weapons.
 */

#ifdef MSVC
// Sumtin' 'ere messes with poor ol' MSVC's head...
#  pragma optimize("g", off)
#endif

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "jheretic.h"

#include "d_net.h"
#include "p_player.h"
#include "p_map.h"
#include "p_tick.h"
#include "p_terraintype.h"

// MACROS ------------------------------------------------------------------

#define LOWERSPEED          (6)
#define RAISESPEED          (6)
#define WEAPONBOTTOM        (128)
#define WEAPONTOP           (32)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

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
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_STAFFUP,                 // upstate
     0,                         // raise sound id
     S_STAFFDOWN,               // downstate
     S_STAFFREADY,              // readystate
     0,                         // readysound
     S_STAFFATK1_1,             // atkstate
     S_STAFFATK1_1,             // holdatkstate
     S_NULL                     // flashstate
    },
    // Staff lvl2
    {
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_STAFFUP2,                // upstate
     0,                         // raise sound id
     S_STAFFDOWN2,              // downstate
     S_STAFFREADY2_1,           // readystate
     SFX_STFCRK,                // readysound
     S_STAFFATK2_1,             // atkstate
     S_STAFFATK2_1,             // holdatkstate
     S_NULL                     // flashstate
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK1_1,              // atkstate
     S_BEAKATK1_1,              // holdatkstate
     S_NULL                     // flashstate
    },
    // lvl2
    {
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK2_1,              // atkstate
     S_BEAKATK2_1,              // holdatkstate
     S_NULL                     // flashstate
    }
    }
   }
  },
  {
   {
    {
    { // Gold wand
     GM_ANY,                    // gamemodebits
     {1, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {USE_GWND_AMMO_1, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_GOLDWANDUP,              // upstate
     0,                         // raise sound id
     S_GOLDWANDDOWN,            // downstate
     S_GOLDWANDREADY,           // readystate
     0,                         // readysound
     S_GOLDWANDATK1_1,          // atkstate
     S_GOLDWANDATK1_1,          // holdatkstate
     S_NULL                     // flashstate
    },
    // lvl2
    {
     GM_ANY,                    // gamemodebits
     {1, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {USE_GWND_AMMO_2, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_GOLDWANDUP,              // upstate
     0,                         // raise sound id
     S_GOLDWANDDOWN,            // downstate
     S_GOLDWANDREADY,           // readystate
     0,                         // readysound
     S_GOLDWANDATK2_1,          // atkstate
     S_GOLDWANDATK2_1,          // holdatkstate
     S_NULL                     // flashstate
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK1_1,              // atkstate
     S_BEAKATK1_1,              // holdatkstate
     S_NULL                     // flashstate
    },
    // lvl2
    {
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK2_1,              // atkstate
     S_BEAKATK2_1,              // holdatkstate
     S_NULL                     // flashstate
    }
    }
   }
  },
  {
   {
    {
    { // Crossbow
     GM_ANY,                    // gamemodebits
     {0, 1, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, USE_CBOW_AMMO_1, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_CRBOWUP,                 // upstate
     0,                         // raise sound id
     S_CRBOWDOWN,               // downstate
     S_CRBOW1,                  // readystate
     0,                         // readysound
     S_CRBOWATK1_1,             // atkstate
     S_CRBOWATK1_1,             // holdatkstate
     S_NULL                     // flashstate
    },
    // lvl2
    {
     GM_ANY,                    // gamemodebits
     {0, 1, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, USE_CBOW_AMMO_2, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_CRBOWUP,                 // upstate
     0,                         // raise sound id
     S_CRBOWDOWN,               // downstate
     S_CRBOW1,                  // readystate
     0,                         // readysound
     S_CRBOWATK2_1,             // atkstate
     S_CRBOWATK2_1,             // holdatkstate
     S_NULL                     // flashstate
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK1_1,              // atkstate
     S_BEAKATK1_1,              // holdatkstate
     S_NULL                     // flashstate
    },
    // lvl2
    {
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK2_1,              // atkstate
     S_BEAKATK2_1,              // holdatkstate
     S_NULL                     // flashstate
    }
    }
   }
  },
  {
   {
    {
    { // Blaster
     GM_ANY,                    // gamemodebits
     {0, 0, 1, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, USE_BLSR_AMMO_1, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BLASTERUP,               // upstate
     0,                         // raise sound id
     S_BLASTERDOWN,             // downstate
     S_BLASTERREADY,            // readystate
     0,                         // readysound
     S_BLASTERATK1_1,           // atkstate
     S_BLASTERATK1_3,           // holdatkstate
     S_NULL                     // flashstate
    },
    // lvl2
    {
     GM_ANY,                    // gamemodebits
     {0, 0, 1, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, USE_BLSR_AMMO_2, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BLASTERUP,               // upstate
     0,                         // raise sound id
     S_BLASTERDOWN,             // downstate
     S_BLASTERREADY,            // readystate
     0,                         // readysound
     S_BLASTERATK2_1,           // atkstate
     S_BLASTERATK2_3,           // holdatkstate
     S_NULL                     // flashstate
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK1_1,              // atkstate
     S_BEAKATK1_1,              // holdatkstate
     S_NULL                     // flashstate
    },
    // lvl2
    {
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK2_1,              // atkstate
     S_BEAKATK2_1,              // holdatkstate
     S_NULL                     // flashstate
    }
    }
   }
  },
  {
   {
    {
    { // Skull rod
     GM_NOTSHAREWARE,           // gamemodebits
     {0, 0, 0, 1, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, USE_SKRD_AMMO_1, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_HORNRODUP,               // upstate
     0,                         // raise sound id
     S_HORNRODDOWN,             // downstate
     S_HORNRODREADY,            // readystate
     0,                         // readysound
     S_HORNRODATK1_1,           // atkstate
     S_HORNRODATK1_1,           // holdatkstate
     S_NULL                     // flashstate
    },
    // lvl2
    {
     GM_NOTSHAREWARE,           // gamemodebits
     {0, 0, 0, 1, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, USE_SKRD_AMMO_2, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_HORNRODUP,               // upstate
     0,                         // raise sound id
     S_HORNRODDOWN,             // downstate
     S_HORNRODREADY,            // readystate
     0,                         // readysound
     S_HORNRODATK2_1,           // atkstate
     S_HORNRODATK2_1,           // holdatkstate
     S_NULL                     // flashstate
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK1_1,              // atkstate
     S_BEAKATK1_1,              // holdatkstate
     S_NULL                     // flashstate
    },
    // lvl2
    {
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK2_1,              // atkstate
     S_BEAKATK2_1,              // holdatkstate
     S_NULL                     // flashstate
    }
    }
   }
  },
  {
   {
    {
    { // Phoenix rod
     GM_NOTSHAREWARE,           // gamemodebits
     {0, 0, 0, 0, 1, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, USE_PHRD_AMMO_1, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     false,              // autofire when raised if fire held
     S_PHOENIXUP,               // upstate
     0,                         // raise sound id
     S_PHOENIXDOWN,             // downstate
     S_PHOENIXREADY,            // readystate
     0,                         // readysound
     S_PHOENIXATK1_1,           // atkstate
     S_PHOENIXATK1_1,           // holdatkstate
     S_NULL                     // flashstate
    },
    // lvl2
    {
     GM_NOTSHAREWARE,           // gamemodebits
     {0, 0, 0, 0, 1, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, USE_PHRD_AMMO_2, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     false,              // autofire when raised if fire held
     S_PHOENIXUP,               // upstate
     0,                         // raise sound id
     S_PHOENIXDOWN,             // downstate
     S_PHOENIXREADY,            // readystate
     0,                         // readysound
     S_PHOENIXATK2_1,           // atkstate
     S_PHOENIXATK2_2,           // holdatkstate
     S_NULL                     // flashstate
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK1_1,              // atkstate
     S_BEAKATK1_1,              // holdatkstate
     S_NULL                     // flashstate
    },
    // lvl2
    {
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK2_1,              // atkstate
     S_BEAKATK2_1,              // holdatkstate
     S_NULL                     // flashstate
    }
    }
   }
  },
  {
   {
    {
    { // Mace
     GM_NOTSHAREWARE,           // gamemodebits
     {0, 0, 0, 0, 0, 1}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, USE_MACE_AMMO_1}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_MACEUP,                  // upstate
     0,                         // raise sound id
     S_MACEDOWN,                // downstate
     S_MACEREADY,               // readystate
     0,                         // readysound
     S_MACEATK1_1,              // atkstate
     S_MACEATK1_2,              // holdatkstate
     S_NULL                     // flashstate
    },
    // lvl2
    {
     GM_NOTSHAREWARE,           // gamemodebits
     {0, 0, 0, 0, 0, 1}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, USE_MACE_AMMO_2}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_MACEUP,                  // upstate
     0,                         // raise sound id
     S_MACEDOWN,                // downstate
     S_MACEREADY,               // readystate
     0,                         // readysound
     S_MACEATK2_1,              // atkstate
     S_MACEATK2_1,              // holdatkstate
     S_NULL                     // flashstate
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK1_1,              // atkstate
     S_BEAKATK1_1,              // holdatkstate
     S_NULL                     // flashstate
    },
    // lvl2
    {
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK2_1,              // atkstate
     S_BEAKATK2_1,              // holdatkstate
     S_NULL                     // flashstate
    }
    }
   }
  },
  {
   {
    {
    { // Gauntlets
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_GAUNTLETUP,              // upstate
     SFX_GNTACT,                // raise sound id
     S_GAUNTLETDOWN,            // downstate
     S_GAUNTLETREADY,           // readystate
     0,                         // readysound
     S_GAUNTLETATK1_1,          // atkstate
     S_GAUNTLETATK1_3,          // holdatkstate
     S_NULL                     // flashstate
    },
    // lvl2
    {
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_GAUNTLETUP2,             // upstate
     SFX_GNTACT,                // raise sound id
     S_GAUNTLETDOWN2,           // downstate
     S_GAUNTLETREADY2_1,        // readystate
     0,                         // readysound
     S_GAUNTLETATK2_1,          // atkstate
     S_GAUNTLETATK2_3,          // holdatkstate
     S_NULL                     // flashstate
    }
    }
   },
   {
    {
    { // Beak
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK1_1,              // atkstate
     S_BEAKATK1_1,              // holdatkstate
     S_NULL                     // flashstate
    },
    // lvl2
    {
     GM_ANY,                    // gamemodebits
     {0, 0, 0, 0, 0, 0}, // type:  AT_CRYSTAL | AT_ARROW | etc...
     {0, 0, 0, 0, 0, 0}, // pershot: AT_CRYSTAL | AT_ARROW | etc...
     true,               // autofire when raised if fire held
     S_BEAKUP,                  // upstate
     0,                         // raise sound id
     S_BEAKDOWN,                // downstate
     S_BEAKREADY,               // readystate
     0,                         // readysound
     S_BEAKATK2_1,              // atkstate
     S_BEAKATK2_1,              // holdatkstate
     S_NULL                     // flashstate
    }
    }
   }
  }
};

float bulletSlope;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void R_GetWeaponBob(int player, float* x, float* y)
{
    if(x)
    {
        *x = 1 + (PLRPROFILE.psprite.bob * players[player].bob) *
            FIX2FLT(finecosine[(128 * mapTime) & FINEMASK]);
    }

    if(y)
    {
        *y = 32 + (PLRPROFILE.psprite.bob * players[player].bob) *
            FIX2FLT(finesine[(128 * mapTime) & FINEMASK & (FINEANGLES / 2 - 1)]);
    }
}

/**
 *Initialize weapon info, maxammo and clipammo.
 */
void P_InitWeaponInfo(void)
{
#define WPINF "Weapon Info|"

    int                 pclass = PCLASS_PLAYER;
    int                 i;
    char                buf[80];

    for(i = 0; i < NUM_WEAPON_TYPES; ++i)
    {
        // Level 1 (don't use a sublevel for level 1)
        sprintf(buf, WPINF "%i|Static", i);
        weaponInfo[i][pclass].mode[0].staticSwitch = GetDefInt(buf, 0);

        // Level 2
        sprintf(buf, WPINF "%i|2|Static", i);
        weaponInfo[i][pclass].mode[1].staticSwitch = GetDefInt(buf, 0);
    }

#undef WPINF
}

void P_SetPsprite(player_t *player, int position, statenum_t stnum)
{
    pspdef_t           *psp;
    state_t            *state;

    psp = &player->pSprites[position];
    do
    {
        if(!stnum)
        {   // Object removed itself.
            psp->state = NULL;
            break;
        }

        state = &STATES[stnum];
        psp->state = state;
        psp->tics = state->tics; // Could be 0.
        if(state->misc[0])
        {   // Set coordinates.
            psp->pos[VX] = (float) state->misc[0];
            psp->pos[VY] = (float) state->misc[1];
        }

        if(state->action)
        {   // Call action routine.
            state->action(player, psp);
            if(!psp->state)
            {
                break;
            }
        }
        stnum = psp->state->nextState;
    } while(!psp->tics); // An initial state of 0 could cycle through.
}

void P_ActivateMorphWeapon(player_t *player)
{
    player->pendingWeapon = WT_NOCHANGE;
    player->readyWeapon = WT_FIRST;
    player->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;
    player->pSprites[PS_WEAPON].pos[VY] = WEAPONTOP;
    P_SetPsprite(player, PS_WEAPON, S_BEAKREADY);
    NetSv_PSpriteChange(player - players, S_BEAKREADY);
}

void P_PostMorphWeapon(player_t *player, weapontype_t weapon)
{
    player->pendingWeapon = WT_NOCHANGE;
    player->readyWeapon = weapon;
    player->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;
    player->pSprites[PS_WEAPON].pos[VY] = WEAPONBOTTOM;
    P_SetPsprite(player, PS_WEAPON, weaponInfo[weapon][player->pClass].mode[0].upState);
}

/**
 * Starts bringing the pending weapon up from the bottom of the screen.
 */
void P_BringUpWeapon(player_t* player)
{
    weaponmodeinfo_t*   wminfo;

    wminfo = WEAPON_INFO(player->pendingWeapon, player->pClass,
                         (player->powers[PT_WEAPONLEVEL2]? 1:0));

    if(player->pendingWeapon == WT_NOCHANGE)
        player->pendingWeapon = player->readyWeapon;

    if(wminfo->raiseSound)
        S_StartSoundEx(wminfo->raiseSound, player->plr->mo);

    player->pendingWeapon = WT_NOCHANGE;
    player->pSprites[PS_WEAPON].pos[VY] = WEAPONBOTTOM;

    P_SetPsprite(player, PS_WEAPON, wminfo->upState);
}

void P_FireWeapon(player_t *player)
{
    statenum_t          attackState;
    int                 lvl = (player->powers[PT_WEAPONLEVEL2]? 1 : 0);

    if(!P_CheckAmmo(player))
        return;

    P_MobjChangeState(player->plr->mo, PCLASS_INFO(player->pClass)->attackState);

    if(player->refire)
        attackState = weaponInfo[player->readyWeapon][player->pClass].mode[lvl].holdAttackState;
    else
        attackState = weaponInfo[player->readyWeapon][player->pClass].mode[lvl].attackState;

    NetSv_PSpriteChange(player - players, attackState);
    P_SetPsprite(player, PS_WEAPON, attackState);

    P_NoiseAlert(player->plr->mo, player->plr->mo);
    if(player->readyWeapon == WT_EIGHTH && !player->refire)
    {
        // Play the sound for the initial gauntlet attack
        S_StartSoundEx(SFX_GNTUSE, player->plr->mo);
    }

    player->update |= PSF_AMMO;

    // Psprite state.
    player->plr->pSprites[0].state = DDPSP_FIRE;
}

/**
 * The player died, so put the weapon away.
 */
void P_DropWeapon(player_t *player)
{
    int                 level;

    if(player->powers[PT_WEAPONLEVEL2])
        level = 1;
    else
        level = 0;

    P_SetPsprite(player, PS_WEAPON,
                 weaponInfo[player->readyWeapon][player->pClass].mode[level].downState);
}

/**
 * The player can fire the weapon or change to another weapon at this time.
 */
void C_DECL A_WeaponReady(player_t* player, pspdef_t* psp)
{
    weaponmodeinfo_t*   wminfo;
    ddpsprite_t*        ddpsp;

    // Change player from attack state
    if(player->plr->mo->state == &STATES[S_PLAY_ATK1] ||
       player->plr->mo->state == &STATES[S_PLAY_ATK2])
    {
        P_MobjChangeState(player->plr->mo, S_PLAY);
    }

    if(player->readyWeapon != WT_NOCHANGE)
    {
        wminfo = WEAPON_INFO(player->readyWeapon, player->pClass, (player->powers[PT_WEAPONLEVEL2]?1:0));

        // A weaponready sound?
        if(psp->state == &STATES[wminfo->readyState] && wminfo->readySound)
#if __JHERETIC__
            if(P_Random() < 128)
#endif
            S_StartSoundEx(wminfo->readySound, player->plr->mo);

        // Check for change if player is dead, put the weapon away.
        if(player->pendingWeapon != WT_NOCHANGE || !player->health)
        {   //  (pending weapon should allready be validated)
            P_SetPsprite(player, PS_WEAPON, wminfo->downState);
            return;
        }
    }

    // Check for autofire.
    if(player->brain.attack)
    {
        wminfo = WEAPON_INFO(player->readyWeapon, player->pClass, 0);

        if(!player->attackDown || wminfo->autoFire)
        {
            player->attackDown = true;
            P_FireWeapon(player);
            return;
        }
    }
    else
        player->attackDown = false;

    ddpsp = player->plr->pSprites;

    if(!player->morphTics)
    {
        // Bob the weapon based on movement speed.
        R_GetWeaponBob(player - players, &psp->pos[0], &psp->pos[1]);

        ddpsp->offset[0] = ddpsp->offset[1] = 0;
    }

    // Psprite state.
    ddpsp->state = DDPSP_BOBBING;
}

void P_UpdateBeak(player_t *player, pspdef_t *psp)
{
    psp->pos[VY] = WEAPONTOP + FIX2FLT(player->chickenPeck << (FRACBITS - 1));
}

void C_DECL A_BeakReady(player_t *player, pspdef_t *psp)
{
    if(player->brain.attack)
    {   // Chicken beak attack.
        player->attackDown = true;
        P_MobjChangeState(player->plr->mo, S_CHICPLAY_ATK1);
        if(player->powers[PT_WEAPONLEVEL2])
        {
            P_SetPsprite(player, PS_WEAPON, S_BEAKATK2_1);
            NetSv_PSpriteChange(player - players, S_BEAKATK2_1);
        }
        else
        {
            P_SetPsprite(player, PS_WEAPON, S_BEAKATK1_1);
            NetSv_PSpriteChange(player - players, S_BEAKATK1_1);
        }
        P_NoiseAlert(player->plr->mo, player->plr->mo);
    }
    else
    {
        if(player->plr->mo->state == &STATES[S_CHICPLAY_ATK1])
        {   // Take out of attack state.
            P_MobjChangeState(player->plr->mo, S_CHICPLAY);
        }
        player->attackDown = false;
    }
}

/**
 * The player can re fire the weapon without lowering it entirely.
 */
void C_DECL A_ReFire(player_t *player, pspdef_t *psp)
{
    if((player->brain.attack) &&
       player->pendingWeapon == WT_NOCHANGE && player->health)
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

/**
 * Lowers current weapon, and changes weapon at bottom.
 */
void C_DECL A_Lower(player_t *player, pspdef_t *psp)
{
    if(player->morphTics)
        psp->pos[VY] = WEAPONBOTTOM;
    else
        psp->pos[VY] += LOWERSPEED;

    // Psprite state.
    player->plr->pSprites[0].state = DDPSP_DOWN;

    // Should we disable the lowering?
    if(!PLRPROFILE.psprite.bobLower ||
      ((player->powers[PT_WEAPONLEVEL2] &&
        weaponInfo[player->readyWeapon][player->pClass].mode[1].staticSwitch) ||
       weaponInfo[player->readyWeapon][player->pClass].mode[0].staticSwitch))
    {
        DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 0);
    }

    // Is already down.
    if(psp->pos[VY] < WEAPONBOTTOM)
        return;

    // Player is dead.
    if(player->pState == PST_DEAD)
    {
        psp->pos[VY] = WEAPONBOTTOM;

        // Don't bring weapon back up.
        return;
    }

    // The old weapon has been lowered off the screen, so change the weapon
    // and start raising it.
    if(!player->health)
    {   // Player is dead, so keep the weapon off screen.
        P_SetPsprite(player, PS_WEAPON, S_NULL);
        return;
    }

    player->readyWeapon = player->pendingWeapon;

    // Should we suddenly lower the weapon?
    if(PLRPROFILE.psprite.bobLower &&
      ((player->powers[PT_WEAPONLEVEL2] &&
        !weaponInfo[player->readyWeapon][player->pClass].mode[1].staticSwitch) ||
       !weaponInfo[player->readyWeapon][player->pClass].mode[0].staticSwitch))
    {
        DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 1000);
    }

    P_BringUpWeapon(player);
}

void C_DECL A_BeakRaise(player_t* player, pspdef_t* psp)
{
    psp->pos[VY] = WEAPONTOP;
    P_SetPsprite(player, PS_WEAPON,
                 weaponInfo[player->readyWeapon][player->pClass].mode[0].readyState);
}

void C_DECL A_Raise(player_t* player, pspdef_t* psp)
{
    statenum_t          newstate;

    // Psprite state.
    player->plr->pSprites[0].state = DDPSP_UP;

    // Should we disable the lowering?
    if(!PLRPROFILE.psprite.bobLower ||
      ((player->powers[PT_WEAPONLEVEL2] &&
        weaponInfo[player->readyWeapon][player->pClass].mode[1].staticSwitch) ||
       weaponInfo[player->readyWeapon][player->pClass].mode[0].staticSwitch))
    {
        DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 0);
    }

    psp->pos[VY] -= RAISESPEED;

    if(psp->pos[VY] > WEAPONTOP)
        return;

    // Enable the pspr Y offset once again.
    DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 1000);

    psp->pos[VY] = WEAPONTOP;

    // The weapon has been raised all the way,
    //  so change to the ready state.
    if(player->powers[PT_WEAPONLEVEL2])
        newstate = weaponInfo[player->readyWeapon][player->pClass].mode[1].readyState;
    else
        newstate = weaponInfo[player->readyWeapon][player->pClass].mode[0].readyState;

    P_SetPsprite(player, PS_WEAPON, newstate);
}

/**
 * Sets a slope so a near miss is at aproximately the height of the
 * intended target.
 */
void P_BulletSlope(mobj_t* mo)
{
    angle_t             an = mo->angle;

    if(PLRPROFILE.ctrl.useAutoAim)
    {
        // See which target is to be aimed at.
        bulletSlope = P_AimLineAttack(mo, an, 16 * 64);
        if(!lineTarget)
        {
            // No target yet, look closer.
            an += 1 << 26;
            bulletSlope = P_AimLineAttack(mo, an, 16 * 64);
            if(!lineTarget)
            {
                an -= 2 << 26;
                bulletSlope = P_AimLineAttack(mo, an, 16 * 64);
            }
        }

        if(lineTarget)
        {   // Found a target, we're done.
            return;
        }
    }

    // Fall back to manual aiming by lookdir.
    bulletSlope = tan(LOOKDIR2RAD(mo->dPlayer->lookDir)) / 1.2;
}

void C_DECL A_BeakAttackPL1(player_t* player, pspdef_t* psp)
{
    angle_t             angle;
    int                 damage;
    float               slope;

    P_ShotAmmo(player);
    damage = 1 + (P_Random() & 3);
    angle = player->plr->mo->angle;
    slope = P_AimLineAttack(player->plr->mo, angle, MELEERANGE);

    puffType = MT_BEAKPUFF;

    P_LineAttack(player->plr->mo, angle, MELEERANGE, slope, damage);
    if(lineTarget)
    {
        player->plr->mo->angle =
            R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                            lineTarget->pos[VX], lineTarget->pos[VY]);
    }

    S_StartSoundEx(SFX_CHICPK1 + (P_Random() % 3), player->plr->mo);
    player->chickenPeck = 12;
    psp->tics -= P_Random() & 7;
}

void C_DECL A_BeakAttackPL2(player_t *player, pspdef_t *psp)
{
    angle_t             angle;
    int                 damage;
    float               slope;

    P_ShotAmmo(player);
    damage = HITDICE(4);
    angle = player->plr->mo->angle;
    slope = P_AimLineAttack(player->plr->mo, angle, MELEERANGE);

    puffType = MT_BEAKPUFF;

    P_LineAttack(player->plr->mo, angle, MELEERANGE, slope, damage);
    if(lineTarget)
    {
        player->plr->mo->angle =
            R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                            lineTarget->pos[VX], lineTarget->pos[VY]);
    }

    S_StartSoundEx(SFX_CHICPK1 + (P_Random() % 3), player->plr->mo);
    player->chickenPeck = 12;
    psp->tics -= P_Random() & 3;
}

void C_DECL A_StaffAttackPL1(player_t *player, pspdef_t *psp)
{
    angle_t             angle;
    int                 damage;
    float               slope;

    P_ShotAmmo(player);
    damage = 5 + (P_Random() & 15);
    angle = player->plr->mo->angle;
    angle += (P_Random() - P_Random()) << 18;
    slope = P_AimLineAttack(player->plr->mo, angle, MELEERANGE);

    puffType = MT_STAFFPUFF;

    P_LineAttack(player->plr->mo, angle, MELEERANGE, slope, damage);
    if(lineTarget)
    {
        // Turn to face target.
        player->plr->mo->angle =
            R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                            lineTarget->pos[VX], lineTarget->pos[VY]);
    }
}

void C_DECL A_StaffAttackPL2(player_t *player, pspdef_t *psp)
{
    angle_t             angle;
    int                 damage;
    float               slope;

    P_ShotAmmo(player);
    damage = 18 + (P_Random() & 63);
    angle = player->plr->mo->angle;
    angle += (P_Random() - P_Random()) << 18;

    slope = P_AimLineAttack(player->plr->mo, angle, MELEERANGE);

    puffType = MT_STAFFPUFF2;

    P_LineAttack(player->plr->mo, angle, MELEERANGE, slope, damage);
    if(lineTarget)
    {
        // Turn to face target.
        player->plr->mo->angle =
            R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                            lineTarget->pos[VX], lineTarget->pos[VY]);
    }
}

void C_DECL A_FireBlasterPL1(player_t *player, pspdef_t *psp)
{
    mobj_t             *mo;
    angle_t             angle;
    int                 damage;

    mo = player->plr->mo;
    S_StartSoundEx(SFX_GLDHIT, mo);
    P_ShotAmmo(player);
    P_BulletSlope(mo);

    damage = HITDICE(4);
    angle = mo->angle;
    if(player->refire)
    {
        angle += (P_Random() - P_Random()) << 18;
    }

    puffType = MT_BLASTERPUFF1;

    P_LineAttack(mo, angle, MISSILERANGE, bulletSlope, damage);
    S_StartSoundEx(SFX_BLSSHT, mo);
}

void C_DECL A_FireBlasterPL2(player_t* player, pspdef_t* psp)
{

    P_ShotAmmo(player);
    S_StartSoundEx(SFX_BLSSHT, player->plr->mo);
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_BLASTERFX1, player->plr->mo, NULL);
}

void C_DECL A_FireGoldWandPL1(player_t *player, pspdef_t *psp)
{
    mobj_t             *mo;
    angle_t             angle;
    int                 damage;

    mo = player->plr->mo;
    P_ShotAmmo(player);
    S_StartSoundEx(SFX_GLDHIT, player->plr->mo);
    if(IS_CLIENT)
        return;

    P_BulletSlope(mo);
    damage = 7 + (P_Random() & 7);
    angle = mo->angle;
    if(player->refire)
    {
        angle += (P_Random() - P_Random()) << 18;
    }
    puffType = MT_GOLDWANDPUFF1;
    P_LineAttack(mo, angle, MISSILERANGE, bulletSlope, damage);
}

void C_DECL A_FireGoldWandPL2(player_t *player, pspdef_t *psp)
{
    int                 i;
    mobj_t             *mo;
    angle_t             angle;
    int                 damage;
    float               momZ;

    mo = player->plr->mo;
    P_ShotAmmo(player);
    S_StartSoundEx(SFX_GLDHIT, player->plr->mo);
    if(IS_CLIENT)
        return;

    puffType = MT_GOLDWANDPUFF2;
    P_BulletSlope(mo);
    momZ = MOBJINFO[MT_GOLDWANDFX2].speed * bulletSlope;

    P_SpawnMissileAngle(MT_GOLDWANDFX2, mo, mo->angle - (ANG45 / 8), momZ);
    P_SpawnMissileAngle(MT_GOLDWANDFX2, mo, mo->angle + (ANG45 / 8), momZ);
    angle = mo->angle - (ANG45 / 8);

    for(i = 0; i < 5; ++i)
    {
        damage = 1 + (P_Random() & 7);
        P_LineAttack(mo, angle, MISSILERANGE, bulletSlope, damage);
        angle += ((ANG45 / 8) * 2) / 4;
    }
}

void C_DECL A_FireMacePL1B(player_t *player, pspdef_t *psp)
{
    mobj_t             *pmo, *ball;
    uint                an;

    if(!P_CheckAmmo(player))
        return;

    P_ShotAmmo(player);
    if(IS_CLIENT)
        return;

    pmo = player->plr->mo;
    ball = P_SpawnMobj3f(MT_MACEFX2, pmo->pos[VX], pmo->pos[VY],
                         pmo->pos[VZ] - pmo->floorClip + 28,
                         pmo->angle);
    ball->mom[MZ] =
        2 + FIX2FLT(((int) player->plr->lookDir) << (FRACBITS - 5));
    ball->target = pmo;
    ball->pos[VZ] += FIX2FLT(((int) player->plr->lookDir) << (FRACBITS - 4));

    an = ball->angle >> ANGLETOFINESHIFT;
    ball->mom[MX] = (pmo->mom[MX] / 2) +
        ball->info->speed * FIX2FLT(finecosine[an]);
    ball->mom[MY] = (pmo->mom[MY] / 2) +
        ball->info->speed * FIX2FLT(finesine[an]);

    P_CheckMissileSpawn(ball);
    S_StartSound(SFX_LOBSHT, ball);
}

void C_DECL A_FireMacePL1(player_t *player, pspdef_t *psp)
{
    mobj_t             *ball;

    if(P_Random() < 28)
    {
        A_FireMacePL1B(player, psp);
        return;
    }
    if(!P_CheckAmmo(player))
        return;

    P_ShotAmmo(player);
    psp->pos[VX] = ((P_Random() & 3) - 2);
    psp->pos[VY] = WEAPONTOP + (P_Random() & 3);
    if(IS_CLIENT)
        return;

    ball =
        P_SpawnMissileAngle(MT_MACEFX1, player->plr->mo,
                   player->plr->mo->angle + (((P_Random() & 7) - 4) << 24),
                   -12345);
    if(ball)
    {
        ball->special1 = 16; // Tics till dropoff.
    }
}

void C_DECL A_MacePL1Check(mobj_t *ball)
{
    uint                an;

    if(ball->special1 == 0)
    {
        return;
    }

    ball->special1 -= 4;
    if(ball->special1 > 0)
    {
        return;
    }

    ball->special1 = 0;
    ball->flags2 |= MF2_LOGRAV;
    an = ball->angle >> ANGLETOFINESHIFT;
    ball->mom[MX] = 7 * FIX2FLT(finecosine[an]);
    ball->mom[MY] = 7 * FIX2FLT(finesine[an]);
    ball->mom[MZ] /= 2;
}

void C_DECL A_MaceBallImpact(mobj_t *ball)
{
    if(ball->pos[VZ] <= ball->floorZ && P_HitFloor(ball))
    {   // Landed in some sort of liquid.
        P_MobjRemove(ball, true);
        return;
    }

    if(ball->special3 != MAGIC_JUNK && ball->pos[VZ] <= ball->floorZ &&
       ball->mom[MZ] != 0)
    {   // Bounce.
        ball->special3 = MAGIC_JUNK;
        ball->mom[MZ] = FIX2FLT(FLT2FIX(ball->mom[MZ] * 192) >> 8);
        ball->flags2 &= ~MF2_FLOORBOUNCE;
        P_MobjChangeState(ball, P_GetState(ball->type, SN_SPAWN));
        S_StartSound(SFX_BOUNCE, ball);
    }
    else
    {   // Explode.
        ball->flags |= MF_NOGRAVITY;
        ball->flags2 &= ~MF2_LOGRAV;
        S_StartSound(SFX_LOBHIT, ball);
    }
}

void C_DECL A_MaceBallImpact2(mobj_t *ball)
{
    mobj_t             *tiny;
    uint                an;

    if(ball->pos[VZ] <= ball->floorZ && P_HitFloor(ball))
    {   // Landed in some sort of liquid.
        P_MobjRemove(ball, true);
        return;
    }

    if(ball->pos[VZ] != ball->floorZ || ball->mom[MZ] < 2)
    {   // Explode
        ball->mom[MX] = ball->mom[MY] = ball->mom[MZ] = 0;
        ball->flags |= MF_NOGRAVITY;
        ball->flags2 &= ~(MF2_LOGRAV | MF2_FLOORBOUNCE);
    }
    else
    {   // Bounce
        ball->mom[MZ] = FIX2FLT(FLT2FIX(ball->mom[MZ] * 192) >> 8);
        P_MobjChangeState(ball, P_GetState(ball->type, SN_SPAWN));

        tiny = P_SpawnMobj3fv(MT_MACEFX3, ball->pos, ball->angle + ANG90);
        tiny->target = ball->target;
        an = tiny->angle >> ANGLETOFINESHIFT;
        tiny->mom[MX] = (ball->mom[MX] / 2) +
            ((ball->mom[MZ] - 1) * FIX2FLT(finecosine[an]));
        tiny->mom[MY] = (ball->mom[MY] / 2) +
            ((ball->mom[MZ] - 1) * FIX2FLT(finesine[an]));
        tiny->mom[MZ] = ball->mom[MZ];
        P_CheckMissileSpawn(tiny);

        tiny = P_SpawnMobj3fv(MT_MACEFX3, ball->pos, ball->angle - ANG90);
        tiny->target = ball->target;
        an = tiny->angle >> ANGLETOFINESHIFT;
        tiny->mom[MX] = (ball->mom[MX] / 2) +
            ((ball->mom[MZ] - 1) * FIX2FLT(finecosine[an]));
        tiny->mom[MY] = (ball->mom[MY] / 2) +
            ((ball->mom[MZ] - 1) * FIX2FLT(finesine[an]));
        tiny->mom[MZ] = ball->mom[MZ];

        P_CheckMissileSpawn(tiny);
    }
}

void C_DECL A_FireMacePL2(player_t *player, pspdef_t *psp)
{
    mobj_t             *mo;

    P_ShotAmmo(player);
    S_StartSoundEx(SFX_LOBSHT, player->plr->mo);
    if(IS_CLIENT)
        return;

    mo = P_SpawnMissile(MT_MACEFX4, player->plr->mo, NULL);
    if(mo)
    {
        mo->mom[MX] += player->plr->mo->mom[MX];
        mo->mom[MY] += player->plr->mo->mom[MY];
        mo->mom[MZ] =
            2 + FIX2FLT(((int) player->plr->lookDir) << (FRACBITS - 5));

        if(lineTarget)
            mo->tracer = lineTarget;
    }
}

void C_DECL A_DeathBallImpact(mobj_t *ball)
{
    int                 i;
    mobj_t             *target;
    angle_t             angle;
    boolean             newAngle;

    if(ball->pos[VZ] <= ball->floorZ && P_HitFloor(ball))
    {   // Landed in some sort of liquid.
        P_MobjRemove(ball, true);
        return;
    }

    if(ball->pos[VZ] <= ball->floorZ && ball->mom[MZ] != 0)
    {   // Bounce.
        newAngle = false;
        target = ball->tracer;
        angle = 0;

        if(target)
        {
            if(!(target->flags & MF_SHOOTABLE))
            {   // Target died.
                ball->tracer = NULL;
            }
            else
            {   // Seek.
                angle = R_PointToAngle2(ball->pos[VX], ball->pos[VY],
                                        target->pos[VX], target->pos[VY]);
                newAngle = true;
            }
        }
        else
        {   // Find new target.
            for(i = 0; i < 16; ++i)
            {
                P_AimLineAttack(ball, angle, 10 * 64);
                if(lineTarget && ball->target != lineTarget)
                {
                    ball->tracer = lineTarget;
                    angle = R_PointToAngle2(ball->pos[VX], ball->pos[VY],
                                            lineTarget->pos[VX], lineTarget->pos[VY]);
                    newAngle = true;
                    break;
                }

                angle += ANGLE_45 / 2;
            }
        }

        if(newAngle)
        {
            uint                an = angle >> ANGLETOFINESHIFT;

            ball->angle = angle;
            ball->mom[MX] = ball->info->speed * FIX2FLT(finecosine[an]);
            ball->mom[MY] = ball->info->speed * FIX2FLT(finesine[an]);
        }

        P_MobjChangeState(ball, P_GetState(ball->type, SN_SPAWN));
        S_StartSound(SFX_PSTOP, ball);
    }
    else
    {   // Explode.
        ball->flags |= MF_NOGRAVITY;
        ball->flags2 &= ~MF2_LOGRAV;
        S_StartSound(SFX_PHOHIT, ball);
    }
}

void C_DECL A_SpawnRippers(mobj_t *actor)
{
    int                 i;
    angle_t             angle;
    uint                an;
    mobj_t             *ripper;

    for(i = 0; i < 8; ++i)
    {
        angle = i * ANG45;

        ripper = P_SpawnMobj3fv(MT_RIPPER, actor->pos, angle);
        ripper->target = actor->target;
        an = angle >> ANGLETOFINESHIFT;
        ripper->mom[MX] = ripper->info->speed * FIX2FLT(finecosine[an]);
        ripper->mom[MY] = ripper->info->speed * FIX2FLT(finesine[an]);

        P_CheckMissileSpawn(ripper);
    }
}

void C_DECL A_FireCrossbowPL1(player_t *player, pspdef_t *psp)
{
    mobj_t             *pmo;

    pmo = player->plr->mo;
    P_ShotAmmo(player);
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_CRBOWFX1, pmo, NULL);
    P_SpawnMissileAngle(MT_CRBOWFX3, pmo, pmo->angle - (ANG45 / 10), -12345);
    P_SpawnMissileAngle(MT_CRBOWFX3, pmo, pmo->angle + (ANG45 / 10), -12345);
}

void C_DECL A_FireCrossbowPL2(player_t *player, pspdef_t *psp)
{
    mobj_t             *pmo;

    pmo = player->plr->mo;
    P_ShotAmmo(player);
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_CRBOWFX2, pmo, NULL);
    P_SpawnMissileAngle(MT_CRBOWFX2, pmo, pmo->angle - (ANG45 / 10), -12345);
    P_SpawnMissileAngle(MT_CRBOWFX2, pmo, pmo->angle + (ANG45 / 10), -12345);
    P_SpawnMissileAngle(MT_CRBOWFX3, pmo, pmo->angle - (ANG45 / 5), -12345);
    P_SpawnMissileAngle(MT_CRBOWFX3, pmo, pmo->angle + (ANG45 / 5), -12345);
}

void C_DECL A_BoltSpark(mobj_t *bolt)
{
    mobj_t             *spark;

    if(P_Random() > 50)
    {
        spark = P_SpawnMobj3fv(MT_CRBOWFX4, bolt->pos, P_Random() << 24);
        spark->pos[VX] += FIX2FLT((P_Random() - P_Random()) << 10);
        spark->pos[VY] += FIX2FLT((P_Random() - P_Random()) << 10);
    }
}

void C_DECL A_FireSkullRodPL1(player_t *player, pspdef_t *psp)
{
    mobj_t             *mo;

    if(!P_CheckAmmo(player))
        return;

    P_ShotAmmo(player);
    if(IS_CLIENT)
        return;

    mo = P_SpawnMissile(MT_HORNRODFX1, player->plr->mo, NULL);
    // Randomize the first frame
    if(mo && P_Random() > 128)
    {
        P_MobjChangeState(mo, S_HRODFX1_2);
    }
}

/**
 * The special2 field holds the player number that shot the rain missile.
 * The special1 field is used as a counter for the sound looping.
 */
void C_DECL A_FireSkullRodPL2(player_t *player, pspdef_t *psp)
{
    mobj_t*             mo;

    P_ShotAmmo(player);
    if(IS_CLIENT)
        return;

    mo = P_SpawnMissile(MT_HORNRODFX2, player->plr->mo, NULL);
    if(mo)
        mo->special3 = 140;

    // Use missileMobj instead of the return value from
    // P_SpawnMissile because we need to give info to the mobj
    // even if it exploded immediately.
    if(IS_NETGAME)
    {   // Multi-player game.
        missileMobj->special2 = P_GetPlayerNum(player);
    }
    else
    {   // Always use red missiles in single player games.
        missileMobj->special2 = 2;
    }

    if(lineTarget)
        missileMobj->tracer = lineTarget;

    S_StartSound(SFX_HRNPOW, missileMobj);
}

void C_DECL A_SkullRodPL2Seek(mobj_t *actor)
{
    P_SeekerMissile(actor, ANGLE_1 * 10, ANGLE_1 * 30);
}

void C_DECL A_AddPlayerRain(mobj_t *actor)
{
    int                 playerNum;
    player_t           *player;

    playerNum = IS_NETGAME ? actor->special2 : 0;
    if(!players[playerNum].plr->inGame)
    {   // Player not left the game.
        return;
    }

    player = &players[playerNum];
    if(player->health <= 0)
    {   // Player is dead.
        return;
    }

    if(player->rain1 && player->rain2)
    {   // Terminate an active rain.
        if(player->rain1->special3 < player->rain2->special3)
        {
            if(player->rain1->special3 > 16)
            {
                player->rain1->special3 = 16;
            }
            player->rain1 = NULL;
        }
        else
        {
            if(player->rain2->special3 > 16)
            {
                player->rain2->special3 = 16;
            }
            player->rain2 = NULL;
        }
    }

    // Add rain mobj to list.
    if(player->rain1)
    {
        player->rain2 = actor;
    }
    else
    {
        player->rain1 = actor;
    }
}

void C_DECL A_SkullRodStorm(mobj_t *actor)
{
    float               pos[3];
    mobj_t             *mo;
    int                 playerNum;
    player_t           *player;

    if(actor->special3-- == 0)
    {
        P_MobjChangeState(actor, S_NULL);
        playerNum = (IS_NETGAME ? actor->special2 : 0);

        if(!players[playerNum].plr->inGame)
        {   // Player not left the game.
            return;
        }

        player = &players[playerNum];
        if(player->health <= 0)
        {   // Player is dead.
            return;
        }

        if(player->rain1 == actor)
        {
            player->rain1 = NULL;
        }
        else if(player->rain2 == actor)
        {
            player->rain2 = NULL;
        }

        return;
    }

    if(P_Random() < 25)
    {   // Fudge rain frequency.
        return;
    }

    pos[VX] = actor->pos[VX] + ((P_Random() & 127) - 64);
    pos[VY] = actor->pos[VY] + ((P_Random() & 127) - 64);

    mo = P_SpawnMobj3f(MT_RAINPLR1 + actor->special2,
                       pos[VX], pos[VY], ONCEILINGZ, P_Random() << 24);

    mo->flags |= MF_BRIGHTSHADOW;
    mo->target = actor->target;
    mo->mom[MX] = 1; // Force collision detection.
    mo->mom[MZ] = -mo->info->speed;
    mo->special2 = actor->special2; // Transfer player number.

    P_CheckMissileSpawn(mo);
    if(!(actor->special1 & 31))
        S_StartSound(SFX_RAMRAIN, actor);

    actor->special1++;
}

void C_DECL A_RainImpact(mobj_t *actor)
{
    if(actor->pos[VZ] > actor->floorZ)
    {
        P_MobjChangeState(actor, S_RAINAIRXPLR1_1 + actor->special2);
    }
    else if(P_Random() < 40)
    {
        P_HitFloor(actor);
    }
}

void C_DECL A_HideInCeiling(mobj_t *actor)
{
    actor->pos[VZ] = actor->ceilingZ + 4;
}

void C_DECL A_FirePhoenixPL1(player_t *player, pspdef_t *psp)
{
    uint                an;
    angle_t             angle;

    P_ShotAmmo(player);
    if(IS_CLIENT)
        return;

    P_SpawnMissile(MT_PHOENIXFX1, player->plr->mo, NULL);
    angle = player->plr->mo->angle + ANG180;
    an = angle >> ANGLETOFINESHIFT;
    player->plr->mo->mom[MX] += 4 * FIX2FLT(finecosine[an]);
    player->plr->mo->mom[MY] += 4 * FIX2FLT(finesine[an]);
}

void C_DECL A_PhoenixPuff(mobj_t *actor)
{
    mobj_t             *puff;
    uint                an;

    P_SeekerMissile(actor, ANGLE_1 * 5, ANGLE_1 * 10);

    puff = P_SpawnMobj3fv(MT_PHOENIXPUFF, actor->pos, actor->angle + ANG90);
    an = puff->angle >> ANGLETOFINESHIFT;
    puff->mom[MX] = 1.3 * FIX2FLT(finecosine[an]);
    puff->mom[MY] = 1.3 * FIX2FLT(finesine[an]);
    puff->mom[MZ] = 0;

    puff = P_SpawnMobj3fv(MT_PHOENIXPUFF, actor->pos, actor->angle - ANG90);
    an = puff->angle >> ANGLETOFINESHIFT;

    puff->mom[MX] = 1.3 * FIX2FLT(finecosine[an]);
    puff->mom[MY] = 1.3 * FIX2FLT(finesine[an]);
    puff->mom[MZ] = 0;
}

void C_DECL A_InitPhoenixPL2(player_t *player, pspdef_t *psp)
{
    player->flameCount = FLAME_THROWER_TICS;
}

/**
 * Flame thrower effect.
 */
void C_DECL A_FirePhoenixPL2(player_t *player, pspdef_t *psp)
{
    mobj_t             *mo, *pmo;
    angle_t             angle;
    uint                an;
    float               pos[3], slope;

    if(IS_CLIENT)
        return;

    if(--player->flameCount == 0)
    {   // Out of flame
        P_SetPsprite(player, PS_WEAPON, S_PHOENIXATK2_4);
        NetSv_PSpriteChange(player - players, S_PHOENIXATK2_4);
        player->refire = 0;
        return;
    }

    pmo = player->plr->mo;
    angle = pmo->angle;
    memcpy(pos, pmo->pos, sizeof(pos));
    pos[VX] += FIX2FLT((P_Random() - P_Random()) << 9);
    pos[VY] += FIX2FLT((P_Random() - P_Random()) << 9);
    pos[VZ] += 26 + player->plr->lookDir / 173;
    pos[VZ] -= pmo->floorClip;

    slope = sin(LOOKDIR2RAD(player->plr->lookDir)) / 1.2;

    mo = P_SpawnMobj3fv(MT_PHOENIXFX2, pos, angle);
    mo->target = pmo;
    an = mo->angle >> ANGLETOFINESHIFT;
    mo->mom[MX] = pmo->mom[MX] + mo->info->speed * FIX2FLT(finecosine[an]);
    mo->mom[MY] = pmo->mom[MY] + mo->info->speed * FIX2FLT(finesine[an]);
    mo->mom[MZ] = mo->info->speed * slope;

    if(!player->refire || !(mapTime % 38))
    {
        S_StartSoundEx(SFX_PHOPOW, player->plr->mo);
    }

    P_CheckMissileSpawn(mo);
}

void C_DECL A_ShutdownPhoenixPL2(player_t *player, pspdef_t *psp)
{
    P_ShotAmmo(player);
}

void C_DECL A_FlameEnd(mobj_t *actor)
{
    actor->mom[MZ] += 1.5f;
}

void C_DECL A_FloatPuff(mobj_t *puff)
{
    puff->mom[MZ] += 1.8f;
}

void C_DECL A_GauntletAttack(player_t *player, pspdef_t *psp)
{
    angle_t             angle;
    int                 damage, randVal;
    float               slope, dist;

    P_ShotAmmo(player);
    psp->pos[VX] = ((P_Random() & 3) - 2);
    psp->pos[VY] = WEAPONTOP + (P_Random() & 3);
    angle = player->plr->mo->angle;
    if(player->powers[PT_WEAPONLEVEL2])
    {
        damage = HITDICE(2);
        dist = 4 * MELEERANGE;
        angle += (P_Random() - P_Random()) << 17;
        puffType = MT_GAUNTLETPUFF2;
    }
    else
    {
        damage = HITDICE(2);
        dist = MELEERANGE + 1;
        angle += (P_Random() - P_Random()) << 18;
        puffType = MT_GAUNTLETPUFF1;
    }

    slope = P_AimLineAttack(player->plr->mo, angle, dist);
    P_LineAttack(player->plr->mo, angle, dist, slope, damage);
    if(!lineTarget)
    {
        if(P_Random() > 64)
        {
            player->plr->extraLight = !player->plr->extraLight;
        }

        S_StartSoundEx(SFX_GNTFUL, player->plr->mo);
        return;
    }

    randVal = P_Random();
    if(randVal < 64)
    {
        player->plr->extraLight = 0;
    }
    else if(randVal < 160)
    {
        player->plr->extraLight = 1;
    }
    else
    {
        player->plr->extraLight = 2;
    }

    if(player->powers[PT_WEAPONLEVEL2])
    {
        P_GiveBody(player, damage / 2);
        S_StartSoundEx(SFX_GNTPOW, player->plr->mo);
    }
    else
    {
        S_StartSoundEx(SFX_GNTHIT, player->plr->mo);
    }

    // Turn to face target.
    angle = R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                            lineTarget->pos[VX], lineTarget->pos[VY]);
    if(angle - player->plr->mo->angle > ANG180)
    {
        if(angle - player->plr->mo->angle < -ANG90 / 20)
            player->plr->mo->angle = angle + ANG90 / 21;
        else
            player->plr->mo->angle -= ANG90 / 20;
    }
    else
    {
        if(angle - player->plr->mo->angle > ANG90 / 20)
            player->plr->mo->angle = angle - ANG90 / 21;
        else
            player->plr->mo->angle += ANG90 / 20;
    }
    player->plr->mo->flags |= MF_JUSTATTACKED;
}

void C_DECL A_Light0(player_t *player, pspdef_t *psp)
{
    player->plr->extraLight = 0;
}

void C_DECL A_Light1(player_t *player, pspdef_t *psp)
{
    player->plr->extraLight = 1;
}

void C_DECL A_Light2(player_t *player, pspdef_t *psp)
{
    player->plr->extraLight = 2;
}

/**
 * Called at start of level for each player.
 */
void P_SetupPsprites(player_t *player)
{
    int                 i;

    // Remove all psprites.
    for(i = 0; i < NUMPSPRITES; ++i)
    {
        player->pSprites[i].state = NULL;
    }

    // Spawn the ready weapon.
    player->pendingWeapon = player->readyWeapon;
    P_BringUpWeapon(player);
}

/**
 * Called every tic by player thinking routine.
 */
void P_MovePsprites(player_t *player)
{
    int                 i;
    pspdef_t           *psp;
    state_t            *state;

    psp = &player->pSprites[0];
    for(i = 0; i < NUMPSPRITES; ++i, psp++)
    {
        // A null state means not active.
        state = psp->state;
        if(state)
        {
            // Drop tic count and possibly change state.

            // A -1 tic count never changes.
            if(psp->tics != -1)
            {
                psp->tics--;
                if(!psp->tics)
                {
                    P_SetPsprite(player, i, psp->state->nextState);
                }
            }
        }
    }

    player->pSprites[PS_FLASH].pos[VX] = player->pSprites[PS_WEAPON].pos[VX];
    player->pSprites[PS_FLASH].pos[VY] = player->pSprites[PS_WEAPON].pos[VY];
}

boolean P_UseArtiFireBomb(player_t* player)
{
    uint            an;
    mobj_t*         plrmo, *mo;

    if(!player)
        return false;

    plrmo = player->plr->mo;
    an = plrmo->angle >> ANGLETOFINESHIFT;

    mo = P_SpawnMobj3f(MT_FIREBOMB,
                       plrmo->pos[VX] + 24 * FIX2FLT(finecosine[an]),
                       plrmo->pos[VY] + 24 * FIX2FLT(finesine[an]),
                       plrmo->pos[VZ] - plrmo->floorClip + 15,
                       plrmo->angle);
    if(mo)
    {
        mo->target = player->plr->mo;
    }

    return true;
}

boolean P_UseArtiTombOfPower(player_t* player)
{
    if(!player)
        return false;

    if(player->morphTics)
    {   // Attempt to undo chicken.
        if(P_UndoPlayerMorph(player) == false)
        {   // Failed.
            P_DamageMobj(player->plr->mo, NULL, NULL, 10000, false);
        }
        else
        {   // Succeeded.
            player->morphTics = 0;
            S_StartSound(SFX_WPNUP, player->plr->mo);
        }
    }
    else
    {
        if(!P_GivePower(player, PT_WEAPONLEVEL2))
        {
            return false;
        }

        if(player->readyWeapon == WT_FIRST)
        {
            P_SetPsprite(player, PS_WEAPON, S_STAFFREADY2_1);
        }
        else if(player->readyWeapon == WT_EIGHTH)
        {
            P_SetPsprite(player, PS_WEAPON, S_GAUNTLETREADY2_1);
        }
    }

    return true;
}

boolean P_UseArtiEgg(player_t* player)
{
    mobj_t*         plrmo;

    if(!player)
        return false;
    plrmo = player->plr->mo;

# if __JHEXEN__
    P_SpawnPlayerMissile(MT_EGGFX, plrmo);
    P_SPMAngle(MT_EGGFX, plrmo, plrmo->angle - (ANG45 / 6));
    P_SPMAngle(MT_EGGFX, plrmo, plrmo->angle + (ANG45 / 6));
    P_SPMAngle(MT_EGGFX, plrmo, plrmo->angle - (ANG45 / 3));
    P_SPMAngle(MT_EGGFX, plrmo, plrmo->angle + (ANG45 / 3));
# else
    P_SpawnMissile(MT_EGGFX, plrmo, NULL);
    P_SpawnMissileAngle(MT_EGGFX, plrmo, plrmo->angle - (ANG45 / 6), -12345);
    P_SpawnMissileAngle(MT_EGGFX, plrmo, plrmo->angle + (ANG45 / 6), -12345);
    P_SpawnMissileAngle(MT_EGGFX, plrmo, plrmo->angle - (ANG45 / 3), -12345);
    P_SpawnMissileAngle(MT_EGGFX, plrmo, plrmo->angle + (ANG45 / 3), -12345);
# endif

    return true;
}

boolean P_UseArtiFly(player_t* player)
{
    if(!player)
        return false;

    if(!P_GivePower(player, PT_FLIGHT))
    {
        return false;
    }

    return true;
}

boolean P_UseArtiTeleport(player_t* player)
{
    if(!player)
        return false;

    P_ArtiTele(player);
    return true;
}

boolean P_UseArtiTorch(player_t* player)
{
    if(!player)
        return false;

    return P_GivePower(player, PT_INFRARED);
}

boolean P_UseArtiHealth(player_t* player)
{
    if(!player)
        return false;

    return P_GiveBody(player, 25);
}

boolean P_UseArtiSuperHealth(player_t* player)
{
    if(!player)
        return false;

    return P_GiveBody(player, 100);
}

boolean P_UseArtiInvisibility(player_t* player)
{
    if(!player)
        return false;

    return P_GivePower(player, PT_INVISIBILITY);
}

boolean P_UseArtiInvulnerability(player_t* player)
{
    if(!player)
        return false;

    return P_GivePower(player, PT_INVULNERABILITY);
}
