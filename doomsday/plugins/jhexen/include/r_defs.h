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
 * r_defs.h:
 */

#ifndef __R_DEFS_H__
#define __R_DEFS_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "p_mobj.h"
#include "s_sequence.h"

typedef struct xsector_s {
    short           special, tag;
    int             soundtraversed; // 0 = untraversed, 1,2 = sndlines -1
    mobj_t         *soundtarget;   // thing that made a sound (or null)
    seqtype_t       seqType;       // stone, metal, heavy, etc...
    void           *specialdata;   // thinker_t for reversable actions
} xsector_t;

typedef struct xline_s {
    byte            special;
    byte            arg1;
    byte            arg2;
    byte            arg3;
    byte            arg4;
    byte            arg5;
    // Has been rendered at least once and needs to appear in the map,
    // for each player.
    boolean         mapped[MAXPLAYERS];
    int             validcount;
} xline_t;

xline_t*    P_ToXLine(line_t* line);
xline_t*    P_GetXLine(uint index);
xsector_t*  P_ToXSector(sector_t* sector);
xsector_t*  P_GetXSector(uint index);
xsector_t*  P_ToXSectorOfSubsector(subsector_t* sub);
#endif
