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

typedef enum aste_e {
    ASTE_INACTIVE,
    ASTE_RUNNING,
    ASTE_SUSPENDED,
    ASTE_WAITING_FOR_TAG,
    ASTE_WAITING_FOR_POLY,
    ASTE_WAITING_FOR_SCRIPT,
    ASTE_TERMINATING
} aste_t;

typedef struct acsinfo_s {
    int number;
    int const *address;
    int argCount;
    aste_t state;
    int waitValue;
} acsinfo_t;

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

void T_InterpretACS(acs_t *script);

#ifdef __cplusplus
} // extern "C"
#endif

typedef struct acsstore_s {
    uint map;     ///< Target map.
    int script;   ///< Script number on target map.
    byte args[4]; ///< Padded to 4 for alignment.
} acsstore_t;

DENG_EXTERN_C int ACScriptCount;
DENG_EXTERN_C byte const *ActionCodeBase;
DENG_EXTERN_C acsinfo_t *ACSInfo;
DENG_EXTERN_C int MapVars[MAX_ACS_MAP_VARS];
DENG_EXTERN_C int WorldVars[MAX_ACS_WORLD_VARS];
DENG_EXTERN_C int ACSStoreSize;
DENG_EXTERN_C acsstore_t *ACSStore;

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

#endif // LIBHEXEN_PLAY_ACS_H
