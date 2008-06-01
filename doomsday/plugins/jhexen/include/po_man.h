/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
 * po_man.h: Polyobjects.
 */

#ifndef __PO_MAN_H__
#define __PO_MAN_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

typedef enum {
    PODOOR_NONE,
    PODOOR_SLIDE,
    PODOOR_SWING,
} podoortype_t;

typedef struct {
    thinker_t       thinker;
    int             polyobj;
    int             intSpeed;
    unsigned int    dist;
    int             fangle;
    float           speed[2]; // for sliding walls
} polyevent_t;

typedef struct {
    thinker_t       thinker;
    int             polyobj;
    int             intSpeed;
    int             dist;
    int             totalDist;
    int             direction;
    float           speed[2];
    int             tics;
    int             waitTics;
    podoortype_t    type;
    boolean         close;
} polydoor_t;

enum {
    PO_ANCHOR_TYPE = 3000,
    PO_SPAWN_TYPE,
    PO_SPAWNCRUSH_TYPE
};

extern polyobj_t *polyobjs; // List of all poly-objects on the level.
extern int po_NumPolyobjs;

void        PO_InitForMap(void);
boolean     PO_Busy(int polyobj);

boolean     PO_FindAndCreatePolyobj(int tag, boolean crush, float startX, float startY);

void        T_PolyDoor(polydoor_t *pd);
void        T_RotatePoly(polyevent_t *pe);
boolean     EV_RotatePoly(linedef_t *line, byte *args, int direction,
                          boolean overRide);
void        T_MovePoly(polyevent_t *pe);
boolean     EV_MovePoly(linedef_t *line, byte *args, boolean timesEight,
                        boolean overRide);
boolean     EV_OpenPolyDoor(linedef_t *line, byte *args, podoortype_t type);

#endif
