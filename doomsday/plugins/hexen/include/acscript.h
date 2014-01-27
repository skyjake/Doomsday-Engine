/** @file acscript.h  Hexen "ACS" scripting system.
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
#include "p_mobj.h"

#define MAX_ACS_SCRIPT_VARS     10
#define MAX_ACS_MAP_VARS        32
#define MAX_ACS_WORLD_VARS      64
#define ACS_STACK_DEPTH         32

#ifdef __cplusplus
extern "C" {
#endif

/**
 * To be called when a new game session begins to initialize ACS scripting.
 */
void P_InitACScript(void);

void P_LoadACScripts(lumpnum_t lump);

/**
 * @param map  Map number on which the script is being started on.
 *             @c 0 = Current map. Otherwise 1-based index of the map to start on (deferred).
 */
dd_bool P_StartACScript(int number, uint map, byte *args, mobj_t *activator, Line *line, int side);

dd_bool P_TerminateACScript(int number, uint map);

dd_bool P_SuspendACScript(int number, uint map);

void P_ACScriptTagFinished(int tag);

void P_ACScriptPolyobjFinished(int tag);

/**
 * To be called when the current map changes to activate any deferred scripts which
 * should now begin/resume.
 */
void P_ACScriptRunDeferredTasks(uint map/*Uri const *map*/);

void P_WriteGlobalACScriptData(void);
void P_ReadGlobalACScriptData(int saveVersion);

void P_WriteMapACScriptData(void);
void P_ReadMapACScriptData(void);

#ifdef __cplusplus
} // extern "C"
#endif

/**
 * Action script thinker.
 */
typedef struct acscript_s {
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

#ifdef __cplusplus
    void push(int value)
    {
        stack[stackPtr++] = value;
    }

    int pop()
    {
        return stack[--stackPtr];
    }

    int top()
    {
        return stack[stackPtr - 1];
    }

    void drop()
    {
        stackPtr--;
    }
#endif
} ACScript;

#ifdef __cplusplus
extern "C" {
#endif

void ACScript_Thinker(ACScript *script);

/**
 * Serialize the thinker to the currently open save file.
 */
void ACScript_Write(ACScript const *script);

/**
 * Deserialize the thinker from the currently open save file.
 */
int ACScript_Read(ACScript *script, int mapVersion);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBHEXEN_PLAY_ACS_H
