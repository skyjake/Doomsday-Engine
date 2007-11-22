/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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

/**
 * p_acs.h: ACS scripting system.
 */

#ifndef __P_PSPR_H__
#define __P_PSPR_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#define MAX_ACS_SCRIPT_VARS     10
#define MAX_ACS_MAP_VARS        32
#define MAX_ACS_WORLD_VARS      64
#define ACS_STACK_DEPTH         32
#define MAX_ACS_STORE           20

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
    int            *address;
    int             argCount;
    aste_t          state;
    int             waitValue;
} acsinfo_t ;

typedef struct acs_s {
    thinker_t       thinker;
    mobj_t         *activator;
    line_t         *line;
    int             side;
    int             number;
    int             infoIndex;
    int             delayCount;
    int             stack[ACS_STACK_DEPTH];
    int             stackPtr;
    int             vars[MAX_ACS_SCRIPT_VARS];
    int            *ip;
} acs_t;

typedef struct acsstore_s {
    int             map; // Target map.
    int             script; // Script number on target map.
    byte            args[4]; // Padded to 4 for alignment.
} acsstore_t;

extern int ACScriptCount;
extern byte *ActionCodeBase;
extern acsinfo_t *ACSInfo;
extern int MapVars[MAX_ACS_MAP_VARS];
extern int WorldVars[MAX_ACS_WORLD_VARS];
extern acsstore_t ACSStore[MAX_ACS_STORE + 1]; // +1 for termination marker.

void            P_LoadACScripts(int lump);
boolean         P_StartACS(int number, int map, byte *args, mobj_t *activator,
                           line_t *line, int side);
boolean         P_StartLockedACS(line_t *line, byte *args, mobj_t *mo,
                                 int side);
boolean         P_TerminateACS(int number, int map);
boolean         P_SuspendACS(int number, int map);
void            T_InterpretACS(acs_t *script);
void            P_TagFinished(int tag);
void            P_PolyobjFinished(int po);
void            P_ACSInitNewGame(void);
void            P_CheckACSStore(void);

#endif
