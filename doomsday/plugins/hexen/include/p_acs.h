/** @file p_acs.h  Hexen "ACS" scripting system.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBHEXEN_PLAY_ACS_H
#define LIBHEXEN_PLAY_ACS_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "jhexen.h"

#define MAX_ACS_SCRIPT_VARS     10
#define MAX_ACS_MAP_VARS        32
#define MAX_ACS_WORLD_VARS      64
#define ACS_STACK_DEPTH         32

#ifdef __cplusplus
extern "C" {
#endif

void P_LoadACScripts(int lump);

dd_bool P_StartACS(int number, uint map, byte *args, mobj_t *activator, Line *line, int side);

dd_bool P_StartLockedACS(Line *line, byte *args, mobj_t *mo, int side);

dd_bool P_TerminateACS(int number, uint map);

dd_bool P_SuspendACS(int number, uint map);

void P_TagFinished(int tag);

void P_ACSPolyobjFinished(int tag);

void P_ACSInitNewGame(void);

void P_CheckACSStore(uint map);

void P_WriteGlobalACScriptData();
void P_ReadGlobalACScriptData(int saveVersion);

void P_WriteMapACScriptData();
void P_ReadMapACScriptData();

#ifdef __cplusplus
} // extern "C"
#endif

/**
 * Action script thinker.
 */
typedef struct acs_s {
    thinker_t thinker;
    mobj_t *activator;
    Line *line;
    int side;
    int number;
    int infoIndex;
    int delayCount;
    int stack[ACS_STACK_DEPTH];
    int stackPtr;
    int vars[MAX_ACS_SCRIPT_VARS];
    int const *ip;
} acs_t;

#ifdef __cplusplus
extern "C" {
#endif

void ACScript_Thinker(acs_t *script);

/**
 * Serialize this thinker to the currently open save file.
 */
void ACScript_Write(acs_t const *script);

/**
 * Deserialize the thinker from the currently open save file.
 */
int ACScript_Read(acs_t *script, int mapVersion);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBHEXEN_PLAY_ACS_H
