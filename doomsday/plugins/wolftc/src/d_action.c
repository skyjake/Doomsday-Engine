/**\file
 * Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 * Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
 * Copyright © 1993-1996 by id Software, Inc.
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

action_t actions[NUM_ACTIONS + 1] = {
    {"left", 0},
    {"right", 0},
    {"forward", 0},
    {"backward", 0},
    {"strafel", 0},
    {"strafer", 0},
    {"fire", 0},
    {"use", 0},
    {"strafe", 0},
    {"speed", 0},
    {"weap1", 0},       // Fist.
    {"weapon2", 0},
    {"weap3", 0},       // Shotgun.
    {"weapon4", 0},
    {"weapon5", 0},
    {"weapon6", 0},
    {"weapon7", 0},
    {"weapon8", 0},     // Chainsaw
    {"weapon9", 0},     // Super sg.
    {"nextwpn", 0},
    {"prevwpn", 0},
    {"mlook", 0},
    {"jlook", 0},
    {"lookup", 0},
    {"lookdown", 0},
    {"lookcntr", 0},
    {"jump", 0},
    {"demostop", 0},
    {"weapon1", 0},     // Weapon cycle 1: fist/chain saw
    {"weapon3", 0},     // Weapon cycle 2: shotgun/super shotgun
    {"mzoomin", 0},
    {"mzoomout", 0},
    {"mpanup", 0},
    {"mpandown", 0},
    {"mpanleft", 0},
    {"mpanright", 0},
    {"flyup", 0},
    {"flydown", 0},
    {"falldown", 0},
    {"", 0}                     // A terminator.
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------
