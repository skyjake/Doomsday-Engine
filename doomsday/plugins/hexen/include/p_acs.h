/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * p_acs.h: ACS scripting system.
 */

#ifndef __P_ACS_H__
#define __P_ACS_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

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
    int             number;
    const int*      address;
    int             argCount;
    aste_t          state;
    int             waitValue;
} acsinfo_t;

typedef struct acs_s {
    thinker_t       thinker;
    mobj_t*         activator;
    LineDef*        line;
    int             side;
    int             number;
    int             infoIndex;
    int             delayCount;
    int             stack[ACS_STACK_DEPTH];
    int             stackPtr;
    int             vars[MAX_ACS_SCRIPT_VARS];
    const int*      ip;
} acs_t;

typedef struct acsstore_s {
    uint            map; // Target map.
    int             script; // Script number on target map.
    byte            args[4]; // Padded to 4 for alignment.
} acsstore_t;

extern int ACScriptCount;
extern const byte* ActionCodeBase;
extern acsinfo_t* ACSInfo;
extern int MapVars[MAX_ACS_MAP_VARS];
extern int WorldVars[MAX_ACS_WORLD_VARS];
extern int ACSStoreSize;
extern acsstore_t* ACSStore;

void            P_LoadACScripts(int lump);
boolean         P_StartACS(int number, uint map, byte* args,
                           mobj_t* activator, LineDef* line, int side);
boolean         P_StartLockedACS(LineDef* line, byte* args, mobj_t* mo,
                                 int side);
boolean         P_TerminateACS(int number, uint map);
boolean         P_SuspendACS(int number, uint map);
void            T_InterpretACS(acs_t* script);
void            P_TagFinished(int tag);
void            P_PolyobjFinished(int po);
void            P_ACSInitNewGame(void);
void            P_CheckACSStore(uint map);

#endif
