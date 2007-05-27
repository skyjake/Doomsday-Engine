/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 1999 Activision
 *
 * This program is covered by the HERETIC / HEXEN (LIMITED USE) source
 * code license; you can redistribute it and/or modify it under the terms
 * of the HERETIC / HEXEN source code license as published by Activision.
 *
 * THIS MATERIAL IS NOT MADE OR SUPPORTED BY ACTIVISION.
 *
 * WARRANTY INFORMATION.
 * This program is provided as is. Activision and it's affiliates make no
 * warranties of any kind, whether oral or written , express or implied,
 * including any warranty of merchantability, fitness for a particular
 * purpose or non-infringement, and no other representations or claims of
 * any kind shall be binding on or obligate Activision or it's affiliates.
 *
 * LICENSE CONDITIONS.
 * You shall not:
 *
 * 1) Exploit this Program or any of its parts commercially.
 * 2) Use this Program, or permit use of this Program, on more than one
 *    computer, computer terminal, or workstation at the same time.
 * 3) Make copies of this Program or any part thereof, or make copies of
 *    the materials accompanying this Program.
 * 4) Use the program, or permit use of this Program, in a network,
 *    multi-user arrangement or remote access arrangement, including any
 *    online use, except as otherwise explicitly provided by this Program.
 * 5) Sell, rent, lease or license any copies of this Program, without
 *    the express prior written consent of Activision.
 * 6) Remove, disable or circumvent any proprietary notices or labels
 *    contained on or within the Program.
 *
 * You should have received a copy of the HERETIC / HEXEN source code
 * license along with this program (Ravenlic.txt); if not:
 * http://www.ravensoft.com/
 */

/*
 * Weapon sprite animation, weapon objects.
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

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

static struct macespot_s{
    fixed_t x;
    fixed_t y;
} MaceSpots[MAX_MACE_SPOTS];

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
weaponinfo_t weaponinfo[NUM_WEAPON_TYPES][NUM_PLAYER_CLASSES] = {
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
     sfx_stfcrk,                // readysound
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
     sfx_gntact,                // raise sound id
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
     sfx_gntact,                // raise sound id
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

fixed_t bulletslope;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int MaceSpotCount;

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
#define WPINF "Weapon Info|"

    int     pclass = PCLASS_PLAYER;
    int     i;
    char    buf[80];

    for(i = 0; i < NUM_WEAPON_TYPES; i++)
    {
        // Level 1 (don't use a sublevel for level 1)
        sprintf(buf, WPINF "%i|Static", i);
        weaponinfo[i][pclass].mode[0].static_switch = GetDefInt(buf, 0);

        // Level 2
        sprintf(buf, WPINF "%i|2|Static", i);
        weaponinfo[i][pclass].mode[1].static_switch = GetDefInt(buf, 0);
    }
}

void P_OpenWeapons(void)
{
    MaceSpotCount = 0;
}

void P_AddMaceSpot(thing_t * mthing)
{
    // FIXME: Remove fixed limits
    if(MaceSpotCount == MAX_MACE_SPOTS)
    {
        Con_Error("Too many mace spots.");
    }
    MaceSpots[MaceSpotCount].x = mthing->x << FRACBITS;
    MaceSpots[MaceSpotCount].y = mthing->y << FRACBITS;
    MaceSpotCount++;
}

/*
 * Chooses the next spot to place the mace.
 */
void P_RepositionMace(mobj_t *mo)
{
    int     spot;
    subsector_t *ss;

    P_UnsetThingPosition(mo);
    spot = P_Random() % MaceSpotCount;
    mo->pos[VX] = MaceSpots[spot].x;
    mo->pos[VY] = MaceSpots[spot].y;
    ss = R_PointInSubsector(mo->pos[VX], mo->pos[VY]);

    mo->floorz = P_GetFloatp(ss, DMU_CEILING_HEIGHT);
    mo->pos[VZ] = FIX2FLT(mo->floorz);

    mo->ceilingz = P_GetFloatp(ss, DMU_CEILING_HEIGHT);
    P_SetThingPosition(mo);
}

/*
 * Called at level load after things are loaded.
 */
void P_CloseWeapons(void)
{
    int     spot;

    if(!MaceSpotCount)
    {                           // No maces placed
        return;
    }
    if(!deathmatch && P_Random() < 64)
    {                           // Sometimes doesn't show up if not in deathmatch
        return;
    }
    spot = P_Random() % MaceSpotCount;
    P_SpawnMobj(MaceSpots[spot].x, MaceSpots[spot].y, ONFLOORZ, MT_WMACE);
}

void P_SetPsprite(player_t *player, int position, statenum_t stnum)
{
    pspdef_t *psp;
    state_t *state;

    psp = &player->psprites[position];
    do
    {
        if(!stnum)
        {                       // Object removed itself.
            psp->state = NULL;
            break;
        }
        state = &states[stnum];
        psp->state = state;
        psp->tics = state->tics;    // could be 0
        if(state->misc[0])
        {                       // Set coordinates.
            psp->sx = state->misc[0] << FRACBITS;
            psp->sy = state->misc[1] << FRACBITS;
        }
        if(state->action)
        {                       // Call action routine.
            state->action(player, psp);
            if(!psp->state)
            {
                break;
            }
        }
        stnum = psp->state->nextstate;
    } while(!psp->tics);        // An initial state of 0 could cycle through.
}

void P_ActivateMorphWeapon(player_t *player)
{
    player->pendingweapon = WT_NOCHANGE;
    player->readyweapon = WT_FIRST;
    player->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;
    player->psprites[ps_weapon].sy = WEAPONTOP;
    P_SetPsprite(player, ps_weapon, S_BEAKREADY);
    NetSv_PSpriteChange(player - players, S_BEAKREADY);
}

void P_PostMorphWeapon(player_t *player, weapontype_t weapon)
{
    player->pendingweapon = WT_NOCHANGE;
    player->readyweapon = weapon;
    player->update |= PSF_PENDING_WEAPON | PSF_READY_WEAPON;
    player->psprites[ps_weapon].sy = WEAPONBOTTOM;
    P_SetPsprite(player, ps_weapon, weaponinfo[weapon][player->class].mode[0].upstate);
}

/*
 * Starts bringing the pending weapon up from the bottom of the screen.
 */
void P_BringUpWeapon(player_t *player)
{
    weaponmodeinfo_t *wminfo;

    wminfo = WEAPON_INFO(player->pendingweapon, player->class,
                         (player->powers[PT_WEAPONLEVEL2]? 1:0));

    if(player->pendingweapon == WT_NOCHANGE)
        player->pendingweapon = player->readyweapon;

    if(wminfo->raisesound)
        S_StartSoundEx(wminfo->raisesound, player->plr->mo);

    player->pendingweapon = WT_NOCHANGE;
    player->psprites[ps_weapon].sy = WEAPONBOTTOM;

    P_SetPsprite(player, ps_weapon, wminfo->upstate);
}

/*
 * Returns true if there is enough ammo to shoot.  If not, selects the
 * next weapon to use.
 */
boolean P_CheckAmmo(player_t *player)
{
    ammotype_t i;
    int     count;
    int     lvl;
    boolean good;

#if __JHERETIC__
    if(player->powers[PT_WEAPONLEVEL2] && !deathmatch)
        // If deathmatch always use level one ammo requirements.
        lvl = 1;
    else
#endif
        lvl = 0;

    // Check we have enough of ALL ammo types used by this weapon.
    good = true;
    for(i=0; i < NUM_AMMO_TYPES && good; ++i)
    {
        if(!weaponinfo[player->readyweapon][player->class].mode[lvl].ammotype[i])
            continue; // Weapon does not take this type of ammo.

        // Minimal amount for one shot varies.
        count = weaponinfo[player->readyweapon][player->class].mode[lvl].pershot[i];

        // Return if current ammunition sufficient.
        if(player->ammo[i] < count)
        {
            good = false;
        }
    }
    if(good)
        return true;

    // Out of ammo, pick a weapon to change to.
    P_MaybeChangeWeapon(player, WT_NOCHANGE, AT_NOAMMO, false);

    P_SetPsprite(player, ps_weapon,
                 weaponinfo[player->readyweapon][player->class].mode[lvl].downstate);
    return false;
}

void P_FireWeapon(player_t *player)
{
    statenum_t attackState;
    int lvl = (player->powers[PT_WEAPONLEVEL2]? 1 : 0);

    if(!P_CheckAmmo(player))
        return;

    P_SetMobjState(player->plr->mo, PCLASS_INFO(player->class)->attackstate);

    if(player->refire)
        attackState = weaponinfo[player->readyweapon][player->class].mode[lvl].holdatkstate;
    else
        attackState = weaponinfo[player->readyweapon][player->class].mode[lvl].atkstate;

    NetSv_PSpriteChange(player - players, attackState);
    P_SetPsprite(player, ps_weapon, attackState);

    P_NoiseAlert(player->plr->mo, player->plr->mo);
    if(player->readyweapon == WT_EIGHTH && !player->refire)
    {                           
        // Play the sound for the initial gauntlet attack
        S_StartSoundEx(sfx_gntuse, player->plr->mo);
    }

    player->update |= PSF_AMMO;

    // Psprite state.
    player->plr->psprites[0].state = DDPSP_FIRE;
}

/*
 * The player died, so put the weapon away.
 */
void P_DropWeapon(player_t *player)
{
    if(player->powers[PT_WEAPONLEVEL2])
    {
        P_SetPsprite(player, ps_weapon,
                     weaponinfo[player->readyweapon][player->class].mode[1].downstate);
    }
    else
    {
        P_SetPsprite(player, ps_weapon,
                     weaponinfo[player->readyweapon][player->class].mode[0].downstate);
    }
}

/*
 * The player can fire the weapon or change to another weapon at this time.
 */
void C_DECL A_WeaponReady(player_t *player, pspdef_t * psp)
{
    weaponmodeinfo_t *wminfo;
    ddpsprite_t *ddpsp;

    // Change player from attack state
    if(player->plr->mo->state == &states[S_PLAY_ATK1] ||
       player->plr->mo->state == &states[S_PLAY_ATK2])
    {
        P_SetMobjState(player->plr->mo, S_PLAY);
    }

    if(player->readyweapon != WT_NOCHANGE)
    {
        wminfo = WEAPON_INFO(player->readyweapon, player->class, (player->powers[PT_WEAPONLEVEL2]?1:0));

        // A weaponready sound?
        if(psp->state == &states[wminfo->readystate] && wminfo->readysound)
#if __JHERETIC__
            if(P_Random() < 128)
#endif
            S_StartSoundEx(wminfo->readysound, player->plr->mo);

        // check for change
        //  if player is dead, put the weapon away
        if(player->pendingweapon != WT_NOCHANGE || !player->health)
        {   //  (pending weapon should allready be validated)
            P_SetPsprite(player, ps_weapon, wminfo->downstate);
            return;
        }
    }

    // check for autofire
    if(player->plr->cmd.actions & BT_ATTACK)
    {
        wminfo = WEAPON_INFO(player->readyweapon, player->class, 0);

        if(!player->attackdown || wminfo->autofire)
        {
            player->attackdown = true;
            P_FireWeapon(player);
            return;
        }
    }
    else
        player->attackdown = false;

    ddpsp = player->plr->psprites;

    if(!player->morphTics)
    {
        // Bob the weapon based on movement speed.
        psp->sx = G_GetInteger(DD_PSPRITE_BOB_X);
        psp->sy = G_GetInteger(DD_PSPRITE_BOB_Y);

        ddpsp->offx = ddpsp->offy = 0;
    }

    // Psprite state.
    ddpsp->state = DDPSP_BOBBING;
}

void P_UpdateBeak(player_t *player, pspdef_t *psp)
{
    psp->sy = WEAPONTOP + (player->chickenPeck << (FRACBITS - 1));
}

void C_DECL A_BeakReady(player_t *player, pspdef_t *psp)
{
    if(player->plr->cmd.actions & BT_ATTACK)
    {                           // Chicken beak attack
        player->attackdown = true;
        P_SetMobjState(player->plr->mo, S_CHICPLAY_ATK1);
        if(player->powers[PT_WEAPONLEVEL2])
        {
            P_SetPsprite(player, ps_weapon, S_BEAKATK2_1);
            NetSv_PSpriteChange(player - players, S_BEAKATK2_1);
        }
        else
        {
            P_SetPsprite(player, ps_weapon, S_BEAKATK1_1);
            NetSv_PSpriteChange(player - players, S_BEAKATK1_1);
        }
        P_NoiseAlert(player->plr->mo, player->plr->mo);
    }
    else
    {
        if(player->plr->mo->state == &states[S_CHICPLAY_ATK1])
        {                       // Take out of attack state
            P_SetMobjState(player->plr->mo, S_CHICPLAY);
        }
        player->attackdown = false;
    }
}

/**
 * The player can re fire the weapon without lowering it entirely.
 */
void C_DECL A_ReFire(player_t *player, pspdef_t *psp)
{
    if((player->plr->cmd.actions & BT_ATTACK) &&
       player->pendingweapon == WT_NOCHANGE &&
       player->health)
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
        psp->sy = WEAPONBOTTOM;
    else
        psp->sy += LOWERSPEED;

    // Psprite state.
    player->plr->psprites[0].state = DDPSP_DOWN;

    // Should we disable the lowering?
    if(!cfg.bobWeaponLower ||
      ((player->powers[PT_WEAPONLEVEL2] &&
        weaponinfo[player->readyweapon][player->class].mode[1].static_switch) ||
       weaponinfo[player->readyweapon][player->class].mode[0].static_switch))
    {
        DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 0);
    }

    // Is already down.
    if(psp->sy < WEAPONBOTTOM)
        return;

    // Player is dead.
    if(player->playerstate == PST_DEAD)
    {
        psp->sy = WEAPONBOTTOM;

        // don't bring weapon back up
        return;
    }

    // The old weapon has been lowered off the screen,
    // so change the weapon and start raising it
    if(!player->health)
    {
        // Player is dead, so keep the weapon off screen
        P_SetPsprite(player, ps_weapon, S_NULL);
        return;
    }

    player->readyweapon = player->pendingweapon;

    // Should we suddenly lower the weapon?
    if(cfg.bobWeaponLower &&
      ((player->powers[PT_WEAPONLEVEL2] &&
        !weaponinfo[player->readyweapon][player->class].mode[1].static_switch) ||
       !weaponinfo[player->readyweapon][player->class].mode[0].static_switch))
    {
        DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 1000);
    }

    P_BringUpWeapon(player);
}

void C_DECL A_BeakRaise(player_t *player, pspdef_t * psp)
{
    psp->sy = WEAPONTOP;
    P_SetPsprite(player, ps_weapon,
                 weaponinfo[player->readyweapon][player->class].mode[0].readystate);
}

void C_DECL A_Raise(player_t *player, pspdef_t * psp)
{
    statenum_t newstate;

    // Psprite state.
    player->plr->psprites[0].state = DDPSP_UP;

    // Should we disable the lowering?
    if(!cfg.bobWeaponLower ||
      ((player->powers[PT_WEAPONLEVEL2] &&
        weaponinfo[player->readyweapon][player->class].mode[1].static_switch) ||
       weaponinfo[player->readyweapon][player->class].mode[0].static_switch))
    {
        DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 0);
    }

    psp->sy -= RAISESPEED;

    if(psp->sy > WEAPONTOP)
        return;

    // Enable the pspr Y offset once again.
    DD_SetInteger(DD_WEAPON_OFFSET_SCALE_Y, 1000);

    psp->sy = WEAPONTOP;

    // The weapon has been raised all the way,
    //  so change to the ready state.
    if(player->powers[PT_WEAPONLEVEL2])
        newstate = weaponinfo[player->readyweapon][player->class].mode[1].readystate;
    else
        newstate = weaponinfo[player->readyweapon][player->class].mode[0].readystate;

    P_SetPsprite(player, ps_weapon, newstate);
}

/*
 * Sets a slope so a near miss is at aproximately the height of the
 * intended target.
 */
void P_BulletSlope(mobj_t *mo)
{
    angle_t an = mo->angle;

    if(!cfg.noAutoAim) // Autoaiming enabled.
    {
        // See which target is to be aimed at.
        bulletslope = P_AimLineAttack(mo, an, 16 * 64 * FRACUNIT);
        if(!linetarget)
        {
            // No target yet, look closer.
            an += 1 << 26;
            bulletslope = P_AimLineAttack(mo, an, 16 * 64 * FRACUNIT);
            if(!linetarget)
            {
                an -= 2 << 26;
                bulletslope = P_AimLineAttack(mo, an, 16 * 64 * FRACUNIT);
            }
        }
        if(linetarget)
        {
            // Found a target, we're done.
            return;
        }
    }

    // Fall back to manual aiming by lookdir.
    bulletslope =
        FRACUNIT * (tan(LOOKDIR2RAD(mo->dplayer->lookdir)) / 1.2);
}

void C_DECL A_BeakAttackPL1(player_t *player, pspdef_t * psp)
{
    angle_t angle;
    int     damage;
    int     slope;

    damage = 1 + (P_Random() & 3);
    angle = player->plr->mo->angle;
    slope = P_AimLineAttack(player->plr->mo, angle, MELEERANGE);
    PuffType = MT_BEAKPUFF;
    P_LineAttack(player->plr->mo, angle, MELEERANGE, slope, damage);
    if(linetarget)
    {
        player->plr->mo->angle =
            R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                            linetarget->pos[VX], linetarget->pos[VY]);
    }
    S_StartSoundEx(sfx_chicpk1 + (P_Random() % 3), player->plr->mo);
    player->chickenPeck = 12;
    psp->tics -= P_Random() & 7;
}

void C_DECL A_BeakAttackPL2(player_t *player, pspdef_t * psp)
{
    angle_t angle;
    int     damage;
    int     slope;

    damage = HITDICE(4);
    angle = player->plr->mo->angle;
    slope = P_AimLineAttack(player->plr->mo, angle, MELEERANGE);
    PuffType = MT_BEAKPUFF;
    P_LineAttack(player->plr->mo, angle, MELEERANGE, slope, damage);
    if(linetarget)
    {
        player->plr->mo->angle =
            R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                            linetarget->pos[VX], linetarget->pos[VY]);
    }
    S_StartSoundEx(sfx_chicpk1 + (P_Random() % 3), player->plr->mo);
    player->chickenPeck = 12;
    psp->tics -= P_Random() & 3;
}

void C_DECL A_StaffAttackPL1(player_t *player, pspdef_t * psp)
{
    angle_t angle;
    int     damage;
    int     slope;

    damage = 5 + (P_Random() & 15);
    angle = player->plr->mo->angle;
    angle += (P_Random() - P_Random()) << 18;
    slope = P_AimLineAttack(player->plr->mo, angle, MELEERANGE);
    PuffType = MT_STAFFPUFF;
    P_LineAttack(player->plr->mo, angle, MELEERANGE, slope, damage);
    if(linetarget)
    {
        // turn to face target
        player->plr->mo->angle =
            R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                            linetarget->pos[VX], linetarget->pos[VY]);
    }
}

void C_DECL A_StaffAttackPL2(player_t *player, pspdef_t * psp)
{
    angle_t angle;
    int     damage;
    int     slope;

    // P_inter.c:P_DamageMobj() handles target momentums
    damage = 18 + (P_Random() & 63);
    angle = player->plr->mo->angle;
    angle += (P_Random() - P_Random()) << 18;
    slope = P_AimLineAttack(player->plr->mo, angle, MELEERANGE);
    PuffType = MT_STAFFPUFF2;
    P_LineAttack(player->plr->mo, angle, MELEERANGE, slope, damage);
    if(linetarget)
    {
        // turn to face target
        player->plr->mo->angle =
            R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                            linetarget->pos[VX], linetarget->pos[VY]);
    }
}

void C_DECL A_FireBlasterPL1(player_t *player, pspdef_t * psp)
{
    mobj_t *mo;
    angle_t angle;
    int     damage;

    mo = player->plr->mo;
    S_StartSoundEx(sfx_gldhit, mo);
    P_ShotAmmo(player);
    P_BulletSlope(mo);
    damage = HITDICE(4);
    angle = mo->angle;
    if(player->refire)
    {
        angle += (P_Random() - P_Random()) << 18;
    }
    PuffType = MT_BLASTERPUFF1;
    P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage);
    S_StartSoundEx(sfx_blssht, mo);
}

void C_DECL A_FireBlasterPL2(player_t *player, pspdef_t * psp)
{
    mobj_t *mo;

    P_ShotAmmo(player);
    S_StartSoundEx(sfx_blssht, player->plr->mo);
    if(IS_CLIENT)
        return;

    mo = P_SpawnMissile(player->plr->mo, NULL, MT_BLASTERFX1);
    if(mo)
    {
        mo->thinker.function = P_BlasterMobjThinker;
    }
}

void C_DECL A_FireGoldWandPL1(player_t *player, pspdef_t * psp)
{
    mobj_t *mo;
    angle_t angle;
    int     damage;

    mo = player->plr->mo;
    P_ShotAmmo(player);
    S_StartSoundEx(sfx_gldhit, player->plr->mo);
    if(IS_CLIENT)
        return;

    P_BulletSlope(mo);
    damage = 7 + (P_Random() & 7);
    angle = mo->angle;
    if(player->refire)
    {
        angle += (P_Random() - P_Random()) << 18;
    }
    PuffType = MT_GOLDWANDPUFF1;
    P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage);
}

void C_DECL A_FireGoldWandPL2(player_t *player, pspdef_t * psp)
{
    int     i;
    mobj_t *mo;
    angle_t angle;
    int     damage;
    fixed_t mom[MZ];

    mo = player->plr->mo;
    P_ShotAmmo(player);
    S_StartSoundEx(sfx_gldhit, player->plr->mo);
    if(IS_CLIENT)
        return;

    PuffType = MT_GOLDWANDPUFF2;
    P_BulletSlope(mo);
    mom[MZ] = FixedMul(mobjinfo[MT_GOLDWANDFX2].speed, bulletslope);
    P_SpawnMissileAngle(mo, MT_GOLDWANDFX2, mo->angle - (ANG45 / 8), mom[MZ]);
    P_SpawnMissileAngle(mo, MT_GOLDWANDFX2, mo->angle + (ANG45 / 8), mom[MZ]);
    angle = mo->angle - (ANG45 / 8);
    for(i = 0; i < 5; i++)
    {
        damage = 1 + (P_Random() & 7);
        P_LineAttack(mo, angle, MISSILERANGE, bulletslope, damage);
        angle += ((ANG45 / 8) * 2) / 4;
    }
}

void C_DECL A_FireMacePL1B(player_t *player, pspdef_t * psp)
{
    mobj_t *pmo;
    mobj_t *ball;
    angle_t angle;

    if(!P_CheckAmmo(player))
        return;

    P_ShotAmmo(player);
    if(IS_CLIENT)
        return;
    pmo = player->plr->mo;
    ball =
        P_SpawnMobj(pmo->pos[VX], pmo->pos[VY],
                    pmo->pos[VZ] - FLT2FIX(pmo->floorclip + 28),
                    MT_MACEFX2);
    ball->mom[MZ] =
        2 * FRACUNIT + (((int) player->plr->lookdir) << (FRACBITS - 5));
    angle = pmo->angle;
    ball->target = pmo;
    ball->angle = angle;
    ball->pos[VZ] += ((int) player->plr->lookdir) << (FRACBITS - 4);
    angle >>= ANGLETOFINESHIFT;
    ball->mom[MX] =
        (pmo->mom[MX] >> 1) + FixedMul(ball->info->speed, finecosine[angle]);
    ball->mom[MY] =
        (pmo->mom[MY] >> 1) + FixedMul(ball->info->speed, finesine[angle]);
    P_CheckMissileSpawn(ball);
    S_StartSound(sfx_lobsht, ball);
}

void C_DECL A_FireMacePL1(player_t *player, pspdef_t * psp)
{
    mobj_t *ball;

    if(P_Random() < 28)
    {
        A_FireMacePL1B(player, psp);
        return;
    }
    if(!P_CheckAmmo(player))
        return;

    P_ShotAmmo(player);
    psp->sx = ((P_Random() & 3) - 2) * FRACUNIT;
    psp->sy = WEAPONTOP + (P_Random() & 3) * FRACUNIT;
    if(IS_CLIENT)
        return;

    ball =
        P_SpawnMissileAngle(player->plr->mo, MT_MACEFX1,
                   player->plr->mo->angle + (((P_Random() & 7) - 4) << 24),
                   -12345);
    if(ball)
    {
        ball->special1 = 16;    // tics till dropoff
    }
}

void C_DECL A_MacePL1Check(mobj_t *ball)
{
    angle_t angle;

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
    angle = ball->angle >> ANGLETOFINESHIFT;
    ball->mom[MX] = FixedMul(7 * FRACUNIT, finecosine[angle]);
    ball->mom[MY] = FixedMul(7 * FRACUNIT, finesine[angle]);
    ball->mom[MZ] -= ball->mom[MZ] >> 1;
}

void C_DECL A_MaceBallImpact(mobj_t *ball)
{
    if((ball->pos[VZ] <= FLT2FIX(ball->floorz)) && (P_HitFloor(ball) != FLOOR_SOLID))
    {                           // Landed in some sort of liquid
        P_RemoveMobj(ball);
        return;
    }
    if((ball->health != MAGIC_JUNK) && (ball->pos[VZ] <= FLT2FIX(ball->floorz)) && ball->mom[MZ])
    {                           // Bounce
        ball->health = MAGIC_JUNK;
        ball->mom[MZ] = (ball->mom[MZ] * 192) >> 8;
        ball->flags2 &= ~MF2_FLOORBOUNCE;
        P_SetMobjState(ball, ball->info->spawnstate);
        S_StartSound(sfx_bounce, ball);
    }
    else
    {                           // Explode
        ball->flags |= MF_NOGRAVITY;
        ball->flags2 &= ~MF2_LOGRAV;
        S_StartSound(sfx_lobhit, ball);
    }
}

void C_DECL A_MaceBallImpact2(mobj_t *ball)
{
    mobj_t *tiny;
    angle_t angle;

    if((ball->pos[VZ] <= FLT2FIX(ball->floorz)) && (P_HitFloor(ball) != FLOOR_SOLID))
    {                           // Landed in some sort of liquid
        P_RemoveMobj(ball);
        return;
    }
    if((ball->pos[VZ] != FLT2FIX(ball->floorz)) || (ball->mom[MZ] < 2 * FRACUNIT))
    {                           // Explode
        ball->mom[MX] = ball->mom[MY] = ball->mom[MZ] = 0;
        ball->flags |= MF_NOGRAVITY;
        ball->flags2 &= ~(MF2_LOGRAV | MF2_FLOORBOUNCE);
    }
    else
    {                           // Bounce
        ball->mom[MZ] = (ball->mom[MZ] * 192) >> 8;
        P_SetMobjState(ball, ball->info->spawnstate);

        tiny = P_SpawnMobj(ball->pos[VX], ball->pos[VY], ball->pos[VZ], MT_MACEFX3);
        angle = ball->angle + ANG90;
        tiny->target = ball->target;
        tiny->angle = angle;
        angle >>= ANGLETOFINESHIFT;
        tiny->mom[MX] =
            (ball->mom[MX] >> 1) + FixedMul(ball->mom[MZ] - FRACUNIT,
                                         finecosine[angle]);
        tiny->mom[MY] =
            (ball->mom[MY] >> 1) + FixedMul(ball->mom[MZ] - FRACUNIT,
                                         finesine[angle]);
        tiny->mom[MZ] = ball->mom[MZ];
        P_CheckMissileSpawn(tiny);

        tiny = P_SpawnMobj(ball->pos[VX], ball->pos[VY], ball->pos[VZ], MT_MACEFX3);
        angle = ball->angle - ANG90;
        tiny->target = ball->target;
        tiny->angle = angle;
        angle >>= ANGLETOFINESHIFT;
        tiny->mom[MX] =
            (ball->mom[MX] >> 1) + FixedMul(ball->mom[MZ] - FRACUNIT,
                                         finecosine[angle]);
        tiny->mom[MY] =
            (ball->mom[MY] >> 1) + FixedMul(ball->mom[MZ] - FRACUNIT,
                                         finesine[angle]);
        tiny->mom[MZ] = ball->mom[MZ];
        P_CheckMissileSpawn(tiny);
    }
}

void C_DECL A_FireMacePL2(player_t *player, pspdef_t * psp)
{
    mobj_t *mo;

    P_ShotAmmo(player);
    S_StartSoundEx(sfx_lobsht, player->plr->mo);
    if(IS_CLIENT)
        return;

    mo = P_SpawnMissile(player->plr->mo, NULL, MT_MACEFX4);
    if(mo)
    {
        mo->mom[MX] += player->plr->mo->mom[MX];
        mo->mom[MY] += player->plr->mo->mom[MY];
        mo->mom[MZ] =
            2 * FRACUNIT + (((int) player->plr->lookdir) << (FRACBITS - 5));

        if(linetarget)
            mo->tracer = linetarget;
    }
}

void C_DECL A_DeathBallImpact(mobj_t *ball)
{
    int     i;
    mobj_t *target;
    angle_t angle;
    boolean newAngle;

    if((ball->pos[VZ] <= FLT2FIX(ball->floorz)) && (P_HitFloor(ball) != FLOOR_SOLID))
    {                           // Landed in some sort of liquid
        P_RemoveMobj(ball);
        return;
    }

    if((ball->pos[VZ] <= FLT2FIX(ball->floorz)) && ball->mom[MZ])
    {   // Bounce
        newAngle = false;
        target = ball->tracer;
        angle = 0;
        if(target)
        {
            if(!(target->flags & MF_SHOOTABLE))
            {   // Target died
                ball->tracer = NULL;
            }
            else
            {   // Seek
                angle = R_PointToAngle2(ball->pos[VX], ball->pos[VY],
                                        target->pos[VX], target->pos[VY]);
                newAngle = true;
            }
        }
        else
        {   // Find new target
            for(i = 0; i < 16; i++)
            {
                P_AimLineAttack(ball, angle, 10 * 64 * FRACUNIT);
                if(linetarget && ball->target != linetarget)
                {
                    ball->tracer = linetarget;
                    angle = R_PointToAngle2(ball->pos[VX], ball->pos[VY],
                                            linetarget->pos[VX], linetarget->pos[VY]);
                    newAngle = true;
                    break;
                }
                angle += ANGLE_45 / 2;
            }
        }
        if(newAngle)
        {
            ball->angle = angle;
            angle >>= ANGLETOFINESHIFT;
            ball->mom[MX] = FixedMul(ball->info->speed, finecosine[angle]);
            ball->mom[MY] = FixedMul(ball->info->speed, finesine[angle]);
        }
        P_SetMobjState(ball, ball->info->spawnstate);
        S_StartSound(sfx_pstop, ball);
    }
    else
    {                           // Explode
        ball->flags |= MF_NOGRAVITY;
        ball->flags2 &= ~MF2_LOGRAV;
        S_StartSound(sfx_phohit, ball);
    }
}

void C_DECL A_SpawnRippers(mobj_t *actor)
{
    int     i;
    angle_t angle;
    mobj_t *ripper;

    for(i = 0; i < 8; i++)
    {
        ripper = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_RIPPER);
        angle = i * ANG45;
        ripper->target = actor->target;
        ripper->angle = angle;
        angle >>= ANGLETOFINESHIFT;
        ripper->mom[MX] = FixedMul(ripper->info->speed, finecosine[angle]);
        ripper->mom[MY] = FixedMul(ripper->info->speed, finesine[angle]);
        P_CheckMissileSpawn(ripper);
    }
}

void C_DECL A_FireCrossbowPL1(player_t *player, pspdef_t * psp)
{
    mobj_t *pmo;

    pmo = player->plr->mo;
    P_ShotAmmo(player);
    if(IS_CLIENT)
        return;
    P_SpawnMissile(pmo, NULL, MT_CRBOWFX1);
    P_SpawnMissileAngle(pmo, MT_CRBOWFX3, pmo->angle - (ANG45 / 10), -12345);
    P_SpawnMissileAngle(pmo, MT_CRBOWFX3, pmo->angle + (ANG45 / 10), -12345);
}

void C_DECL A_FireCrossbowPL2(player_t *player, pspdef_t * psp)
{
    mobj_t *pmo;

    pmo = player->plr->mo;
    P_ShotAmmo(player);
    if(IS_CLIENT)
        return;
    P_SpawnMissile(pmo, NULL, MT_CRBOWFX2);
    P_SpawnMissileAngle(pmo, MT_CRBOWFX2, pmo->angle - (ANG45 / 10), -12345);
    P_SpawnMissileAngle(pmo, MT_CRBOWFX2, pmo->angle + (ANG45 / 10), -12345);
    P_SpawnMissileAngle(pmo, MT_CRBOWFX3, pmo->angle - (ANG45 / 5), -12345);
    P_SpawnMissileAngle(pmo, MT_CRBOWFX3, pmo->angle + (ANG45 / 5), -12345);
}

void C_DECL A_BoltSpark(mobj_t *bolt)
{
    mobj_t *spark;

    if(P_Random() > 50)
    {
        spark = P_SpawnMobj(bolt->pos[VX], bolt->pos[VY], bolt->pos[VZ], MT_CRBOWFX4);
        spark->pos[VX] += (P_Random() - P_Random()) << 10;
        spark->pos[VY] += (P_Random() - P_Random()) << 10;
    }
}

void C_DECL A_FireSkullRodPL1(player_t *player, pspdef_t * psp)
{
    mobj_t *mo;

    if(!P_CheckAmmo(player))
        return;

    P_ShotAmmo(player);

    if(IS_CLIENT)
        return;

    mo = P_SpawnMissile(player->plr->mo, NULL, MT_HORNRODFX1);
    // Randomize the first frame
    if(mo && P_Random() > 128)
    {
        P_SetMobjState(mo, S_HRODFX1_2);
    }
}

/*
 * The special2 field holds the player number that shot the rain missile.
 * The special1 field is used as a counter for the sound looping.
 */
void C_DECL A_FireSkullRodPL2(player_t *player, pspdef_t * psp)
{
    P_ShotAmmo(player);

    if(IS_CLIENT)
        return;

    P_SpawnMissile(player->plr->mo, NULL, MT_HORNRODFX2);
    // Use MissileMobj instead of the return value from
    // P_SpawnMissile because we need to give info to the mobj
    // even if it exploded immediately.
    if(IS_NETGAME)
    {                           // Multi-player game
        MissileMobj->special2 = P_GetPlayerNum(player);
    }
    else
    {                           // Always use red missiles in single player games
        MissileMobj->special2 = 2;
    }

    if(linetarget)
        MissileMobj->tracer = linetarget;

    S_StartSound(sfx_hrnpow, MissileMobj);
}

void C_DECL A_SkullRodPL2Seek(mobj_t *actor)
{
    P_SeekerMissile(actor, ANGLE_1 * 10, ANGLE_1 * 30);
}

void C_DECL A_AddPlayerRain(mobj_t *actor)
{
    int     playerNum;
    player_t *player;

    playerNum = IS_NETGAME ? actor->special2 : 0;
    if(!players[playerNum].plr->ingame)
    {                           // Player left the game
        return;
    }
    player = &players[playerNum];
    if(player->health <= 0)
    {                           // Player is dead
        return;
    }

    if(player->rain1 && player->rain2)
    {                           // Terminate an active rain
        if(player->rain1->health < player->rain2->health)
        {
            if(player->rain1->health > 16)
            {
                player->rain1->health = 16;
            }
            player->rain1 = NULL;
        }
        else
        {
            if(player->rain2->health > 16)
            {
                player->rain2->health = 16;
            }
            player->rain2 = NULL;
        }
    }
    // Add rain mobj to list
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
    fixed_t x;
    fixed_t y;
    mobj_t *mo;
    int     playerNum;
    player_t *player;

    if(actor->health-- == 0)
    {
        P_SetMobjState(actor, S_NULL);
        playerNum = IS_NETGAME ? actor->special2 : 0;
        if(!players[playerNum].plr->ingame)
        {   // Player left the game
            return;
        }
        player = &players[playerNum];
        if(player->health <= 0)
        {   // Player is dead
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
    {   // Fudge rain frequency
        return;
    }
    x = actor->pos[VX] + ((P_Random() & 127) - 64) * FRACUNIT;
    y = actor->pos[VY] + ((P_Random() & 127) - 64) * FRACUNIT;
    mo = P_SpawnMobj(x, y, ONCEILINGZ, MT_RAINPLR1 + actor->special2);
    mo->flags |= MF_BRIGHTSHADOW;
    mo->target = actor->target;
    mo->mom[MX] = 1;               // Force collision detection
    mo->mom[MZ] = -mo->info->speed;
    mo->special2 = actor->special2; // Transfer player number
    P_CheckMissileSpawn(mo);
    if(!(actor->special1 & 31))
        S_StartSound(sfx_ramrain, actor);

    actor->special1++;
}

void C_DECL A_RainImpact(mobj_t *actor)
{
    if(actor->pos[VZ] > FLT2FIX(actor->floorz))
    {
        P_SetMobjState(actor, S_RAINAIRXPLR1_1 + actor->special2);
    }
    else if(P_Random() < 40)
    {
        P_HitFloor(actor);
    }
}

void C_DECL A_HideInCeiling(mobj_t *actor)
{
    actor->pos[VZ] = FLT2FIX(actor->ceilingz + 4);
}

void C_DECL A_FirePhoenixPL1(player_t *player, pspdef_t * psp)
{
    angle_t angle;

    P_ShotAmmo(player);
    if(IS_CLIENT)
        return;

    P_SpawnMissile(player->plr->mo, NULL, MT_PHOENIXFX1);
    angle = player->plr->mo->angle + ANG180;
    angle >>= ANGLETOFINESHIFT;
    player->plr->mo->mom[MX] += FixedMul(4 * FRACUNIT, finecosine[angle]);
    player->plr->mo->mom[MY] += FixedMul(4 * FRACUNIT, finesine[angle]);
}

void C_DECL A_PhoenixPuff(mobj_t *actor)
{
    mobj_t *puff;
    angle_t angle;

    P_SeekerMissile(actor, ANGLE_1 * 5, ANGLE_1 * 10);

    puff = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_PHOENIXPUFF);
    angle = actor->angle + ANG90;
    angle >>= ANGLETOFINESHIFT;
    puff->mom[MX] = FixedMul(FRACUNIT * 1.3, finecosine[angle]);
    puff->mom[MY] = FixedMul(FRACUNIT * 1.3, finesine[angle]);
    puff->mom[MZ] = 0;

    puff = P_SpawnMobj(actor->pos[VX], actor->pos[VY], actor->pos[VZ], MT_PHOENIXPUFF);
    angle = actor->angle - ANG90;
    angle >>= ANGLETOFINESHIFT;
    puff->mom[MX] = FixedMul(FRACUNIT * 1.3, finecosine[angle]);
    puff->mom[MY] = FixedMul(FRACUNIT * 1.3, finesine[angle]);
    puff->mom[MZ] = 0;
}

void C_DECL A_InitPhoenixPL2(player_t *player, pspdef_t * psp)
{
    player->flamecount = FLAME_THROWER_TICS;
}

/*
 * Flame thrower effect.
 */
void C_DECL A_FirePhoenixPL2(player_t *player, pspdef_t * psp)
{
    mobj_t *mo;
    mobj_t *pmo;
    angle_t angle;
    fixed_t pos[3];
    fixed_t slope;

    if(IS_CLIENT)
        return;

    if(--player->flamecount == 0)
    {                           // Out of flame
        P_SetPsprite(player, ps_weapon, S_PHOENIXATK2_4);
        NetSv_PSpriteChange(player - players, S_PHOENIXATK2_4);
        player->refire = 0;
        return;
    }

    pmo = player->plr->mo;
    angle = pmo->angle;
    memcpy(pos, pmo->pos, sizeof(pos));
    pos[VX] += ((P_Random() - P_Random()) << 9);
    pos[VY] += ((P_Random() - P_Random()) << 9);
    pos[VZ] += 26 * FRACUNIT + ((int) (player->plr->lookdir) << FRACBITS) / 173;

    pos[VZ] -= FLT2FIX(pmo->floorclip);

    slope = FRACUNIT * sin(LOOKDIR2RAD(player->plr->lookdir)) / 1.2;
    mo = P_SpawnMobj(pos[VX], pos[VY], pos[VZ], MT_PHOENIXFX2);
    mo->target = pmo;
    mo->angle = angle;
    mo->mom[MX] =
        pmo->mom[MX] + FixedMul(mo->info->speed,
                             finecosine[angle >> ANGLETOFINESHIFT]);
    mo->mom[MY] =
        pmo->mom[MY] + FixedMul(mo->info->speed,
                             finesine[angle >> ANGLETOFINESHIFT]);
    mo->mom[MZ] = FixedMul(mo->info->speed, slope);
    if(!player->refire || !(leveltime % 38))
    {
        S_StartSoundEx(sfx_phopow, player->plr->mo);
    }
    P_CheckMissileSpawn(mo);
}

void C_DECL A_ShutdownPhoenixPL2(player_t *player, pspdef_t * psp)
{
    P_ShotAmmo(player);
}

void C_DECL A_FlameEnd(mobj_t *actor)
{
    actor->mom[MZ] += 1.5 * FRACUNIT;
}

void C_DECL A_FloatPuff(mobj_t *puff)
{
    puff->mom[MZ] += 1.8 * FRACUNIT;
}

void C_DECL A_GauntletAttack(player_t *player, pspdef_t * psp)
{
    angle_t angle;
    int     damage;
    int     slope;
    int     randVal;
    fixed_t dist;

    psp->sx = ((P_Random() & 3) - 2) * FRACUNIT;
    psp->sy = WEAPONTOP + (P_Random() & 3) * FRACUNIT;
    angle = player->plr->mo->angle;
    if(player->powers[PT_WEAPONLEVEL2])
    {
        damage = HITDICE(2);
        dist = 4 * MELEERANGE;
        angle += (P_Random() - P_Random()) << 17;
        PuffType = MT_GAUNTLETPUFF2;
    }
    else
    {
        damage = HITDICE(2);
        dist = MELEERANGE + 1;
        angle += (P_Random() - P_Random()) << 18;
        PuffType = MT_GAUNTLETPUFF1;
    }
    slope = P_AimLineAttack(player->plr->mo, angle, dist);
    P_LineAttack(player->plr->mo, angle, dist, slope, damage);
    if(!linetarget)
    {
        if(P_Random() > 64)
        {
            player->plr->extralight = !player->plr->extralight;
        }
        S_StartSoundEx(sfx_gntful, player->plr->mo);
        return;
    }
    randVal = P_Random();
    if(randVal < 64)
    {
        player->plr->extralight = 0;
    }
    else if(randVal < 160)
    {
        player->plr->extralight = 1;
    }
    else
    {
        player->plr->extralight = 2;
    }
    if(player->powers[PT_WEAPONLEVEL2])
    {
        P_GiveBody(player, damage >> 1);
        S_StartSoundEx(sfx_gntpow, player->plr->mo);
    }
    else
    {
        S_StartSoundEx(sfx_gnthit, player->plr->mo);
    }
    // turn to face target
    angle = R_PointToAngle2(player->plr->mo->pos[VX], player->plr->mo->pos[VY],
                            linetarget->pos[VX], linetarget->pos[VY]);
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

void C_DECL A_Light0(player_t *player, pspdef_t * psp)
{
    player->plr->extralight = 0;
}

void C_DECL A_Light1(player_t *player, pspdef_t * psp)
{
    player->plr->extralight = 1;
}

void C_DECL A_Light2(player_t *player, pspdef_t * psp)
{
    player->plr->extralight = 2;
}

/*
 * Called at start of level for each player
 */
void P_SetupPsprites(player_t *player)
{
    int     i;

    // Remove all psprites
    for(i = 0; i < NUMPSPRITES; i++)
    {
        player->psprites[i].state = NULL;
    }
    // Spawn the ready weapon
    player->pendingweapon = player->readyweapon;
    P_BringUpWeapon(player);
}

/*
 * Called every tic by player thinking routine
 */
void P_MovePsprites(player_t *player)
{
    int     i;
    pspdef_t *psp;
    state_t *state;

    psp = &player->psprites[0];
    for(i = 0; i < NUMPSPRITES; i++, psp++)
    {
        if((state = psp->state) != 0)   // a null state means not active
        {
            // drop tic count and possibly change state
            if(psp->tics != -1) // a -1 tic count never changes
            {
                psp->tics--;
                if(!psp->tics)
                {
                    P_SetPsprite(player, i, psp->state->nextstate);
                }
            }
        }
    }
    player->psprites[ps_flash].sx = player->psprites[ps_weapon].sx;
    player->psprites[ps_flash].sy = player->psprites[ps_weapon].sy;
}
