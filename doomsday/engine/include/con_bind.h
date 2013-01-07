/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

/*
 * con_bind.h: Control Binding
 */

#ifndef __DOOMSDAY_CONSOLE_BIND_H__
#define __DOOMSDAY_CONSOLE_BIND_H__

#include <stdio.h>
#include "de_base.h"

typedef enum {
    BND_UNUSED = -1,
    BND_COMMAND = 0,
    BND_AXIS,
    NUM_BIND_TYPES
} bindtype_t;

typedef struct {
    char       *command[NUM_EVENT_STATES]; // { down, up, repeat }
} bindcommand_t;

typedef struct {
    int         localPlayer;
    int         playercontrol;
    boolean     invert;
} bindaxis_t;

typedef struct {
    bindtype_t type;
    union {
        bindcommand_t command;
        bindaxis_t axiscontrol;
    } data;
} bindcontrol_t;

typedef struct {
    int         controlID;      // control index.
    bindcontrol_t *binds;       // [maxBindClasses] size
} binding_t;

void    B_Register(void);
void    B_Init(void);
void    DD_AddBindClass(struct bindclass_s *);
boolean  B_SetBindClass(uint classID, uint type);
void    B_RegisterBindClasses(void);
char   *B_ShortNameForKey(int ddkey);
boolean B_Responder(ddevent_t *ev);
void    B_WriteToFile(FILE *file);
void    B_Shutdown(void);

#endif
