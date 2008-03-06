/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2003-2005 Samuel Villarreal <svkaiser@gmail.com>
 *\author Copyright © 1999 by Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 *\author Copyright © 1999-2000 by Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 *\author Copyright © 1993-1996 by id Software, Inc.
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
 * p_switch.h: Switches, buttons. Two-state animation. Exits.
 */

#ifndef __P_SWITCH_H__
#define __P_SWITCH_H__

#ifndef __DOOM64TC__
#  error "Using Doom64TC headers without __DOOM64TC__"
#endif

#include "doomsday.h"

#include "p_mobj.h"

#define BUTTONTIME          (TICSPERSEC) // 1 second, in ticks.

/**
 * This struct is used to provide byte offsets when reading a custom
 * SWITCHES lump thus it must be packed and cannot be altered.
 */
#pragma pack(1)
typedef struct {
    /* Do NOT change these members in any way! */
    char            name1[9];
    char            name2[9];
    short           episode;
} switchlist_t;
#pragma pack()

typedef enum linesection_e{
    LS_MIDDLE,
    LS_BOTTOM,
    LS_TOP
} linesection_t;

typedef struct button_s {
    linedef_t      *line;
    linesection_t   section;
    int             texture;
    int             timer;
    mobj_t         *soundOrg;

    struct button_s *next;
} button_t;

extern button_t *buttonlist;

void            P_InitSwitchList(void);

void            P_FreeButtons(void);
void            P_ChangeSwitchTexture(linedef_t *line, int useAgain);

void            P_SetSectorColor(linedef_t *line);
boolean         P_UseSpecialLine(mobj_t *thing, linedef_t *line, int side);

int             EV_DestoryLineShield(linedef_t* line);
int             EV_SwitchTextureFree(linedef_t* line);
int             EV_ActivateSpecial(linedef_t *line);

#endif
