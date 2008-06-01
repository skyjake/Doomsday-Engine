/**\file
 *\section License
 * License: Raven
 * Online License Link: http://www.dengine.net/raven_license/End_User_License_Hexen_Source_Code.html
 *
 *\author Copyright © 2004-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * p_lights.h:
 */

#ifndef __P_LIGHTS_H__
#define __P_LIGHTS_H__

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#define LIGHT_SEQUENCE_START        (2)
#define LIGHT_SEQUENCE              (3)
#define LIGHT_SEQUENCE_ALT          (4)

typedef enum {
    LITE_RAISEBYVALUE,
    LITE_LOWERBYVALUE,
    LITE_CHANGETOVALUE,
    LITE_FADE,
    LITE_GLOW,
    LITE_FLICKER,
    LITE_STROBE
} lighttype_t;

typedef struct {
    thinker_t       thinker;
    sector_t*       sector;
    lighttype_t     type;
    float           value1;
    float           value2;
    int             tics1;  //// \todo Type LITEGLOW uses this as a third light value. As such, it has been left as 0 - 255 for now.
    int             tics2;
    int             count;
} light_t;

typedef struct {
    thinker_t       thinker;
    sector_t*       sector;
    int             index;
    float           baseValue;
} phase_t;

void            T_Phase(phase_t* phase);
void            P_SpawnPhasedLight(sector_t* sec, float base, int index);

void            T_Light(light_t* light);
void            P_SpawnLightSequence(sector_t* sec, int indexStep);

boolean         EV_SpawnLight(linedef_t* line, byte* arg, lighttype_t type);

#endif
