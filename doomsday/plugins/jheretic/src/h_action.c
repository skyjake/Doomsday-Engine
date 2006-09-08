/**\file
 *\section Copyright and License Summary
 * License: GPL + jHeretic/jHexen Exception
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 * Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 * Copyright © 2005-2006 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

// HEADER FILES ------------------------------------------------------------

#include "jheretic.h"

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
    {"flyup", 0},
    {"flydown", 0},
    {"falldown", 0},
    {"lookup", 0},
    {"lookdown", 0},
    {"lookcntr", 0},
    {"usearti", 0},
    {"mlook", 0},
    {"jlook", 0},
    {"nextwpn", 0},
    {"prevwpn", 0},
    {"weap1", 0},           // Staff
    {"weapon2", 0},
    {"weapon3", 0},
    {"weapon4", 0},
    {"weapon5", 0},
    {"weapon6", 0},
    {"weapon7", 0},
    {"weapon8", 0},         // Gauntlets
    {"weapon9", 0},
    {"cantdie", 0},
    {"invisib", 0},
    {"health", 0},
    {"sphealth", 0},
    {"tomepwr", 0},
    {"torch", 0},
    {"firebomb", 0},
    {"egg", 0},
    {"flyarti", 0},
    {"teleport", 0},
    {"panic", 0},
    {"demostop", 0},
    {"jump", 0},
    {"mzoomin", 0},
    {"mzoomout", 0},
    {"mpanup", 0},
    {"mpandown", 0},
    {"mpanleft", 0},
    {"mpanright", 0},
    {"weapon1", 0},         // Weapon cycle 1: staff/guantlets
    {"", 0}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------
