/** @file p_xgline.c Extended generalized line types.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#if __JDOOM__ || __JHERETIC__ || __JDOOM64__

#include <time.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

#include "d_net.h"
#include "dmu_lib.h"
#include "gamesession.h"
#include "p_mapsetup.h"
#include "p_xgline.h"
#include "p_xgsec.h"
#include "p_actor.h"
#include "player.h"
#include "p_map.h"
#include "p_mapspec.h"
#include "p_terraintype.h"
#include "p_tick.h"
#include "p_sound.h"
#include "p_switch.h"

using namespace de;

#define XLTIMER_STOPPED 1    // Timer stopped.

#define EVTYPESTR(evtype) (evtype == XLE_CHAIN? "CHAIN" \
        : evtype == XLE_CROSS? "CROSS" \
        : evtype == XLE_USE? "USE" \
        : evtype == XLE_SHOOT? "SHOOT" \
        : evtype == XLE_HIT? "HIT" \
        : evtype == XLE_TICKER? "TICKER" \
        : evtype == XLE_AUTO? "AUTO" \
        : evtype == XLE_FORCED? "FORCED" \
        : evtype == XLE_FUNC? "FUNCTION" : "???")

#define LREFTYPESTR(reftype) (reftype == LREF_NONE? "NONE" \
        : reftype == LREF_SELF? "SELF" \
        : reftype == LREF_TAGGED? "TAGGED LINES" \
        : reftype == LREF_LINE_TAGGED? "LINE TAGGED LINES" \
        : reftype == LREF_ACT_TAGGED? "ACT TAGGED LINES" \
        : reftype == LREF_INDEX? "INDEXED LINE" \
        : reftype == LREF_ALL? "ALL LINES" : "???")

#define LPREFTYPESTR(reftype) (reftype == LPREF_NONE? "NONE" \
        : reftype == LPREF_MY_FLOOR? "MY FLOOR" \
        : reftype == LPREF_TAGGED_FLOORS? "TAGGED FLOORS" \
        : reftype == LPREF_LINE_TAGGED_FLOORS? "LINE TAGGED FLOORS" \
        : reftype == LPREF_ACT_TAGGED_FLOORS? "ACT TAGGED FLOORS" \
        : reftype == LPREF_INDEX_FLOOR? "INDEXED FLOOR" \
        : reftype == LPREF_ALL_FLOORS? "ALL FLOORS" \
        : reftype == LPREF_MY_CEILING? "MY CEILING" \
        : reftype == LPREF_TAGGED_CEILINGS? "TAGGED CEILINGS" \
        : reftype == LPREF_LINE_TAGGED_CEILINGS? "LINE TAGGED CEILINGS" \
        : reftype == LPREF_ACT_TAGGED_CEILINGS? "ACT TAGGED CEILINGS" \
        : reftype == LPREF_INDEX_CEILING? "INDEXED CEILING" \
        : reftype == LPREF_ALL_CEILINGS? "ALL CEILINGS" \
        : reftype == LPREF_SPECIAL? "SPECIAL" \
        : reftype == LPREF_BACK_FLOOR? "BACK FLOOR" \
        : reftype == LPREF_BACK_CEILING? "BACK CEILING" \
        : reftype == LPREF_THING_EXIST_FLOORS? "SECTORS WITH THING - FLOOR" \
        : reftype == LPREF_THING_EXIST_CEILINGS? "SECTORS WITH THING - CEILING" \
        : reftype == LPREF_THING_NOEXIST_FLOORS? "SECTORS WITHOUT THING - FLOOR" \
        : reftype == LPREF_THING_NOEXIST_CEILINGS? "SECTORS WITHOUT THING - CEILING" : "???")

#define LSREFTYPESTR(reftype) (reftype == LSREF_NONE? "NONE" \
        : reftype == LSREF_MY? "MY SECTOR" \
        : reftype == LSREF_TAGGED? "TAGGED SECTORS" \
        : reftype == LSREF_LINE_TAGGED? "LINE TAGGED SECTORS" \
        : reftype == LSREF_ACT_TAGGED? "ACT TAGGED SECTORS" \
        : reftype == LSREF_INDEX? "INDEXED SECTOR" \
        : reftype == LSREF_ALL? "ALL SECTORS" \
        : reftype == LSREF_BACK? "BACK SECTOR" \
        : reftype == LSREF_THING_EXIST? "SECTORS WITH THING" \
        : reftype == LSREF_THING_NOEXIST? "SECTORS WITHOUT THING" : "???")

#define TO_DMU_TOP_COLOR(x) ((x) == 0? DMU_TOP_COLOR_RED \
        : (x) == 1? DMU_TOP_COLOR_GREEN \
        : DMU_TOP_COLOR_BLUE)

#define TO_DMU_MIDDLE_COLOR(x) ((x) == 0? DMU_MIDDLE_COLOR_RED \
        : (x) == 1? DMU_MIDDLE_COLOR_GREEN \
        : (x) == 2? DMU_MIDDLE_COLOR_BLUE \
        : DMU_MIDDLE_ALPHA)

#define TO_DMU_BOTTOM_COLOR(x) ((x) == 0? DMU_BOTTOM_COLOR_RED \
        : (x) == 1? DMU_BOTTOM_COLOR_GREEN \
        : DMU_BOTTOM_COLOR_BLUE)

void XL_ChangeMaterial(Line *line, int sidenum, int section, world_Material *mat,
    blendmode_t blend = BM_NORMAL, const Vec4f &tintColor = Vec4f(), int flags = 0);

int XL_DoChainSequence(Line *line, dd_bool ceiling, void *context, void *context2, mobj_t *activator);
int XL_DoDamage(Line *line, dd_bool ceiling, void *context, void *context2, mobj_t *activator);
int XL_DoPower(Line *line, dd_bool ceiling, void *context, void *context2, mobj_t *activator);
int XL_DoKey(Line *line, dd_bool ceiling, void *context, void *context2, mobj_t *activator);
int XL_DoExplode(Line *line, dd_bool ceiling, void *context, void *context2, mobj_t *activator);
int XL_DoCommand(Line *line, dd_bool ceiling, void *context, void *context2, mobj_t *activator);

int XLTrav_ChangeLineType(Line *line, dd_bool ceiling, void *context, void *context2, mobj_t *activator);
int XLTrav_Activate(Line *line, dd_bool ceiling, void *context, void *context2, mobj_t *activator);
int XLTrav_Music(Line *line, dd_bool ceiling, void *context, void *context2, mobj_t *activator);
int XLTrav_LineCount(Line *line, dd_bool ceiling, void *context, void *context2, mobj_t *activator);
int XLTrav_LeaveMap(Line *line, dd_bool ceiling, void *context, void *context2, mobj_t *activator);
int XLTrav_DisableLine(Line *line, dd_bool ceiling, void *context, void *context2, mobj_t *activator);
int XLTrav_EnableLine(Line *line, dd_bool ceiling, void *context, void *context2, mobj_t *activator);
int XLTrav_ChangeWallMaterial(Line *line, dd_bool ceiling, void *context, void *context2, mobj_t *activator);
int XLTrav_LineTeleport(Line *line, dd_bool ceiling, void *context, void *context2, mobj_t *activator);

int xgDev = 0; // Print dev messages.

static linetype_t typebuffer;
static char msgbuf[80];
ThinkerT<mobj_s> dummyThing;

struct mobj_s *XG_DummyThing()
{
    return dummyThing;
}

/* ADD NEW XG CLASSES TO THE END - ORIGINAL INDICES MUST STAY THE SAME!!! */
xgclass_t xgClasses[NUMXGCLASSES] =
{
    { NULL, NULL, TRAV_NONE, 0, 1, 0, "None",
      // Dummy class (has no functions but enables use of secondary actions) (no params)
      {{XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
      }
    },
    { de::function_cast<int (*)()>(XL_DoChainSequence), NULL, TRAV_NONE, 0, 1, 0, "Chain Sequence",
      // Execute a chain of other XG line types (a zero ends the list)
       {{XGPF_INT, "Chain Flags", "chsf_", 0},              // ip0: (chsf_) chain sequence flags
        {XGPF_INT, "Line Type 0", "", -1},                  // ip1: Type to execute
        {XGPF_INT, "Line Type 1", "", -1},                  // ip2:  ""  ""  ""
        {XGPF_INT, "Line Type 2", "", -1},                  // ip3:  ""  ""  ""
        {XGPF_INT, "Line Type 3", "", -1},                  // ip4:  ""  ""  ""
        {XGPF_INT, "Line Type 4", "", -1},                  // ip5:  ""  ""  ""
        {XGPF_INT, "Line Type 5", "", -1},                  // ip6:  ""  ""  ""
        {XGPF_INT, "Line Type 6", "", -1},                  // ip7:  ""  ""  ""
        {XGPF_INT, "Line Type 7", "", -1},                  // ip8:  ""  ""  ""
        {XGPF_INT, "Line Type 8", "", -1},                  // ip9:  ""  ""  ""
        {XGPF_INT, "Line Type 9", "", -1},                  // ip10: ""  ""  ""
        {XGPF_INT, "Line Type 10", "", -1},                 // ip11: ""  ""  ""
        {XGPF_INT, "Line Type 11", "", -1},                 // ip12: ""  ""  ""
        {XGPF_INT, "Line Type 12", "", -1},                 // ip13: ""  ""  ""
        {XGPF_INT, "Line Type 13", "", -1},                 // ip14: ""  ""  ""
        {XGPF_INT, "Line Type 14", "", -1},                 // ip15: ""  ""  ""
        {XGPF_INT, "Line Type 15", "", -1},                 // ip16: ""  ""  ""
        {XGPF_INT, "Line Type 16", "", -1},                 // ip17: ""  ""  ""
        {XGPF_INT, "Line Type 17", "", -1},                 // ip18: ""  ""  ""
        {XGPF_INT, "Line Type 18", "", -1}                  // ip19: ""  ""  ""
       }
    },
    { de::function_cast<int (*)()>(XSTrav_MovePlane), XS_InitMovePlane, TRAV_PLANES, 0, 1, 0, "Move Plane",
      // Move one or more planes. Optionaly change textures/types on start/end
       {{XGPF_INT, "Target Ref", "lpref_", 0},              // ip0: (plane ref) plane(s) to move.
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Destination Ref", "spref_", 2},         // ip2: destination height type (zero, relative to current, surrounding highest/lowest floor/ceiling)
        {XGPF_INT, "Move Flags", "pmf_", 3},                // ip3: flags (PMF_*)
        {XGPF_INT, "Start Sound", "", 4 | MAP_SND},         // ip4: start sound
        {XGPF_INT, "End Sound", "", 5 | MAP_SND},           // ip5: end sound
        {XGPF_INT, "Move Sound", "", 6 | MAP_SND},          // ip6: move sound
        {XGPF_INT, "Start Material Ref", "spref_", 7},       // ip7: start texture origin (uses same ids as i2) (spec: use ip8 as tex num)
        {XGPF_INT, "Start Material Num", "", 8 | MAP_MATERIAL}, // ip8: data component or number/name of flat
        {XGPF_INT, "End Material Ref", "spref_", 9},         // ip9: end texture origin (uses same ids as i2) (spec: use ip10 as tex num)
        {XGPF_INT, "End Material Num", "", 10 | MAP_MATERIAL}, // ip10: data component or number/name of flat
        {XGPF_INT, "Start Type Ref", "lpref_", 11},         // ip11: (plane ref) start sector type (spec: use i12 as type ID)
        {XGPF_INT, "Start Type Num", "", -1},               // ip12: data component or type ID
        {XGPF_INT, "End Type Ref", "lpref_", 13},           // ip13: (plane ref) end sector type (spec: use i14 as type ID)
        {XGPF_INT, "End Type Num", "", -1},                 // ip14: data component or type ID
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XSTrav_BuildStairs), XS_InitStairBuilder, TRAV_PLANES, 0, 1, 0, "Build Stairs",
      // Moves one or more planes, incrementing their height with each move
       {{XGPF_INT, "Target Ref", "lpref_", 0},              // ip0: (plane ref) plane to start from
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Spread Material", "", -1},               // ip2: (true/false) stop when texture changes
        {XGPF_INT, "Spread Build", "", -1},                 // ip3: (true/false) spread build?
        {XGPF_INT, "Start Sound", "", 4 | MAP_SND},         // ip4: start build sound (doesn't wait)
        {XGPF_INT, "Step Start Sound", "", 5 | MAP_SND},    // ip5: step start sound
        {XGPF_INT, "Step End Sound", "", 6 | MAP_SND},      // ip6: step end sound
        {XGPF_INT, "Step Move Sound", "", 7 | MAP_SND},     // ip7: step move sound
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XL_DoDamage), NULL, TRAV_NONE, 0, 1, 0, "Damage",
      // Deals health damage to the activator
       {{XGPF_INT, "Min Delta", "", -1},                    // ip0: min damage delta
        {XGPF_INT, "Max Delta", "", -1},                    // ip1: max damage delta
        {XGPF_INT, "Min Limit", "", -1},                    // ip2: min limit (wont damage if health bellow)
        {XGPF_INT, "Max Limit", "", -1},                    // ip3: max limit (wont damage if health above)
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
      }
    },
    { de::function_cast<int (*)()>(XL_DoPower), NULL, TRAV_NONE, 0, 1, 0, "Power",
      // Deals armor damage to the activator (must be a player)
       {{XGPF_INT, "Min Delta", "", -1},                    // ip0: min power delta
        {XGPF_INT, "Max Delta", "", -1},                    // ip1: max power delta
        {XGPF_INT, "Min Limit", "", -1},                    // ip2: min limit
        {XGPF_INT, "Max Limit", "", -1},                    // ip3: max limit
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XLTrav_ChangeLineType), NULL, TRAV_LINES, 0, 1, 0, "Line Type",
      // Changes a line's type (must be an XG type)
       {{XGPF_INT, "Target Ref", "lref_", 0},               // ip0: (line ref) line(s) to change
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Line Type", "", -1},                    // ip2: new type (must be an XG line type)
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XSTrav_SectorType), NULL, TRAV_SECTORS, 0, 1, 0, "Sector Type",
      // Changes a sector's type (must be an XG type)
       {{XGPF_INT, "Target Ref", "lsref_", 0},              // ip0: (sector ref) sector(s) to change
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Sector Type", "", -1},                  // ip2: new type (zero or an XG sector type)
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XSTrav_SectorLight), NULL, TRAV_SECTORS, 0, 1, 0, "Sector Light",
      // Change the light level and/or color  of the target sector(s).
       {{XGPF_INT, "Target Ref", "lsref_", 0},              // ip0: (sector ref) sector(s) to change
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Change Light", "", -1},                 // ip2: if non-zero light level will be changed
        {XGPF_INT, "Change Color", "", -1},                 // ip3: if non-zero colour will be changed
        {XGPF_INT, "Light Ref", "lightref_", 4},            // ip4: (light ref) sector to get the initial light delta from.
                                                            //      lightref_none makes ip5 an absolute value
        {XGPF_INT, "Light Delta", "", -1},                  // ip5: offset to the delta or absolute value
        {XGPF_INT, "Color Ref", "lightref_", 6},            // ip6: (light ref) sector to get the initial colour deltas from.
                                                            //      lightref_none makes ip7-9 absolute values
        {XGPF_INT, "Red Delta", "", -1},                    // ip7: offset to red delta
        {XGPF_INT, "Green Delta", "", -1},                  // ip8: offset to green delta
        {XGPF_INT, "Blue Delta", "", -1},                   // ip9: offset to blue delta
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XLTrav_Activate), NULL, TRAV_LINES, 0, 1, 0, "Activate",
      // Sends a chain event to all the referenced lines
       {{XGPF_INT, "Target Ref", "lref_", 0},               // ip0: (line ref) line(s) to activate
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XL_DoKey), NULL, TRAV_NONE, 0, 1, 0, "Key",
      // Gives/takes keys to/from the activator (must be a player)
      // Params are bitfields! Bit 1 (0x1) corresponds key 1, bit 2 (0x2) key 2, etc.
       {{XGPF_INT, "Give Keys", "", -1},                    // ip0: keys to give
        {XGPF_INT, "Take Keys", "", -1},                    // ip1: keys to take away.
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XLTrav_Music), NULL, TRAV_LINES, 2, 3, 0, "Music",
      // Changes the music track being played
       {{XGPF_INT, "Song ID", "ldref_", 0 | MAP_MUS},       // ip0: song id/name or (line data ref from ip2)
        {XGPF_INT, "Play Looped", "", -1},                  // ip1: non-zero means play looped
        {XGPF_INT, "Data Ref", "lref_", 2},                 // ip2: (line ref) used with line data ref eg set music track to line-tag
        {XGPF_INT, "Data Num", "", -1},                     // ip3:
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XLTrav_LineCount), NULL, TRAV_LINES, 0, 1, 0, "Line Count",
      // Changes the XG line(s)' internal activation counter
       {{XGPF_INT, "Target Ref", "lref_", 0},               // ip0: (line ref) line(s) to change
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Set Absolute", "", -1},                 // ip2: non-zero makes ip3 absolute
        {XGPF_INT, "Count Delta", "", -1},                  // ip3: count delta or absolute
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XLTrav_LeaveMap), NULL, TRAV_LINES, 1, 2, 0, "Leave Map",
      // Exits the current map
       {{XGPF_INT, "Secret Exit", "", -1},                  // ip0: non-zero goto secret map
        {XGPF_INT, "Data Ref", "lref_", 1},                 // ip1: (line ref) line to acquire (line data ref) from
        {XGPF_INT, "Data Num", "", -1},                     // ip2:
        {XGPF_INT, "Goto Map", "ldref_", 3},                // ip3: map ID or (line data ref from ip1)
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XLTrav_DisableLine), NULL, TRAV_LINES, 0, 1, 0, "Disable Line",
      // Disables the referenced line(s) if active
       {{XGPF_INT, "Target Ref", "lref_", 0},               // ip0: (line ref) line(s) to disable
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XLTrav_EnableLine), NULL, TRAV_LINES, 0, 1, 0, "Enable Line",
      // Enables the referenced line(s) if active.
       {{XGPF_INT, "Target Ref", "lref_", 0},               // ip0: (line ref) line(s) to enable
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XL_DoExplode), NULL, TRAV_NONE, 0, 1, 0, "Explode",
      // Explodes the activator (no params).
      {{XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
      }
    },
    { de::function_cast<int (*)()>(XSTrav_PlaneMaterial), NULL, TRAV_PLANES, 0, 1, 0, "Plane Material",
      // Change the material and/or surface color of a plane.
       {{XGPF_INT, "Target Ref", "lpref_", 0},              // ip0 : (plane ref) plane(s) to change
        {XGPF_INT, "Target Num", "", -1},                   // ip1 : ref data
        {XGPF_INT, "Material Ref", "spref_", 2},            // ip2 : Texture ref
        {XGPF_INT, "Material Num", "", 3 | MAP_MATERIAL},   // ip3 : texture number (flat), used with SPREF_NONE
        {XGPF_INT, "Red Delta", "", -1},                    // ip4 : plane surface color (red)
        {XGPF_INT, "Green Delta", "", -1},                  // ip5 : "" (green)
        {XGPF_INT, "Blue Delta", "", -1},                   // ip6 : "" (blue)
        {XGPF_INT, "Change Color", "", -1},                 // ip7: if non-zero tint color will be changed
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XLTrav_ChangeWallMaterial), NULL, TRAV_LINES, 0, 1, 0, "Wall Material",
      // Changes material(s) on the referenced line(s).
      // Changes surface colour(s), alpha, mid textue blendmode and side flags
       {{XGPF_INT, "Target Ref", "lref_", 0},               // ip0: (line ref) line(s) to change
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Side Num", "", -1},                     // ip2: non-zero change the back side
        {XGPF_INT, "Top Material", "", 3 | MAP_MATERIAL},    // ip3: top texture to change to (blank indicates no change)
        {XGPF_INT, "Middle Material", "", 4 | MAP_MATERIAL}, // ip4: middle texture to change to (blank indicates no change)
        {XGPF_INT, "Bottom Material", "", 5 | MAP_MATERIAL}, // ip5: bottom texture to change to (blank indicates no change)
        {XGPF_INT, "Set Mid If None", "", -1},              // ip6: set mid texture even if previously zero
        {XGPF_INT, "Sidedef Flags", "sdf_", 7},             // ip7: (sdf_) side flags (used with surface colour blending, fullbright etc)
        {XGPF_INT, "Middle Blendmode", "bm_", 8},           // ip8: (bm_) middle texture blendmode
        {XGPF_INT, "Top Red Delta", "", -1},                // ip9:
        {XGPF_INT, "Top Green Delta", "", -1},              // ip10:
        {XGPF_INT, "Top Blue Delta", "", -1},               // ip11:
        {XGPF_INT, "Middle Red Delta", "", -1},             // ip12:
        {XGPF_INT, "Middle Green Delta", "", -1},           // ip13:
        {XGPF_INT, "Middle Blue Delta", "", -1},            // ip14:
        {XGPF_INT, "Middle Alpha Delta", "", -1},           // ip15:
        {XGPF_INT, "Bottom Red Delta", "", -1},             // ip16:
        {XGPF_INT, "Bottom Green Delta", "", -1},           // ip17:
        {XGPF_INT, "Bottom Blue Delta", "", -1},            // ip18:
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XL_DoCommand), NULL, TRAV_NONE, 0, 1, 0, "Command",
      // Executes a console command (CCmd)
      {{XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
       {XGPF_INT, "", "", -1},
      }
    },
    { de::function_cast<int (*)()>(XSTrav_SectorSound), NULL, TRAV_SECTORS, 0, 1, 0, "Sector Sound",
      // Plays a sound in sector(s)
       {{XGPF_INT, "Target Ref", "lsref_", 0},              // ip0: (sector ref) sector(s) to play the sound in
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Sound ID", "", 2 | MAP_SND},            // ip2: sound name/id to play
        {XGPF_INT, "Origin", "", -1},                       // ip3: non-zero = play from a specific origin (1=floor, 2=ceiling) else 0=center
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XSTrav_MimicSector), NULL, TRAV_SECTORS, 0, 1, 0, "Mimic Sector",
      // Copies all properties from target sector to destination sector(s)
       {{XGPF_INT, "Target Ref", "lsref_", 0},              // ip0: (sector ref) sector(s) to change
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "Mimic Ref", "spref_", 2},               // ip2: (spref) sector to mimic
        {XGPF_INT, "Mimic Num", "", -1},                    // ip3:
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XSTrav_Teleport), NULL, TRAV_SECTORS, 0, 1, 0, "Teleport",
      // Teleports the activator to the first teleport exit in the target sector
       {{XGPF_INT, "Target Ref", "lsref_", 0},              // ip0: (sector ref) sector(s) to teleport to (first acceptable target is used)
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "No Flash", "", -1},                     // ip2: non-zero = no flash (or sound)
        {XGPF_INT, "No Sound", "", -1},                     // ip3: non-zero = no sound
        {XGPF_INT, "Always Stomp", "", -1},                 // ip4: non-zero = Always telefrag
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    },
    { de::function_cast<int (*)()>(XLTrav_LineTeleport), NULL, TRAV_LINES, 0, 1, 1 | XLE_CROSS, "Line Teleport",
      // Teleports the activator to the referenced line
       {{XGPF_INT, "Target Ref", "lref_", 0},               // ip0: (line ref) teleport destination
        {XGPF_INT, "Target Num", "", -1},                   // ip1:
        {XGPF_INT, "No Flash", "", -1},                     // ip2: non-zero = spawn MT_TFOG
        {XGPF_INT, "Teleport Sound", "", 3 | MAP_SND},      // ip3: sound ID/name to play (or silent)
        {XGPF_INT, "Exit Side", "", -1},                    // ip4: non-zero = exit from the back of the target line
        {XGPF_INT, "Always Stomp", "", -1},                 // ip5: non-zero = Always telefrag
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1},
        {XGPF_INT, "", "", -1}
      }
    }
};

void XG_Register()
{
    C_VAR_INT("xg-dev", &xgDev, CVF_NO_ARCHIVE, 0, 1);

    C_CMD("movefloor",  0, MovePlane);
    C_CMD("moveceil",   0, MovePlane);
    C_CMD("movesec",    0, MovePlane);
}

/**
 * Init XG data for the map.
 */
void XG_Init(void)
{
    XL_Init(); // Init lines.
    XS_Init(); // Init sectors.
}

void XG_Ticker(void)
{
    // Nothing to do.
}

/**
 * This is called during an engine reset. Disables all XG functionality!
 */
void XG_Update(void)
{
    // Clients rely on the server, they don't do XG themselves.
    if(IS_CLIENT)
        return;

    XG_ReadTypes();
    XS_Update();
    XL_Update();
}

/**
 * Adds the given binary format line type to the generated types array
 */
int XL_AddAutoGenType(linetype_t * /*newtype*/)
{
    return true;
}

/**
 * Converts a line ID number to a line type (BOOM support).
 */
int XL_AutoGenType(int /*id*/, linetype_t * /*outptr*/)
{
    return false; // Cos we don't work yet.

#if 0
    linetype_t *l;

    // Do the magic
    // is the ID in the range 12160 -> 32768
    if(!(id >= 12160 && id <= 32768))
        return false;

    // Have we already generated a line of this type?
    if(1)
    {
        // Then return a ptr to it
        outptr = l;
        return true;
    }

    // Which bitfield format are we generating from?
    if(1) //XG format
    {
        /*
         // Generate the new line type
        l->id = id;
        l->flags = def->flags[0];
        l->flags2 = def->flags[1];
        l->flags3 = def->flags[2];
        l->lineClass = def->lineClass;
        l->actType = def->actType;
        l->actCount = def->actCount;
        l->actTime = def->actTime;
     //   l->actTag = def->actTag;
        for(i = 0; i < 10; i++)
        {
            if(i == 9)
                l->aparm[i] = Def_GetMobjNum(def->aparm9);
            else
                l->aparm[i] = def->aparm[i];
        }
     //   l->tickerStart = def->tickerStart;
     //   l->tickerEnd = def->tickerEnd;
     //   l->tickerInterval = def->tickerInterval;
        l->actSound = Friendly(Def_GetSoundNum(def->actSound));
        l->deactSound = Friendly(Def_GetSoundNum(def->deactSound));
        l->evChain = def->evChain;
        l->actChain = def->actChain;
        l->deactChain = def->deactChain;
        l->actLineType = def->actLineType;
        l->deactLineType = def->deactLineType;
        l->wallSection = def->wallSection;
        l->actTex = Friendly(R_CheckTextureNumForName(def->actTex));
        l->deactTex = Friendly(R_CheckTextureNumForName(def->deactTex));
        l->actMsg = def->actMsg;
        l->deactMsg = def->deactMsg;
        l->texMoveAngle = def->texMoveAngle;
        l->texMoveSpeed = def->texMoveSpeed;*/

        // Add it to the generated line types array
        XL_AddAutoGenType(l);
        outptr = l;
    }
#if __JDOOM__ || __JDOOM64__
    else // BOOM format
    {
        // Generate the new line type
        outptr = l;
    }
#endif

    return true;
#endif
 }

/**
 * @return              @c true, if the type is defined.
 */
linetype_t* XL_GetType(int id)
{
    linetype_t*         ptr;
    char                buff[6];

    // Try finding it from the DDXGDATA lump.
    ptr = XG_GetLumpLine(id);
    if(ptr)
    {   // Got it!
        memcpy(&typebuffer, ptr, sizeof(*ptr));
        return &typebuffer;
    }

    // Does Doomsday have a definition for this?
    dd_snprintf(buff, 6, "%i", id);

    if(Def_Get(DD_DEF_LINE_TYPE, buff, &typebuffer))
        return &typebuffer;

    // Is this a type we can generate automatically?
    if(XL_AutoGenType(id, &typebuffer))
        return &typebuffer;

    // A definition was not found.
    return NULL;
}

int XG_RandomInt(int min, int max)
{
    if(max == min) return max;

    float x = M_Random() / 256.0f; // Never reaches 1.
    return (int) (min + x * (max - min) + x);
}

float XG_RandomPercentFloat(float value, int percent)
{
    float i = (2 * M_Random() / 255.0f - 1) * percent / 100.0f;
    return value * (1 + i);
}

int findXLThinker(thinker_t *th, void *context)
{
    xlthinker_t *xl = (xlthinker_t *) th;

    if(xl->line == (Line *) context)
        return true; // Stop iteration, we've found it.

    return false; // Continue iteration.
}

void XL_SetLineType(Line *line, int id)
{
    LOG_AS("XL_SetLineType");

    xline_t *xline = P_ToXLine(line);

    if(XL_GetType(id))
    {
        xline->special = id;

        // Allocate memory for the line type data.
        if(!xline->xg)
        {
            xline->xg = (xgline_t *)Z_Calloc(sizeof(xgline_t), PU_MAP, 0);
        }

        // Init the extended line state.
        xline->xg->disabled    = false;
        xline->xg->timer       = 0;
        xline->xg->tickerTimer = 0;
        std::memcpy(&xline->xg->info, &typebuffer, sizeof(linetype_t));

        // Initial active state.
        xline->xg->active      = (typebuffer.flags & LTF_ACTIVE) != 0;
        xline->xg->activator   = dummyThing;

        LOG_MAP_MSG_XGDEVONLY2("Line %i (%s), ID %i",
                P_ToIndex(line)
                << xgClasses[xline->xg->info.lineClass].className
                << id);

        // If there is not already an xlthinker for this line, create one.
        if(!Thinker_Iterate(XL_Thinker, findXLThinker, line))
        {
            // Not created one yet.
            /*xlthinker_t *xl = (xlthinker_t *)Z_Calloc(sizeof(*xl), PU_MAP, 0);

            xl->thinker.function = XL_Thinker;*/

            ThinkerT<xlthinker_t> xl(Thinker::AllocateMemoryZone);
            xl.function = XL_Thinker;
            xl->line = line;

            Thinker_Add(xl.Thinker::take());
        }
    }
    else if(id)
    {
        LOG_MAP_MSG_XGDEVONLY2("Line %i, type %i NOT DEFINED", P_ToIndex(line) << id);
    }
}

void XL_Init()
{
    dummyThing.Thinker::zap();

    // Clients rely on the server, they don't do XG themselves.
    if(IS_CLIENT) return;

    for(int i = 0; i < numlines; ++i)
    {
        Line *line = (Line *)P_ToPtr(DMU_LINE, i);
        P_ToXLine(line)->xg = 0;

        XL_SetLineType(line, P_ToXLine(line)->special);
    }
}

int XL_TraversePlanes(Line *line, int refType, int ref, void *data, void *context,
    dd_bool travsectors, mobj_t *activator, PlaneTraverserFunc func)
{
    LOG_AS(travsectors? "XL_TraverseSectors" : "XL_TraversePlanes");

    int tag = 0;
    mobj_t *mo = NULL;
    dd_bool ok, findSecTagged;

    if(xgDev)
    {
        char buff[50];

        if(ref)
        {
            sprintf(buff, ": %i", ref);
        }

        LOG_MAP_MSG_XGDEVONLY2("Line %i, ref (%s%s)",
               P_ToIndex(line)
               << (travsectors? LSREFTYPESTR(refType) : LPREFTYPESTR(refType))
               << (ref? buff: ""));
    }

    if(refType == LPREF_NONE)
        return false; // This is not a reference!

    Sector *frontSec = (Sector *)P_GetPtrp(line, DMU_FRONT_SECTOR);
    Sector *backSec  = (Sector *)P_GetPtrp(line, DMU_BACK_SECTOR);

    // References to a single plane
    if(refType == LPREF_MY_FLOOR || refType == LPREF_MY_CEILING)
    {
        if(frontSec)
        {
            return func(frontSec, refType == LPREF_MY_CEILING? true:false,
                        data, context, activator);
        }

        LOG_MAP_MSG_XGDEVONLY2("Line %i has no front sector!", P_ToIndex(line));
    }

    if(refType == LPREF_BACK_FLOOR || refType == LPREF_BACK_CEILING)
    {
        if(backSec)
        {
            return func(backSec, refType == LPREF_BACK_CEILING? true:false,
                        data, context, activator);
        }

        LOG_MAP_MSG_XGDEVONLY2("Line %i has no back sector!", P_ToIndex(line));
    }

    if(refType == LPREF_INDEX_FLOOR)
    {
        return func((Sector *)P_ToPtr(DMU_SECTOR, ref), false, data, context,
                    activator);
    }

    if(refType == LPREF_INDEX_CEILING)
    {
        return func((Sector *)P_ToPtr(DMU_SECTOR, ref), true, data, context,
                    activator);
    }

    // Can we use the tagged sector lists?
    findSecTagged = false;
    if(refType == LPREF_TAGGED_FLOORS || refType == LPREF_TAGGED_CEILINGS)
    {
        findSecTagged = true;
        tag = ref;
    }
    else if(refType == LPREF_LINE_TAGGED_FLOORS ||
            refType == LPREF_LINE_TAGGED_CEILINGS)
    {
        findSecTagged = true;
        tag = P_ToXLine(line)->tag;
    }

    // References to multiple planes
    if(findSecTagged)
    {
        // Use tagged sector lists for these (speed).
        if(iterlist_t *list = P_GetSectorIterListForTag(tag, false))
        {
            // Find the first sector with the tag.
            IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
            IterList_RewindIterator(list);

            Sector *sec;
            while((sec = (Sector *)IterList_MoveIterator(list)) != NULL)
            {
                //xsector_t *xsec = P_ToXSector(sec);

                if(refType == LPREF_TAGGED_FLOORS || refType == LPREF_TAGGED_CEILINGS)
                {
                    if(!func(sec, refType == LPREF_TAGGED_CEILINGS, data,
                             context, activator))
                    {
                        return false;
                    }
                }

                if(refType == LPREF_LINE_TAGGED_FLOORS ||
                   refType == LPREF_LINE_TAGGED_CEILINGS)
                {
                    if(!func(sec, refType == LPREF_LINE_TAGGED_CEILINGS,
                             data, context, activator))
                    {
                        return false;
                    }
                }
            }
        }
    }
    else
    {
        for(int i = 0; i < numsectors; ++i)
        {
            Sector *sec     = (Sector *)P_ToPtr(DMU_SECTOR, i);
            xsector_t *xsec = P_ToXSector(sec);

            if(refType == LPREF_ALL_FLOORS || refType == LPREF_ALL_CEILINGS)
            {
                if(!func(sec, refType == LPREF_ALL_CEILINGS, data,
                         context, activator))
                {
                    return false;
                }
            }

            if((refType == LPREF_ACT_TAGGED_FLOORS ||
                refType == LPREF_ACT_TAGGED_CEILINGS) && xsec->xg &&
                xsec->xg->info.actTag == ref)
            {
                if(!func(sec, refType == LPREF_ACT_TAGGED_CEILINGS, data,
                   context, activator))
                {
                   return false;
                }
            }

            // Reference all sectors with (at least) one mobj of specified
            // type inside.
            if(refType == LPREF_THING_EXIST_FLOORS ||
               refType == LPREF_THING_EXIST_CEILINGS)
            {
                ok = true;

                for(mo = (mobj_t *)P_GetPtrp(sec, DMT_MOBJS); ok && mo; mo = mo->sNext)
                {
                    if(mo->type == P_ToXLine(line)->xg->info.aparm[9])
                    {
                        LOG_MAP_MSG_XGDEVONLY2("Thing of type %i found in sector id %i",
                               P_ToXLine(line)->xg->info.aparm[9] << i);

                        if(!func(sec, refType == LPREF_THING_EXIST_CEILINGS,
                                 data, context, activator))
                        {
                            return false;
                        }

                        ok = false;
                    }
                }
            }

            // Reference all sectors with NONE of the specified mobj type
            // inside.
            if(refType == LPREF_THING_NOEXIST_FLOORS ||
               refType == LPREF_THING_NOEXIST_CEILINGS)
            {
                ok = true;

                for(mo = (mobj_t *)P_GetPtrp(sec, DMT_MOBJS); ok && mo; mo = mo->sNext)
                {
                    if(mo->type != P_ToXLine(line)->xg->info.aparm[9])
                    {
                        continue;
                    }

                    ok = false;
                }

                if(ok)
                {
                    LOG_MAP_MSG_XGDEVONLY2("No things of type %i found in sector id %i",
                           P_ToXLine(line)->xg->info.aparm[9] << i);

                    if(!func(sec, refType == LPREF_THING_NOEXIST_CEILINGS,
                             data, context, activator))
                    {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

int XL_TraverseLines(Line* line, int rtype, int ref, void* data,
                     void* context, mobj_t* activator, LineTraverserFunc func)
{
    LOG_AS("XL_TraverseLines");

    int i;
    int tag = 0;
    int reftype = rtype;
    char buff[50];
    Line *iter = NULL;
    dd_bool findLineTagged;

    // Binary XG data from DD_XGDATA uses the old flag values.
    // Add one to the ref type.
    if(xgDataLumps)
        reftype+= 1;

    if(ref)
        sprintf(buff," : %i",ref);

    LOG_MAP_MSG_XGDEVONLY2("Line %i, ref (%s%s)",
           P_ToIndex(line) << LREFTYPESTR(reftype) << (ref? buff : ""));

    if(reftype == LREF_NONE)
        return func(NULL, true, data, context, activator); // Not a real reference

    // References to single lines
    if(reftype == LREF_SELF) // Traversing self is simple.
        return func(line, true, data, context, activator);

    if(reftype == LREF_INDEX)
        return func((Line *)P_ToPtr(DMU_LINE, ref), true, data, context, activator);

    // Can we use the tagged line lists?
    findLineTagged = false;
    if(reftype == LREF_TAGGED)
    {
        findLineTagged = true;
        tag = ref;
    }
    else if(reftype == LREF_LINE_TAGGED)
    {
        findLineTagged = true;
        tag = P_ToXLine(line)->tag;
    }

    // References to multiple lines
    if(findLineTagged)
    {
        // Use tagged line lists for these (speed).
        if(iterlist_t *list = P_GetLineIterListForTag(tag, false))
        {
            IterList_SetIteratorDirection(list, ITERLIST_FORWARD);
            IterList_RewindIterator(list);
            while((iter = (Line *)IterList_MoveIterator(list)) != NULL)
            {
                if(reftype == LREF_TAGGED)
                {
                    if(!func(iter, true, data, context, activator))
                        return false;
                }
                else if(reftype == LREF_LINE_TAGGED)
                {
                    // Ref is true if line itself should be excluded.
                    if(!ref || iter != line)
                    {
                        if(!func(iter, true, data, context, activator))
                            return false;
                    }
                }
            }
        }
    }
    else
    {
        for(i = 0; i < numlines; ++i)
        {
            iter = (Line *)P_ToPtr(DMU_LINE, i);
            if(reftype == LREF_ALL)
            {
                if(!func(iter, true, data, context, activator))
                    return false;
            }
            else if(reftype == LREF_ACT_TAGGED)
            {
                xline_t *xl = P_ToXLine(iter);

                if(xl->xg && xl->xg->info.actTag == ref)
                {
                    if(!func(iter, true, data, context, activator))
                        return false;
                }
            }
        }
    }
    return true;
}

/**
 * @return  Value as determined by the reference type from the specified line, using data
 *          from either the line itself or context (will always be linetype_t).
 */
int XL_ValidateLineRef(Line *line, int reftype, void * /*context*/, const char *parmname)
{
    LOG_AS("XL_ValidateLineRef");

    int answer = 0;
    Side *side;

    switch(reftype)
    {
    case LDREF_ID: // Line ID.
        answer = P_ToIndex(line);
        LOG_MAP_MSG_XGDEVONLY2("Using Line ID (%i) as %s", answer << parmname);
        break;

    case LDREF_SPECIAL: // Line Special.
        answer = P_ToXLine(line)->special;
        LOG_MAP_MSG_XGDEVONLY2("Using Line Special (%i) as %s", answer << parmname);
        break;

    case LDREF_TAG: // Line Tag.
        answer = P_ToXLine(line)->tag;
        LOG_MAP_MSG_XGDEVONLY2("Using Line Tag (%i) as %s", answer << parmname);
        break;

    case LDREF_ACTTAG: // Line ActTag.
        if(!P_ToXLine(line)->xg)
        {
            LOG_MAP_MSG_XGDEVONLY("REFERENCE NOT AN XG LINE");
            break;
        }

        if(!P_ToXLine(line)->xg->info.actTag)
        {
            LOG_MAP_MSG_XGDEVONLY("REFERENCE DOESNT HAVE AN ACT TAG");
            break;
        }

        answer = P_ToXLine(line)->xg->info.actTag;
        LOG_MAP_MSG_XGDEVONLY2("Using Line ActTag (%i) as %s", answer << parmname);
        break;

    case LDREF_COUNT: // Line count.
        if(!P_ToXLine(line)->xg)
        {
            LOG_MAP_MSG_XGDEVONLY("REFERENCE NOT AN XG LINE");
            break;
        }

        answer = P_ToXLine(line)->xg->info.actCount;
        LOG_MAP_MSG_XGDEVONLY2("Using Line Count (%i) as %s", answer << parmname);
        break;

    case LDREF_ANGLE: { // Line angle.
        coord_t d1[2];

        P_GetDoublepv(line, DMU_DXY, d1);
        answer = M_PointXYToAngle2(0, 0, d1[0], d1[1]) / (float) ANGLE_MAX * 360;
        LOG_MAP_MSG_XGDEVONLY2("Using Line Angle (%i) as %s", answer << parmname);
        break; }

    case LDREF_LENGTH: // Line length.
        // Answer should be in map units.
        answer = P_GetFixedp(line, DMU_LENGTH) >> FRACBITS;

        LOG_MAP_MSG_XGDEVONLY2("Using Line Length (%i) as %s", answer << parmname);
        break;

    case LDREF_TOP_OFFSETX:
        // Can this ever fail?
        side = (Side *)P_GetPtrp(line, DMU_FRONT);
        if(!side)
        {
            LOG_MAP_MSG_XGDEVONLY("REFERENCE MISSING FRONT SIDE!");
            break;
        }

        answer = P_GetIntp(side, DMU_TOP_MATERIAL_OFFSET_X);

        LOG_MAP_MSG_XGDEVONLY2("Using Line Top X Offset (%i) as %s", answer << parmname);
        break;

    case LDREF_TOP_OFFSETY:
        // Can this ever fail? (yes -dj)
        side = (Side *)P_GetPtrp(line, DMU_FRONT);
        if(!side)
        {
            LOG_MAP_MSG_XGDEVONLY("REFERENCE MISSING FRONT SIDE!");
            break;
        }

        answer = P_GetIntp(side, DMU_TOP_MATERIAL_OFFSET_Y);

        LOG_MAP_MSG_XGDEVONLY2("Using Line Top Y Offset (%i) as %s", answer << parmname);
        break;

    case LDREF_MIDDLE_OFFSETX:
        // Can this ever fail? (yes -dj)
        side = (Side *)P_GetPtrp(line, DMU_FRONT);
        if(!side)
        {
            LOG_MAP_MSG_XGDEVONLY("REFERENCE MISSING FRONT SIDE!");
            break;
        }

        answer = P_GetIntp(side, DMU_MIDDLE_MATERIAL_OFFSET_X);

        LOG_MAP_MSG_XGDEVONLY2("Using Line Middle X Offset (%i) as %s", answer << parmname);
        break;

    case LDREF_MIDDLE_OFFSETY:
        // Can this ever fail? (yes -dj)
        side = (Side *)P_GetPtrp(line, DMU_FRONT);
        if(!side)
        {
            LOG_MAP_MSG_XGDEVONLY("REFERENCE MISSING FRONT SIDE!");
            break;
        }

        answer = P_GetIntp(side, DMU_MIDDLE_MATERIAL_OFFSET_Y);

        LOG_MAP_MSG_XGDEVONLY2("Using Line Middle Y Offset (%i) as %s", answer << parmname);
        break;

    case LDREF_BOTTOM_OFFSETX:
        // Can this ever fail? (yes -dj)
        side = (Side *)P_GetPtrp(line, DMU_FRONT);
        if(!side)
        {
            LOG_MAP_MSG_XGDEVONLY("REFERENCE MISSING FRONT SIDE!");
            break;
        }

        answer = P_GetIntp(side, DMU_BOTTOM_MATERIAL_OFFSET_X);

        LOG_MAP_MSG_XGDEVONLY2("Using Line Bottom X Offset (%i) as %s", answer << parmname);
        break;

    case LDREF_BOTTOM_OFFSETY:
        // Can this ever fail? (yes -dj)
        side = (Side *)P_GetPtrp(line, DMU_FRONT);
        if(!side)
        {
            LOG_MAP_MSG_XGDEVONLY("REFERENCE MISSING FRONT SIDE!");
            break;
        }

        answer = P_GetIntp(side, DMU_BOTTOM_MATERIAL_OFFSET_Y);

        LOG_MAP_MSG_XGDEVONLY2("Using Line Bottom Y Offset (%i) as %s", answer << parmname);
        break;

    default:    // Could be explicit, return the actual int value
        answer = reftype;
        break;
    }

    return answer;
}

/**
 * Executes the lines' function as defined by its class.
 */
void XL_DoFunction(linetype_t *info, Line *line, int sideNum, mobj_t *actThing, int evType)
{
    DE_ASSERT(info && line);
    DE_ASSERT(info->lineClass >= 0 && info->lineClass < NUMXGCLASSES);
    LOG_AS("XL_DoFunction");

    xgclass_t *xgClass = &xgClasses[info->lineClass];

    LOG_MAP_MSG_XGDEVONLY2("Line %i, side %i, activator id %i, event %s",
            P_ToIndex(line) << sideNum << (actThing ? actThing->thinker.id : 0)
            << EVTYPESTR(evType));
    LOG_MAP_MSG_XGDEVONLY2("Executing class: %s (0x%X)...", xgClass->className << info->lineClass);

    // Does this class only work with certain events?
    if(xgClass->evTypeFlags > 0)
    {
        if(!(xgClass->evTypeFlags & evType))
        {
            LOG_MAP_MSG_XGDEVONLY2("THIS CLASS DOES NOT SUPPORT %s EVENTS!", EVTYPESTR(evType));
            return;
        }
    }

    // Does this class have an init function?
    if(xgClass->initFunc)
        xgClass->initFunc(line);

    // Does this class have a do function?
    if(xgClass->doFunc)
    {
        // Do we need to traverse?
        switch(xgClass->traverse)
        {
            case TRAV_NONE: // No need for traversal, call the doFunc
                de::function_cast<LineTraverserFunc>(xgClass->doFunc)(line, true, line, info, actThing);
                break;
            case TRAV_LINES: // Traverse lines, executing doFunc for each
                XL_TraverseLines(line, info->iparm[xgClass->travRef],
                                 info->iparm[xgClass->travData], line,
                                 info, actThing, de::function_cast<LineTraverserFunc>(xgClass->doFunc));
                break;
            case TRAV_PLANES: // Traverse planes, executing doFunc for each
            case TRAV_SECTORS:
                XL_TraversePlanes(line, info->iparm[xgClass->travRef],
                                  info->iparm[xgClass->travData], line,
                                  info, xgClass->traverse == TRAV_SECTORS? true : false,
                                  actThing, de::function_cast<PlaneTraverserFunc>(xgClass->doFunc));
                break;
        }
    }
}

int XLTrav_QuickActivate(Line *line, dd_bool /*ceiling*/, void *context, void * /*context2*/,
    mobj_t * /*activator*/)
{
    if(line)
    {
        xline_t *xline = P_ToXLine(line);
        if(xline->xg)
        {
            xline->xg->active = (context? true : false);
            xline->xg->timer  = XLTIMER_STOPPED; // Stop timer.
        }
    }

    return true; // Continue iteration.
}

/**
 * @return  @c true, if the line is active.
 */
int XLTrav_CheckLine(Line *line, dd_bool /*ceiling*/, void *context,
    void * /*context2*/, mobj_t * /*activator*/)
{
    if(line)
    {
        xline_t *xline = P_ToXLine(line);

        if(!xline->xg)
        {
            return false; // Stop checking!
        }

        return CPP_BOOL(xline->xg->active) == (context? true : false);
    }

    return true; // Continue iteration.
}

/**
 * @param data  If @c true, the line will receive a chain event if inactive.
 *              If @c false, the line will receive a chain event if active.
 */
int XLTrav_SmartActivate(Line *line, dd_bool /*ceiling*/, void *context,
    void *context2, mobj_t * /*activator*/)
{
    if(line)
    {
        xline_t *xline = P_ToXLine(line);

        if(xline->xg)
        {
            if(CPP_BOOL(xline->xg->active) != (context? true : false))
            {
                XL_LineEvent(XLE_CHAIN, 0, line, 0, context2);
            }
        }
    }

    return true; // Continue iteration.
}

//
// XG Line Type Classes which don't require traversal
//

int XL_DoChainSequence(Line *line, dd_bool /*ceiling*/, void * /*context*/,
    void *context2, mobj_t * /*activator*/)
{
    if(line)
    {
        xline_t *xline = P_ToXLine(line);

        if(xline->xg)
        {
            linetype_t *info = static_cast<linetype_t *>(context2);

            xline->xg->chIdx = 1; // This is the first.
            // Start counting the first interval.
            xline->xg->chTimer =
                XG_RandomPercentFloat(info->fparm[1], info->fparm[0]);
        }
    }

    return true;
}

int XL_DoDamage(Line * /*line*/, dd_bool /*ceiling*/, void * /*context*/, void *context2,
    mobj_t *activator)
{
    linetype_t *info = static_cast<linetype_t *>(context2);

    if(!activator)
    {
/*        if(evtype == XLE_FUNC)
            LOG_MAP_MSG_XGDEVONLY("Sector Functions don't have activators! Use a Sector Chain instead");
        else*/
            LOG_MAP_MSG_XGDEVONLY("No activator! Can't damage anything");

        return false;
    }

    if(activator->health > info->iparm[2])
    {
        // Iparms define the min and max damage to inflict.
        // The real amount is random.
        int i = XG_RandomInt(info->iparm[0], info->iparm[1]);
        if(i > 0)
        {
            P_DamageMobj(activator, 0, 0, i, false);
        }
        else if(i < 0 && activator->health < info->iparm[3])
        {
            int origHealth = activator->health;
            // Don't go above a given level.
            activator->health = MIN_OF(activator->health-i, info->iparm[3]);
            // Need to signal an update?
            if(activator->player && activator->health != origHealth)
            {
                activator->player->health = activator->health;
                activator->player->update |= PSF_HEALTH;
            }
        }
    }

    return true;
}

int XL_DoPower(Line * /*line*/, dd_bool /*ceiling*/, void * /*context*/,
    void *context2, mobj_t *activator)
{
    linetype_t *info = static_cast<linetype_t *>(context2);
    DE_ASSERT(info);
    player_t *player = activator? activator->player : nullptr;

    // Only players have armor.
    if(!player)
    {
/*        if(evtype == XLE_FUNC)
            LOG_MAP_MSG_XGDEVONLY("Sector Functions don't have activators! Use a Sector Chain instead");
        else*/
            LOG_MAP_MSG_XGDEVONLY("Activator MUST be a player...");

        return false;
    }

    int delta = XG_RandomInt(info->iparm[0], info->iparm[1]);
    if(delta > 0)
    {
        if(player->armorPoints + delta >= info->iparm[3])
            delta = de::max(0, info->iparm[3] - player->armorPoints);
    }
    else
    {
        if(player->armorPoints + delta <= info->iparm[2])
            delta = de::min(0, info->iparm[2] - player->armorPoints);
    }

    if(delta)
    {
        if(!player->armorType)
            P_PlayerSetArmorType(player, 1);
        P_PlayerGiveArmorBonus(player, delta);
    }

    return true;
}

int XL_DoKey(Line * /*line*/, dd_bool /*ceiling*/, void * /*context*/,
    void *context2, mobj_t *activator)
{
    linetype_t *info = static_cast<linetype_t *>(context2);
    player_t *player = (activator? activator->player : 0);

    if(!player)  // Must be a player.
    {
/*        if(evtype == XLE_FUNC)
            LOG_MAP_MSG_XGDEVONLY("Sector Functions don't have activators! Use a Sector Chain instead");
        else*/
            LOG_MAP_MSG_XGDEVONLY("Activator MUST be a player...");

        return false;
    }

    for(int i = 0; i < NUM_KEY_TYPES; ++i)
    {
        if(info->iparm[0] & (1 << i))
            P_GiveKey(player, keytype_t(i));
        if(info->iparm[1] & (1 << i))
            player->keys[i] = false;
    }

    return true;
}

int XL_DoExplode(Line * /*line*/, dd_bool /*ceiling*/, void * /*context*/,
    void * /*context2*/, mobj_t *activator)
{
    if(!activator)
    {
/*        if(evtype == XLE_FUNC)
            LOG_MAP_MSG_XGDEVONLY("Sector Functions don't have activators! Use a Sector Chain instead");
        else*/
            LOG_MAP_MSG_XGDEVONLY("No activator! Can't explode anything");
        return false;
    }

    P_ExplodeMissile(activator);
    return true;
}

int XL_DoCommand(Line * /*line*/, dd_bool /*ceiling*/, void * /*context*/,
    void *context2, mobj_t * /*activator*/)
{
    linetype_t *info = static_cast<linetype_t *>(context2);

    DD_Execute(true, info->sparm[0]);
    return true;
}

//
// Following classes require traversal hence "Trav_"
//

int XLTrav_ChangeLineType(Line *line, dd_bool /*ceiling*/, void * /*context*/,
    void *context2, mobj_t * /*activator*/)
{
    if(line)
    {
        linetype_t *info = static_cast<linetype_t *>(context2);
        XL_SetLineType(line, info->iparm[2]);
    }

    return true; // Keep looking.
}

static Side *lineSideIfSector(Line &line, bool back = false)
{
    if(P_GetPtrp(&line, back? DMU_BACK_SECTOR : DMU_FRONT_SECTOR))
    {
        return (Side *)P_GetPtrp(&line, back? DMU_BACK : DMU_FRONT);
    }
    return nullptr;
}

int XLTrav_ChangeWallMaterial(Line *line, dd_bool /*ceiling*/, void * /*context*/,
    void *context2, mobj_t * /*activator*/)
{
    LOG_AS("XLTrav_ChangeWallMaterial");

    linetype_t *info = static_cast<linetype_t *>(context2);
    if(!line) return true; // Continue iteration.

    // i2: sidenum
    // i3: top material (zero if no change)
    // i4: mid material (zero if no change, -1 to remove)
    // i5: bottom material (zero if no change)
    // i6: (true/false) set midtexture even if previously zero
    // i7: sdf_* flags
    // i8: mid blendmode
    // i9: top red
    // i10: top green
    // i11: top blue
    // i12: mid red
    // i13: mid green
    // i14: mid blue
    // i15: mid alpha
    // i16: bottom red
    // i17: bottom green
    // i18: bottom blue

    Side *side = lineSideIfSector(*line, CPP_BOOL(info->iparm[2]));
    if(!side) return true; // Continue iteration.

    LOG_MAP_MSG_XGDEVONLY2("Line %i", P_ToIndex(line));

    XL_ChangeMaterial(line, info->iparm[2], LWS_UPPER,
                      (world_Material *)P_ToPtr(DMU_MATERIAL, info->iparm[3]),
                      BM_NORMAL, Vec3f(info->iparm[9], info->iparm[10], info->iparm[11]) / 255.f,
                      info->iparm[7]);

    world_Material *mat = 0;
    if(info->iparm[4] && (P_GetPtrp(side, DMU_MIDDLE_MATERIAL) || info->iparm[6]))
    {
        if(!P_GetPtrp(line, DMU_BACK_SECTOR) && info->iparm[4] == -1)
            mat = 0;
        else
            mat = (world_Material *)P_ToPtr(DMU_MATERIAL, info->iparm[4]);
    }

    XL_ChangeMaterial(line, info->iparm[2], LWS_MID, mat,
                      blendmode_t(info->iparm[8]),
                      Vec4f(info->iparm[12], info->iparm[13], info->iparm[14], info->iparm[15]) / 255.f,
                      info->iparm[7]);

    XL_ChangeMaterial(line, info->iparm[2], LWS_LOWER,
                      (world_Material *)P_ToPtr(DMU_MATERIAL, info->iparm[5]),
                      BM_NORMAL, Vec3f(info->iparm[16], info->iparm[17], info->iparm[18]) / 255.f,
                      info->iparm[7]);

    return true;
}

int XLTrav_Activate(Line *line, dd_bool /*ceiling*/, void * /*context*/,
    void * /*context2*/, mobj_t *activator)
{
    if(line)
    {
        XL_LineEvent(XLE_CHAIN, 0, line, 0, activator);
    }
    return true; // Keep looking.
}

int XLTrav_LineCount(Line *line, dd_bool /*ceiling*/, void * /*context*/,
    void *context2, mobj_t * /*activator*/)
{
    linetype_t *info = static_cast<linetype_t *>(context2);

    if(line)
    {
        xline_t *xline = P_ToXLine(line);
        if(xline->xg)
        {
            if(info->iparm[2])
            {
                xline->xg->info.actCount = info->iparm[3];
            }
            else
            {
                xline->xg->info.actCount += info->iparm[3];
            }
        }
    }

    return true;
}

int XLTrav_Music(Line *line, dd_bool /*ceiling*/, void * /*context*/,
    void *context2, mobj_t * /*activator*/)
{
    LOG_AS("XLTrav_Music");

    linetype_t *info = static_cast<linetype_t *>(context2);

    int song = 0;
    int temp = 0;

    if(info->iparm[2] == LREF_NONE)
    {    // The old format, use ip0 explicitly.
        song = info->iparm[0];
    }
    else // We might possibly have a data reference to evaluate.
    {
        if(info->iparm[2] == LREF_NONE) // (ip0) will be used to determine next map.
        {
            song = info->iparm[0];
        }
        else if(line)
        {
            // Evaluate ip0 for a data reference.
            temp = XL_ValidateLineRef(line, info->iparm[0], context2, "Music ID");
            if(!temp)
            {
                LOG_MAP_MSG_XGDEVONLY("Reference data not valid. Song not changed");
            }
            else
            {
                song = temp;
            }
        }
    }

    //// @todo Add code to validate song id here.

    if(song)
    {
        LOG_MAP_MSG_XGDEVONLY2("Play Music ID (%i)%s",
                song << (info->iparm[1]? " looped" : ""));
        S_StartMusicNum(song, info->iparm[1]);
    }

    return false; // Only do this once!
}

int XLTrav_LineTeleport(Line *newLine, dd_bool /*ceiling*/, void *context,
    void *context2, mobj_t *mobj)
{
// Maximum units to move object to avoid hiccups.
#define FUDGEFACTOR         10

    LOG_AS("XLTrav_LineTeleport");

    int fudge = FUDGEFACTOR;
    int side = 0, stepDown;
    uint an;
    mobj_t *flash;
    Line *line = (Line *) context;
    linetype_t *info = (linetype_t *) context2;
    Vertex /* *newV1,*/ *newV2, *oldV1/*, *oldV2*/;
    Sector *newFrontSec, *newBackSec;
    coord_t newPos[3], pos, s, c;
    coord_t oldLineDelta[2], newLineDelta[2];
    angle_t angle;

    // Don't teleport things marked noteleport!
    if(mobj->flags2 & MF2_NOTELEPORT)
    {
        LOG_MAP_MSG_XGDEVONLY("Activator can't be teleported (THING is unteleportable)");
        return false; // No point continuing...
    }

    if(!line)
        return true; // Continue iteration.

    // We shouldn't be trying to teleport to the same line
    if(newLine == line)
    {
        LOG_MAP_MSG_XGDEVONLY("Target == Origin. Continuing search...");
        return true; // Keep looking
    }

    // Retrieve a few properties to make this look neater.
    oldV1 = (Vertex *)P_GetPtrp(line, DMU_VERTEX0);
    //oldV2 = (Vertex *)P_GetPtrp(line, DMU_VERTEX1);
    P_GetDoublepv(line, DMU_DXY, oldLineDelta);

    //newV1 = (Vertex *)P_GetPtrp(newLine, DMU_VERTEX0);
    newV2 = (Vertex *)P_GetPtrp(newLine, DMU_VERTEX1);
    P_GetDoublepv(newLine, DMU_DXY, newLineDelta);
    newFrontSec = (Sector *)P_GetPtrp(newLine, DMU_FRONT_SECTOR);
    newBackSec  = (Sector *)P_GetPtrp(newLine, DMU_BACK_SECTOR);

    // i2: 1 = Spawn Fog
    // i3: Sound = Sound to play
    // i4: 1 = reversed
    // i5: 1 = always telestomp

    LOG_MAP_MSG_XGDEVONLY2("%s, %s, %s",
           (   info->iparm[2]? "Spawn Flash" : "No Flash")
           << (info->iparm[3]? "Play Sound"  : "Silent")
           << (info->iparm[4]? "Reversed"    : "Normal."));

    // Spawn flash at the old position?
    if(info->iparm[2])
    {
        if((flash = P_SpawnMobj(MT_TFOG, mobj->origin, mobj->angle + ANG180, 0)))
        {
            // Play a sound?
            if(info->iparm[3])
                S_StartSound(info->iparm[3], flash);
        }
    }

    // Get the thing's position along the source line
    if(fabs(oldLineDelta[0]) > fabs(oldLineDelta[1]))
        pos = (mobj->origin[VX] - P_GetDoublep(oldV1, DMU_X)) / oldLineDelta[0];
    else
        pos = (mobj->origin[VY] - P_GetDoublep(oldV1, DMU_Y)) / oldLineDelta[1];

    // Get the angle between the two lines, for rotating orientation and
    // momentum. Rotate 180 degrees, and flip the position across the exit
    // line, if reversed.
    angle = (info->iparm[4] ? pos = 1 - pos, 0 : ANG180) +
             M_PointXYToAngle2(0, 0, newLineDelta[0], newLineDelta[1]) -
             M_PointXYToAngle2(0, 0, oldLineDelta[0], oldLineDelta[1]);

    // Interpolate position across the exit line.
    newPos[VX] = P_GetDoublep(newV2, DMU_X) - (pos * newLineDelta[0]);
    newPos[VY] = P_GetDoublep(newV2, DMU_Y) - (pos * newLineDelta[1]);

    // Sine, cosine of angle adjustment
    s = FIX2FLT(finesine[angle >> ANGLETOFINESHIFT]);
    c = FIX2FLT(finecosine[angle >> ANGLETOFINESHIFT]);

    // Whether walking towards first side of exit line steps down
    if(P_GetDoublep(newFrontSec, DMU_FLOOR_HEIGHT) <
       P_GetDoublep(newBackSec, DMU_FLOOR_HEIGHT))
        stepDown = true;
    else
        stepDown = false;

    // Height of thing above ground.
    newPos[VZ] = mobj->origin[VZ] - mobj->floorZ;

    /**
     * Side to exit the line on positionally.
     *
     * @note
     * This flag concerns exit position, not momentum. Due to potential for
     * round-off error, the thing can land on either the left or the right
     * side of the exit line, and steps must be taken to make sure it
     * does not end up on the wrong side.
     *
     * Exit momentum is always towards side 1 in a reversed teleporter, and
     * always towards side 0 otherwise.
     *
     * Exiting positionally on side 1 is always safe, as far as avoiding
     * oscillations and stuck-in-wall problems, but may not be optimum for
     * non-reversed teleporters.
     *
     * Exiting on side 0 can cause oscillations if momentum is towards side
     * 1, as it is with reversed teleporters.
     *
     * Exiting on side 1 slightly improves player viewing when going down a
     * step on a non-reversed teleporter.
     */

    if(!info->iparm[4] || (mobj->player && stepDown))
        side = 1;

    // Make sure we are on correct side of exit line.
    while((Line_PointOnSide(newLine, newPos) < 0) != side && --fudge >= 0)
    {
        if(fabs(newLineDelta[0]) > fabs(newLineDelta[1]))
            newPos[VY] -= FIX2FLT(((newLineDelta[0] < 0) != side)? -1 : 1);
        else
            newPos[VX] += FIX2FLT(((newLineDelta[1] < 0) != side)? -1 : 1);
    }

    // Do the Teleport
    if(!P_TeleportMove(mobj, newPos[VX], newPos[VY], (info->iparm[5] > 0? true : false)))
    {
        LOG_MAP_MSG_XGDEVONLY("Something went horribly wrong... aborting.");
        return false;
    }

    // Adjust z position to be same height above ground as before. Ground
    // level at the exit is measured as the higher of the two floor heights
    // at the exit line.
    if(stepDown)
        mobj->origin[VZ] = newPos[VZ] + P_GetDoublep(newFrontSec, DMU_FLOOR_HEIGHT);
    else
        mobj->origin[VZ] = newPos[VZ] + P_GetDoublep(newBackSec, DMU_FLOOR_HEIGHT);

    // Rotate mobj's orientation according to difference in line angles.
    mobj->angle += angle;

    // Update momentum of mobj crossing teleporter line?
    newPos[VX] = mobj->mom[MX];
    newPos[VY] = mobj->mom[MY];

    // Rotate mobj's momentum to come out of exit just like it entered.
    mobj->mom[MX] = (newPos[VX] * c) - (newPos[VY] * s);
    mobj->mom[MY] = (newPos[VY] * c) + (newPos[VX] * s);

    // Feet clipped?
    if(mobj->flags2 & MF2_FLOORCLIP)
    {
        mobj->floorClip = 0;

        if(FEQUAL(mobj->origin[VZ], P_GetDoublep(Mobj_Sector(mobj), DMU_FLOOR_HEIGHT)))
        {
            const terraintype_t *tt = P_MobjFloorTerrain(mobj);
            if(tt->flags & TTF_FLOORCLIP)
            {
                mobj->floorClip = 10;
            }
        }
    }

    // Spawn flash at the new position?
    if (!info->iparm[2])
    {
        an = mobj->angle >> ANGLETOFINESHIFT;
        if((flash = P_SpawnMobjXYZ(MT_TFOG,
                                   mobj->origin[VX] + 24 * FIX2FLT(finecosine[an]),
                                   mobj->origin[VY] + 24 * FIX2FLT(finesine[an]),
                                   mobj->origin[VZ], mobj->angle + ANG180, 0)))
        {
            // Play a sound?
            if(info->iparm[3])
                S_StartSound(info->iparm[3], flash);
        }
    }

    // Adjust the player's view, incase there has been a height change
    if(mobj->player)
    {
        mobj->player->viewZ = mobj->origin[VZ] + mobj->player->viewHeight;
        mobj->dPlayer->flags |= DDPF_FIXANGLES | DDPF_FIXORIGIN | DDPF_FIXMOM;
    }

    return false; // Do this only once!

#undef FUDGEFACTOR
}

int XLTrav_LeaveMap(Line *line, dd_bool /*ceiling*/, void * /*context*/, void *context2,
                    mobj_t * /*activator*/)
{
    LOG_AS("XLTrav_LeaveMap");

    linetype_t *info = static_cast<linetype_t *>(context2);

    // Is this a secret exit?
    if(info->iparm[0] > 0)
    {
        G_SetGameActionMapCompleted(gfw_Session()->mapUriForNamedExit("secret"), 0, true);
        return false;
    }

    res::Uri newMapUri;
    if(info->iparm[1] == LREF_NONE)
    {
        // (ip3) will be used to determine next map (1-based).
        if(info->iparm[3])
        {
            newMapUri = G_ComposeMapUri(gfw_Session()->episodeId().toInt() - 1, info->iparm[3] - 1);
            LOG_MAP_MSG_XGDEVONLY2("Next map set to \"%s\"", newMapUri);
        }
    }
    // We might possibly have a data reference to evaluate.
    else if(line)
    {
        const int oldMapNumber = XL_ValidateLineRef(line, info->iparm[3], context2, "Map Number");
        if(oldMapNumber > 0)
        {
            newMapUri = G_ComposeMapUri(gfw_Session()->episodeId().toInt() - 1, oldMapNumber - 1);
        }
    }

    if(newMapUri.isEmpty())
    {
        newMapUri = gfw_Session()->mapUriForNamedExit("next");
        LOG_MAP_MSG_XGDEVONLY("Next map set to default for the 'next' exit");
    }

    // Check that the map truly exists.
    if(!P_MapExists(newMapUri.compose()))
    {
        // Backward compatibility dictates that invalid refs be interpreted to mean the start map
        // of the current episode (which is known to always exist).
        newMapUri = res::makeUri(gfw_Session()->episodeDef()->gets("startMap"));
    }

    G_SetGameActionMapCompleted(newMapUri);
    return false; // Only do this once!
}

int XLTrav_DisableLine(Line *line, dd_bool /*ceiling*/, void *context,
    void * /*context2*/, mobj_t * /*activator*/)
{
    if(line)
    {
        xline_t *xline = P_ToXLine(line);

        if(xline->xg)
        {
            xline_t *origLine = P_ToXLine((Line *) context);

            xline->xg->disabled = origLine->xg->active;
        }
    }

    return true; // Keep looking...
}

int XLTrav_EnableLine(Line *line, dd_bool /*ceiling*/, void *context,
    void * /*context2*/, mobj_t * /*activator*/)
{
    if(line)
    {
        xline_t *xline = P_ToXLine(line);
        if(xline->xg)
        {
            xline_t *origLine = P_ToXLine((Line*) context);

            xline->xg->disabled = !origLine->xg->active;
        }
    }

    return true; // Keep looking...
}

/**
 * Checks if the given lines are active or inactive.
 *
 * @return  @c true if all are in the specified state.
 */
dd_bool XL_CheckLineStatus(Line *line, int reftype, int ref, int active,
                           mobj_t *activator)
{
    return XL_TraverseLines(line, reftype, ref, &active, 0, activator, XLTrav_CheckLine);
}

int XL_CheckMobjGone(thinker_t *th, void *context)
{
    LOG_AS("XL_CheckMobjGone");

    mobj_t *mo    = (mobj_t *) th;
    int thingtype = *static_cast<int *>(context);

    if(mo->type == thingtype && mo->health > 0)
    {
        // Not dead.
        LOG_MAP_MSG_XGDEVONLY2("Thing type %i: Found mo id=%i, health=%i, pos=%s",
               thingtype << mo->thinker.id << mo->health << Vec4d(mo->origin).asText());
        return true; // Stop iteration.
    }

    return false; // Continue iteration.
}

void XL_SwapSwitchTextures(Line *line, int snum)
{
    LOG_AS("XL_SwapSwitchTextures");

    if(line)
    {
        Side *side = (Side *)P_GetPtrp(line, snum? DMU_BACK : DMU_FRONT);

        if(side && P_ToggleSwitch(side, SFX_NONE, true, 0))
        {
            LOG_MAP_MSG_XGDEVONLY2("Line %i, side %i",
                   P_ToIndex(line) << P_ToIndex(side));
        }
    }
}

/**
 * Changes material of the given line.
 */
void XL_ChangeMaterial(Line *line, int sidenum, int section, world_Material *mat,
    blendmode_t blendmode, const Vec4f &tintColor, int flags)
{
    Side *side = (Side *)P_GetPtrp(line, sidenum? DMU_BACK:DMU_FRONT);
    if(!side) return;

    LOG_MAP_MSG_XGDEVONLY2("Line:%i side:%i section:%i material:%i tintColor:%s blendmode:%i",
           P_ToIndex(line) << sidenum << section << P_ToIndex(mat) << tintColor.asText() << blendmode);

    // Which wall section are we working on?
    if(section == LWS_MID)
    {
        // Are we removing the middle texture?
        if(mat == (world_Material *) -1)
        {
            P_SetPtrp(side, DMU_MIDDLE_MATERIAL, NULL);
        }
        else if(mat)
        {
            P_SetPtrp(side, DMU_MIDDLE_MATERIAL, mat);
        }

        // Are we changing the blendmode?
        if(blendmode)
        {
            P_SetIntp(side, DMU_MIDDLE_BLENDMODE, blendmode);
        }

        // Are we changing the surface color?
        for(int i = 0; i < 4; ++i)
        {
            if(!de::fequal(tintColor[i], 0))
                P_SetFloatp(side, TO_DMU_MIDDLE_COLOR(i), tintColor[i]);
        }
    }
    else if(section == LWS_UPPER)
    {
        if(mat)
        {
            P_SetPtrp(side, DMU_TOP_MATERIAL, mat);
        }

        for(int i = 0; i < 3; ++i)
        {
            if(!de::fequal(tintColor[i], 0))
                P_SetFloatp(side, TO_DMU_TOP_COLOR(i), tintColor[i]);
        }
    }
    else if(section == LWS_LOWER)
    {
        if(mat)
        {
            P_SetPtrp(side, DMU_BOTTOM_MATERIAL, mat);
        }

        for(int i = 0; i < 3; ++i)
        {
            if(!de::fequal(tintColor[i], 0))
                P_SetFloatp(side, TO_DMU_BOTTOM_COLOR(i), tintColor[i]);
        }
    }

    // Adjust the side's flags
    P_SetIntp(side, DMU_FLAGS, P_GetIntp(side, DMU_FLAGS) | flags);
}

void XL_Message(mobj_t *act, char *msg, dd_bool global)
{
    LOG_AS("XL_Message");

    player_t *pl;
    int i;

    if(!msg || !msg[0]) return;

    if(global)
    {
        LOG_MAP_MSG_XGDEVONLY2("GLOBAL '%s'", msg);
        // Send to all players in the game.
        for(i = 0; i < MAXPLAYERS; ++i)
        {
            if(players[i].plr->inGame)
                P_SetMessage(&players[i], msg);
        }
        return;
    }

    if(act->player)
    {
        pl = act->player;
    }
    else if((act->flags & MF_MISSILE) && act->target && act->target->player)
    {
        // Originator of the missile.
        pl = act->target->player;
    }
    else
    {
        // We don't know whom to send the message.
        LOG_MAP_MSG_XGDEVONLY2("'%s'\nNO DESTINATION, MESSAGE DISCARDED", msg);
        return;
    }
    P_SetMessage(pl, msg);
}

void XL_ActivateLine(dd_bool activating, linetype_t *info, Line *line, int sidenum,
                     mobj_t *activator, int evtype)
{
    DE_ASSERT(line);
    LOG_AS("XL_ActivateLine");

    xline_t *xline = P_ToXLine(line);
    if(!xline) return; // huh?

    LOG_MAP_MSG_XGDEVONLY2("%s line %i, side %i, type %i",
           (activating? "Activating" : "Deactivating") << P_ToIndex(line)
           << sidenum << xline->special);

    DE_ASSERT(xline->xg);
    xgline_t &xgline = *xline->xg;
    if(xgline.disabled)
    {
        LOG_MAP_MSG_XGDEVONLY("LINE DISABLED, ABORTING");
        return; // The line is disabled.
    }

    if((activating && xgline.active) || (!activating && !xgline.active))
    {
        LOG_MAP_MSG_XGDEVONLY2("Line is ALREADY %s, ABORTING", (activating ? "ACTIVE" : "INACTIVE"));
        return; // Do nothing (can't activate if already active!).
    }

    // Activation should happen on the front side.
    // Let the line know who's activating it.
    xgline.activator = activator;

    // Process (de)activation chains. Chains always pass as an activation
    // method, but the other requirements of the chained type must be met.
    if(activating && info->actChain)
    {
        LOG_MAP_MSG_XGDEVONLY2("Line has Act Chain (type %i) - It will be processed first...", info->actChain);
        XL_LineEvent(XLE_CHAIN, info->actChain, line, sidenum, activator);
    }
    else if(!activating && info->deactChain)
    {
        LOG_MAP_MSG_XGDEVONLY2("Line has Deact Chain (type %i) - It will be processed first...", info->deactChain);
        XL_LineEvent(XLE_CHAIN, info->deactChain, line, sidenum, activator);
    }

    // Automatically swap any SW* textures.
    if(xgline.active != activating)
    {
        XL_SwapSwitchTextures(line, sidenum);
    }

    // Change the state of the line.
    xgline.active = activating;
    xgline.timer  = 0; // Reset timer.

    // Activate lines with a matching tag with Group Activation.
    if( (activating && (info->flags2 & LTF2_GROUP_ACT)) ||
       (!activating && (info->flags2 & LTF2_GROUP_DEACT)))
    {
        XL_TraverseLines(line, LREF_LINE_TAGGED, true, &activating, 0, activator,
                         XLTrav_SmartActivate);
    }

    // For lines flagged Multiple, quick-(de)activate other lines that have
    // the same line tag.
    if(info->flags2 & LTF2_MULTIPLE)
    {
        XL_TraverseLines(line, LREF_LINE_TAGGED, true, &activating, 0, activator,
                         XLTrav_QuickActivate);
    }

    // Should we apply the function of the line? Functions are defined by
    // the class of the line type.
    if(  (activating && (info->flags2 & LTF2_WHEN_ACTIVATED)) ||
        (!activating && (info->flags2 & LTF2_WHEN_DEACTIVATED)))
    {
        if(!(info->flags2 & LTF2_WHEN_LAST) || info->actCount == 1)
        {
            XL_DoFunction(info, line, sidenum, activator, evtype);
        }
        else
        {
            LOG_MAP_MSG_XGDEVONLY2("Line %i FUNCTION TEST FAILED", P_ToIndex(line));
        }
    }
    else if(activating)
    {
        LOG_MAP_MSG_XGDEVONLY2("Line %i has no activation function", P_ToIndex(line));
    }
    else
    {
        LOG_MAP_MSG_XGDEVONLY2("Line %i has no deactivation function", P_ToIndex(line));
    }

    // Now do any secondary actions that should happen AFTER
    // the function of the line (regardless if one was applied or not)
    if(activating)
    {
        XL_Message(activator, info->actMsg,
                   (info->flags2 & LTF2_GLOBAL_A_MSG) != 0);

        if(info->actSound)
        {
            S_SectorSound((Sector *)P_GetPtrp(line, DMU_FRONT_SECTOR), info->actSound);
        }

        // Change the texture of the line if asked to.
        if(info->wallSection && info->actMaterial != NOMATERIALID)
        {
            XL_ChangeMaterial(line, sidenum, info->wallSection,
                              (world_Material *)P_ToPtr(DMU_MATERIAL, info->actMaterial));
        }

        // Change the class of the line if asked to
        if(info->actLineType)
        {
            XL_SetLineType(line, info->actLineType);
        }
    }
    else
    {
        XL_Message(activator, info->deactMsg, (info->flags2 & LTF2_GLOBAL_D_MSG) != 0);

        if(info->deactSound)
        {
            S_SectorSound((Sector *)P_GetPtrp(line, DMU_FRONT_SECTOR), info->deactSound);
        }

        // Change the texture of the line if asked to.
        if(info->wallSection && info->deactMaterial != NOMATERIALID)
        {
            XL_ChangeMaterial(line, sidenum, info->wallSection,
                              (world_Material *)P_ToPtr(DMU_MATERIAL, info->deactMaterial));
        }

        // Change the class of the line if asked to.
        if(info->deactLineType)
        {
            XL_SetLineType(line, info->deactLineType);
        }
    }
}

/**
 * XL_CheckKeys
 */
dd_bool XL_CheckKeys(mobj_t* mo, int flags2, dd_bool doMsg, dd_bool doSfx)
{
    player_t*           act = mo->player;
#if __JDOOM__ || __JDOOM64__
    int                 num = 6;
    int*                keys = (int *) act->keys;
    int                 badsound = SFX_OOF;
#elif __JHERETIC__
    int                 num = 3;
    dd_bool*            keys = act->keys;
    int                 badsound = SFX_PLROOF;
#elif __JSTRIFE__
//// @todo FIXME!!!
    int                 num = 3;
    int*                keys = (int *) act->keys;
    int                 badsound = SFX_NONE;
#endif
    int                 i;

    for(i = 0; i < num; ++i)
    {
        if((flags2 & LTF2_KEY(i)) && !keys[i])
        {   // This key is missing!

            // Show a message?
            if(doMsg)
            {
                sprintf(msgbuf, "YOU NEED A %s.", GET_TXT(TXT_KEY1 + i));
                XL_Message(mo, msgbuf, false);
            }

            // Play a sound?
            if(doSfx)
                S_ConsoleSound(badsound, mo, act - players);

            return false;
        }
    }

    return true;
}

/**
 * Decides if the event leads to (de)activation.
 * Line must be extended.
 * Most conditions use AND (act method, game mode and difficult use OR).
 *
 * @return              @c true, iff the event is processed.
 */
int XL_LineEvent(int evtype, int linetype, Line* line, int sidenum,
                 void* data)
{
    LOG_AS("XL_LineEvent");

    int                 i;
    xline_t*            xline;
    xgline_t*           xg;
    linetype_t*         info;
    dd_bool             active;
    mobj_t*             activator_thing = (mobj_t *) data;
    player_t*           activator = 0;
    dd_bool             anyTrigger = false;

    // Clients rely on the server, they don't do XG themselves.
    if(IS_CLIENT)
        return false;

    xline = P_ToXLine(line);
    xg = xline->xg;
    info = &xg->info;
    active = xg->active;

    if(activator_thing)
        activator = activator_thing->player;

#if __JDOOM__ || __JDOOM64__
    // BOOM intergration
    if((xline->flags & ML_ALLTRIGGER) && !(info->flags2 & LTF2_OVERRIDE_ANY))
        anyTrigger = true;
#endif

    LOG_MAP_MSG_XGDEVONLY2("%s line %i, side %i (chained type %i)%s",
           EVTYPESTR(evtype) << P_ToIndex(line) << sidenum << linetype
           << (anyTrigger? " ANY Trigger" : ""));

    if(xg->disabled)
    {
        LOG_MAP_MSG_XGDEVONLY("LINE IS DISABLED, ABORTING EVENT");
        return false; // The line is disabled.
    }

    // This is a chained event.
    if(linetype)
    {
        if(!XL_GetType(linetype))
            return false;
        info = &typebuffer;
    }

    // Process chained event first. It takes precedence.
    if(info->evChain)
    {
        if(XL_LineEvent(evtype, info->evChain, line, sidenum, data))
        {
            LOG_MAP_MSG_XGDEVONLY2("Event %s, line %i, side %i OVERRIDDEN BY EVENT CHAIN %i",
                   EVTYPESTR(evtype) << P_ToIndex(line) << sidenum << info->evChain);
            return true;
        }
    }

    // Check restrictions and conditions that will prevent processing
    // the event.
    if((active && info->actType == LTACT_COUNTED_OFF) ||
       (!active && info->actType == LTACT_COUNTED_ON))
    {
        // Can't be processed at this time.
        LOG_MAP_MSG_XGDEVONLY2("Line %i: Active=%i, type=%i ABORTING EVENT",
               P_ToIndex(line) << active << info->actType);
        return false;
    }

    // Check the type of the event vs. the requirements of the line.
    if(evtype == XLE_CHAIN || evtype == XLE_FUNC)
        goto type_passes;
    if(evtype == XLE_USE &&
       ((((info->flags & LTF_PLAYER_USE_A) && activator && !active) ||
         ((info->flags & LTF_OTHER_USE_A) && !activator && !active) ||
         ((info->flags & LTF_PLAYER_USE_D) && activator && active) ||
         ((info->flags & LTF_OTHER_USE_D) && !activator && active)) || anyTrigger))
        goto type_passes;
    if(evtype == XLE_SHOOT &&
       ((((info->flags & LTF_PLAYER_SHOOT_A) && activator && !active) ||
         ((info->flags & LTF_OTHER_SHOOT_A) && !activator && !active) ||
         ((info->flags & LTF_PLAYER_SHOOT_D) && activator && active) ||
         ((info->flags & LTF_OTHER_SHOOT_D) && !activator && active)) || anyTrigger))
        goto type_passes;
    if(evtype == XLE_CROSS &&
       ((((info->flags & LTF_PLAYER_CROSS_A) && activator && !active) ||
         ((info->flags & LTF_MONSTER_CROSS_A) &&
          (activator_thing && (activator_thing->flags & MF_COUNTKILL)) && !active) ||
         ((info->flags & LTF_MISSILE_CROSS_A) &&
          (activator_thing && (activator_thing->flags & MF_MISSILE)) && !active) ||
         ((info->flags & LTF_ANY_CROSS_A) && !active) ||
         ((info->flags & LTF_PLAYER_CROSS_D) && activator && active) ||
         ((info->flags & LTF_MONSTER_CROSS_D) &&
          (activator_thing && (activator_thing->flags & MF_COUNTKILL)) && active) ||
         ((info->flags & LTF_MISSILE_CROSS_D) &&
          (activator_thing && (activator_thing->flags & MF_MISSILE)) && active) ||
         ((info->flags & LTF_ANY_CROSS_D) && active)) || anyTrigger))
        goto type_passes;
    if(evtype == XLE_HIT &&
       ((((info->flags & LTF_PLAYER_HIT_A) && activator && !active) ||
         ((info->flags & LTF_OTHER_HIT_A) && !activator && !active) ||
         ((info->flags & LTF_MONSTER_HIT_A) &&
          (activator_thing && (activator_thing->flags & MF_COUNTKILL)) && !active) ||
         ((info->flags & LTF_MISSILE_HIT_A) &&
          (activator_thing && (activator_thing->flags & MF_MISSILE)) && !active) ||
         ((info->flags & LTF_ANY_HIT_A) && !active) ||
         ((info->flags & LTF_PLAYER_HIT_D) && activator && active) ||
         ((info->flags & LTF_OTHER_HIT_D) && !activator && active) ||
         ((info->flags & LTF_MONSTER_HIT_D) &&
          (activator_thing && (activator_thing->flags & MF_COUNTKILL)) && active) ||
         ((info->flags & LTF_MISSILE_HIT_D) &&
           (activator_thing && (activator_thing->flags & MF_MISSILE)) && active) ||
         ((info->flags & LTF_ANY_HIT_D) && active)) || anyTrigger))
        goto type_passes;
    if(evtype == XLE_TICKER &&
       (((info->flags & LTF_TICKER_A) && !active) ||
        ((info->flags & LTF_TICKER_D) && active)))
        goto type_passes;

    // Type doesn't pass, sorry.
    LOG_MAP_MSG_XGDEVONLY2("Line %i: ACT REQUIREMENTS NOT FULFILLED, ABORTING EVENT", P_ToIndex(line));
    return false;

  type_passes:

    if(info->flags & LTF_NO_OTHER_USE_SECRET)
    {
        // Non-players can't use this line if line is flagged secret.
        if(evtype == XLE_USE && !activator && (xline->flags & ML_SECRET))
        {
            LOG_MAP_MSG_XGDEVONLY2("Line %i: ABORTING EVENT due to no_other_use_secret", P_ToIndex(line));
            return false;
        }
    }

    if(info->flags & LTF_MOBJ_GONE)
    {
        if(Thinker_Iterate(P_MobjThinker, XL_CheckMobjGone, &info->aparm[9]))
            return false;
    }

    if(info->flags & LTF_ACTIVATOR_TYPE)
    {
        // Check the activator's type.
        if(!activator_thing || activator_thing->type != info->aparm[9])
        {
            LOG_MAP_MSG_XGDEVONLY2("Line %i: ABORTING EVENT due to activator type", P_ToIndex(line));
            return false;
        }
    }

    if((evtype == XLE_USE || evtype == XLE_SHOOT || evtype == XLE_CROSS) &&
       !(info->flags2 & LTF2_TWOSIDED))
    {
        // Only allow (de)activation from the front side.
        if(sidenum != 0)
        {
            LOG_MAP_MSG_XGDEVONLY2("Line %i: ABORTING EVENT due to line side test", P_ToIndex(line));
            return false;
        }
    }

    // Check counting.
    if(!info->actCount)
    {
        LOG_MAP_MSG_XGDEVONLY2("Line %i: ABORTING EVENT due to Count = 0", P_ToIndex(line));
        return false;
    }

    // More requirements.
    if((info->flags2 & LTF2_HEALTH_ABOVE) && activator_thing &&
       activator_thing->health <= info->aparm[0])
        return false;
    if((info->flags2 & LTF2_HEALTH_BELOW) && activator_thing &&
       activator_thing->health >= info->aparm[1])
        return false;
    if((info->flags2 & LTF2_POWER_ABOVE) &&
       (!activator || activator->armorPoints <= info->aparm[2]))
        return false;
    if((info->flags2 & LTF2_POWER_BELOW) &&
       (!activator || activator->armorPoints >= info->aparm[3]))
        return false;
    if(info->flags2 & LTF2_LINE_ACTIVE)
        if(!XL_CheckLineStatus(line, info->aparm[4], info->aparm[5], true,
                               activator_thing))
        {
            LOG_MAP_MSG_XGDEVONLY2("Line %i: ABORTING EVENT due to line_active test", P_ToIndex(line));
            return false;
        }
    if(info->flags2 & LTF2_LINE_INACTIVE)
        if(!XL_CheckLineStatus(line, info->aparm[6], info->aparm[7], false,
                               activator_thing))
        {
            LOG_MAP_MSG_XGDEVONLY2("Line %i: ABORTING EVENT due to line_inactive test", P_ToIndex(line));
            return false;
        }
    // Check game mode.
    if (IS_NETGAME)
    {
        const int netFlags = info->flags2 & (LTF2_COOPERATIVE | LTF2_DEATHMATCH);
        if (!netFlags)
        {
            LOG_MAP_MSG_XGDEVONLY2("Line %i: ABORTING EVENT due to netgame mode", P_ToIndex(line));
            return false;
        }
        if (netFlags != (LTF2_COOPERATIVE | LTF2_DEATHMATCH))
        {
            // Need to check which game mode.
            const auto netType = gfw_Rule(deathmatch);
            if (((netFlags & LTF2_COOPERATIVE) && netType != 0) ||
                ((netFlags & LTF2_DEATHMATCH) && netType == 0))
            {
                LOG_MAP_MSG_XGDEVONLY2("Line %i: ABORTING EVENT due to non-matching deathmatch/coop flag", P_ToIndex(line));
                return false;
            }
        }
    }
    else
    {
        if(!(info->flags2 & LTF2_SINGLEPLAYER))
        {
            LOG_MAP_MSG_XGDEVONLY2("Line %i: ABORTING EVENT due to game mode (1p)", P_ToIndex(line));
            return false;
        }
    }

    // Check skill level.
    // SM_NOTHINGS will be interpreted as SM_BABY.
    if(gfw_Rule(skill) < 1)
        i = 1;
    else if(gfw_Rule(skill) > 3)
        i = 4;
    else
        i = 1 << (gfw_Rule(skill) - 1);

    if(!(info->flags2 & (i << LTF2_SKILL_SHIFT)))
    {
        LOG_MAP_MSG_XGDEVONLY2("Line %i: ABORTING EVENT due to skill level (%i)",
               P_ToIndex(line) << gfw_Rule(skill));
        return false;
    }

    // Check activator color.
    if(info->flags2 & LTF2_COLOR)
    {
        if(!activator)
            return false;
        if(cfg.playerColor[activator - players] != info->aparm[8])
        {
            LOG_MAP_MSG_XGDEVONLY2("Line %i: ABORTING EVENT due to activator color (%i)",
                   P_ToIndex(line) << cfg.playerColor[activator-players]);
            return false;
        }
    }

    // Keys require that the activator is a player.
    if(info->
       flags2 & (LTF2_KEY1 | LTF2_KEY2 | LTF2_KEY3 | LTF2_KEY4 | LTF2_KEY5 |
                 LTF2_KEY6))
    {
        // Check keys.
        if(!activator)
        {
            LOG_MAP_MSG_XGDEVONLY2("Line %i: ABORTING EVENT due to missing key (no activator)", P_ToIndex(line));
            return false;
        }

        // Check that all the flagged keys are present.
        if(!XL_CheckKeys(activator_thing, info->flags2, true,
                         (evtype == XLE_USE? true : false)))
        {
            LOG_MAP_MSG_XGDEVONLY2("Line %i: ABORTING EVENT due to missing key", P_ToIndex(line));
            return false;        // Keys missing!
        }
    }

    // All tests passed, use this event.
    if(info->actCount > 0 && evtype != XLE_CHAIN && evtype != XLE_FUNC)
    {
        // Decrement counter.
        info->actCount--;

        LOG_MAP_MSG_XGDEVONLY2("Line %i: Decrementing counter, now %i",
               P_ToIndex(line) << info->actCount);
    }

    XL_ActivateLine(!active, info, line, sidenum, activator_thing, evtype);
    return true;
}

/**
 * @return              @c true, if the event was processed.
 */
int XL_CrossLine(Line* line, int sidenum, mobj_t* thing)
{
    if(!line || !P_ToXLine(line)->xg)
        return false;

    return XL_LineEvent(XLE_CROSS, 0, line, sidenum, thing);
}

/**
 * @return              @c true, if the event was processed.
 */
int XL_UseLine(Line* line, int sidenum, mobj_t* thing)
{
    if(!line || !P_ToXLine(line)->xg)
        return false;

    return XL_LineEvent(XLE_USE, 0, line, sidenum, thing);
}

/**
 * @return              @c true, if the event was processed.
 */
int XL_ShootLine(Line *line, int sidenum, mobj_t *thing)
{
    if(!line || !P_ToXLine(line)->xg)
        return false;

    return XL_LineEvent(XLE_SHOOT, 0, line, sidenum, thing);
}

int XL_HitLine(Line *line, int sidenum, mobj_t *thing)
{
    if(!line || !P_ToXLine(line)->xg)
        return false;

    return XL_LineEvent(XLE_HIT, 0, line, sidenum, thing);
}

void XL_DoChain(Line *line, int chain, dd_bool activating, mobj_t *actThing)
{
    LOG_AS("XL_DoChain");

    // We'll use a dummy for the chain.
    Line *dummyLineDef     = P_AllocDummyLine();
    xline_t *xdummyLineDef = P_ToXLine(dummyLineDef);

    xdummyLineDef->xg = (xgline_t *)Z_Calloc(sizeof(xgline_t), PU_MAP, 0);

    P_SetPtrp(dummyLineDef, DMU_FRONT_SECTOR, P_GetPtrp(line, DMU_FRONT_SECTOR));
    if(0 != P_GetPtrp(line, DMU_BACK))
    {
        P_SetPtrp(dummyLineDef, DMU_BACK_SECTOR, P_GetPtrp(line, DMU_BACK_SECTOR));
    }

    LOG_MAP_MSG_XGDEVONLY2("Line %i, chained type %i", P_ToIndex(line) << chain);
    LOG_MAP_MSG_XGDEVONLY2("(dummy line will show up as %i)", P_ToIndex(dummyLineDef));

    // Copy all properties to the dummies.
    P_CopyLine(dummyLineDef, line);

    xdummyLineDef->xg->active = !activating;

    // Make the chain event
    XL_LineEvent(XLE_CHAIN, chain, dummyLineDef, 0, actThing);

    Z_Free(xdummyLineDef->xg);
    P_FreeDummyLine(dummyLineDef);
}

/**
 * XG lines get to think.
 */
void XL_Thinker(void *xlThinkerPtr)
{
    DE_ASSERT(xlThinkerPtr);
    LOG_AS("XL_Thinker");

    xlthinker_t *xl = static_cast<xlthinker_t *>(xlThinkerPtr);
    Line *line      = xl->line;

    // Clients rely on the server, they don't do XG themselves.
    if(IS_CLIENT) return;

    if(!xl->line) return;

    // Not an xline? Most perculiar...
    xline_t *xline = P_ToXLine(line);
    if(!xline) return;

    // Not an extended line?
    xgline_t *xg = xline->xg;
    if(!xg) return;

    // If disabled do nothing.
    if(xg->disabled) return;

    linetype_t *info = &xg->info;
    float levtime = TIC2FLT(mapTime);

    // Increment time.
    if(xg->timer >= 0)
    {
        xg->timer++;
        xg->tickerTimer++;
    }

    // Activation by ticker.
    if((info->tickerEnd <= 0 ||
        (levtime >= info->tickerStart && levtime <= info->tickerEnd)) &&
       xg->tickerTimer > info->tickerInterval)
    {
        if(info->flags & LTF_TICKER)
        {
            xg->tickerTimer = 0;
            XL_LineEvent(XLE_TICKER, 0, line, 0, XG_DummyThing());
        }

        // How about some forced functions?
        if((((info->flags2 & LTF2_WHEN_ACTIVE) && xg->active) ||
            ((info->flags2 & LTF2_WHEN_INACTIVE) && !xg->active)) &&
            (!(info->flags2 & LTF2_WHEN_LAST) || info->actCount == 1))
        {
            XL_DoFunction(info, line, 0, (mobj_t *)xg->activator, XLE_FORCED);
        }
    }

    // Only process active chain sequences.
    if(xg->active && info->lineClass == LTC_CHAIN_SEQUENCE)
    {
        xg->chTimer -= TIC2FLT(1);

        // idata = current pos
        // fdata = count down seconds

        // i1..i19: line types
        // f0: interval randomness (100 means real interval can be 0%..200%).
        // f1..f19: intervals (seconds)

        // If the counter goes to zero, it's time to execute the chain.
        if(xg->chTimer < 0)
        {
            LOG_MAP_MSG_XGDEVONLY2("Line %i, executing...", P_ToIndex(line));

            // Are there any more chains?
            if(xg->chIdx < DDLT_MAX_PARAMS && info->iparm[xg->chIdx])
            {
                // Only send activation events.
                XL_DoChain(line, info->iparm[xg->chIdx], true, (mobj_t *)xg->activator);

                // Advance to the next one.
                xg->chIdx++;

                // Are we out of chains?
                if((xg->chIdx == DDLT_MAX_PARAMS || !info->iparm[xg->chIdx]) &&
                   (info->iparm[0] & CHSF_LOOP))
                {
                    // Loop back to beginning.
                    xg->chIdx = 1;
                }

                // If there are more chains, get the next interval.
                if(xg->chIdx < DDLT_MAX_PARAMS && info->iparm[xg->chIdx])
                {
                    // Start counting it down.
                    xg->chTimer =
                        XG_RandomPercentFloat(info->fparm[xg->chIdx],
                                              info->fparm[0]);
                }
            }
            else if(info->iparm[0] & CHSF_DEACTIVATE_WHEN_DONE)
            {
                // The sequence has been completed.
                XL_ActivateLine(false, info, line, 0, (mobj_t *)xg->activator, XLE_CHAIN);
            }
        }
    }

    // Check for automatical (de)activation.
    if(((info->actType == LTACT_COUNTED_OFF ||
         info->actType == LTACT_FLIP_COUNTED_OFF) && xg->active) ||
       ((info->actType == LTACT_COUNTED_ON ||
         info->actType == LTACT_FLIP_COUNTED_ON) && !xg->active))
    {
        if(info->actTime >= 0 && xg->timer > FLT2TIC(info->actTime))
        {
            LOG_MAP_MSG_XGDEVONLY2("Line %i, timed to go %s",
                    P_ToIndex(line)
                    << (xg->active ? "INACTIVE" : "ACTIVE"));

            // Swap line state without any checks.
            XL_ActivateLine(!xg->active, info, line, 0, dummyThing, XLE_AUTO);
        }
    }

    if(info->materialMoveSpeed)
    {
        // The texture should be moved. Calculate the offsets.
        coord_t current[2]; // The current offset.
        Side* side;
        float spd = info->materialMoveSpeed;
        coord_t offset[2];
        angle_t ang = ((angle_t) (ANGLE_MAX * (info->materialMoveAngle / 360))) >> ANGLETOFINESHIFT;

        offset[VX] = -(FIX2FLT(finecosine[ang]) * spd);
        offset[VY] = FIX2FLT(finesine[ang]) * spd;

        /**
         * Apply to both sides of the line.
         * These are group offsets. All surfaces on a given side are moved
         * using the same texmove speed/angle.
         *
         * @todo Implement per-surface texture movement also which would
         *        be added to each independantly.
         */

        // Front side.
        side = (Side *)P_GetPtrp(line, DMU_FRONT);
        if(side)
        {
            P_GetDoublepv(side, DMU_TOP_MATERIAL_OFFSET_XY, current);
            current[VX] += offset[VX];
            current[VY] += offset[VY];
            P_SetDoublepv(side, DMU_TOP_MATERIAL_OFFSET_XY, current);

            P_GetDoublepv(side, DMU_MIDDLE_MATERIAL_OFFSET_XY, current);
            current[VX] += offset[VX];
            current[VY] += offset[VY];
            P_SetDoublepv(side, DMU_MIDDLE_MATERIAL_OFFSET_XY, current);

            P_GetDoublepv(side, DMU_BOTTOM_MATERIAL_OFFSET_XY, current);
            current[VX] += offset[VX];
            current[VY] += offset[VY];
            P_SetDoublepv(side, DMU_BOTTOM_MATERIAL_OFFSET_XY, current);
        }

        // Back side.
        side = (Side *)P_GetPtrp(line, DMU_BACK);
        if(side)
        {
            P_GetDoublepv(side, DMU_TOP_MATERIAL_OFFSET_XY, current);
            current[VX] += offset[VX];
            current[VY] += offset[VY];
            P_SetDoublepv(side, DMU_TOP_MATERIAL_OFFSET_XY, current);

            P_GetDoublepv(side, DMU_MIDDLE_MATERIAL_OFFSET_XY, current);
            current[VX] += offset[VX];
            current[VY] += offset[VY];
            P_SetDoublepv(side, DMU_MIDDLE_MATERIAL_OFFSET_XY, current);

            P_GetDoublepv(side, DMU_BOTTOM_MATERIAL_OFFSET_XY, current);
            current[VX] += offset[VX];
            current[VY] += offset[VY];
            P_SetDoublepv(side, DMU_BOTTOM_MATERIAL_OFFSET_XY, current);
        }
    }
}

/**
 * During update, definitions are re-read, so the pointers need to be
 * updated. However, this is a bit messy operation, prone to errors.
 * Instead, we just disable XG...
 */
void XL_Update(void)
{
    int i;
    xline_t *xline;

    // It's all PU_MAP memory, so we can just lose it.
    for(i = 0; i < numlines; ++i)
    {
        xline = P_GetXLine(i);

        if(xline->xg)
        {
            xline->xg = NULL;
            xline->special = 0;
        }
    }
}

#endif
