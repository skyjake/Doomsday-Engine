/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * xgclass.h:
 */

#ifndef __XG_LINE_CLASSES_H__
#define __XG_LINE_CLASSES_H__

// When the common playsim is in place - Doomsday will call the
// the XG Class Funcs which are owned by the game.

// iparm string mapping identifiers
#define MAP_SND             0x01000000
#define MAP_MUS             0x02000000
#define MAP_MATERIAL        0x04000000
#define MAP_MASK            0x00ffffff

enum {
    XGPF_INT,
    XGPF_FLOAT,
    XGPF_STRING
};

typedef struct {
    int             flags;
    char            name[128];
    char            flagPrefix[20];
    int             map;
} xgclassparm_t;

typedef enum {
    TRAV_NONE, // The class func is executed once only, WITHOUT any traversal
    TRAV_LINES,
    TRAV_PLANES,
    TRAV_SECTORS /* Actually traverses planes but pretends to the user that its
                    traversing sectors via XG_Dev messages (easier to comprehend) */
} xgtravtype;

#ifndef C_DECL
#  if defined(WIN32)
#    define C_DECL __cdecl
#  elif defined(UNIX)
#    define C_DECL
#  endif
#endif

typedef struct xgclass_s{
    // Do function (called during ref iteration)
    int             (C_DECL *doFunc)();

    // Init function (called once, before ref iteration)
    void            (*initFunc)(struct linedef_s *line);

    // what the class wants to traverse
    xgtravtype      traverse;

    // the iparm numbers to use for ref traversal
    int             travRef;
    int             travData;

    int             evTypeFlags; /* if > 0 the class only supports certain
                                    event types (which are flags on this var) */
    const char*     className; // txt string id
    xgclassparm_t   iparm[20]; // iparms
} xgclass_t;

// Line type classes. Add new classes to the end!
enum
{
    LTC_NONE, // No action.
    LTC_CHAIN_SEQUENCE,
    LTC_PLANE_MOVE,
    LTC_BUILD_STAIRS,
    LTC_DAMAGE,
    LTC_POWER,
    LTC_LINE_TYPE,
    LTC_SECTOR_TYPE,
    LTC_SECTOR_LIGHT,
    LTC_ACTIVATE,
    LTC_KEY,
    LTC_MUSIC, // Change the music to play.
    LTC_LINE_COUNT, // Line activation count delta.
    LTC_LEAVE_MAP,
    LTC_DISABLE_IF_ACTIVE,
    LTC_ENABLE_IF_ACTIVE,
    LTC_EXPLODE, // Explodes the activator.
    LTC_PLANE_MATERIAL,
    LTC_WALL_MATERIAL,
    LTC_COMMAND,
    LTC_SOUND, // Play a sector sound.
    LTC_MIMIC_SECTOR,
    LTC_TELEPORT,
    LTC_LINE_TELEPORT,
    NUMXGCLASSES
};

#endif
