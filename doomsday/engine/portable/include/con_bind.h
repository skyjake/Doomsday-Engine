/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 * con_bind.h: Event/Command Binding
 */

#ifndef __DOOMSDAY_CONSOLE_BIND_H__
#define __DOOMSDAY_CONSOLE_BIND_H__

#include <stdio.h>
#include "de_base.h"

typedef struct {
    char *command[3];       // { down, up, repeat }
} command_t;

typedef struct {
    evtype_t    type;
    int         data1;      // keys/mouse/joystick buttons
    int         flags;
    command_t  *commands;
} binding_t;

void            B_Bind(event_t *event, char *command, int bindClass);
void            DD_AddBindClass(struct bindclass_s *);
boolean         B_SetBindClass(int classID, int type);
void            B_RegisterBindClasses(void);
int             B_BindingsForCommand(char *command, char *buffer, int bindClass);
void            B_ClearBinding(char *command, int bindClass);
boolean         B_Responder(event_t *ev);
void            B_WriteToFile(FILE * file);
void            B_Shutdown();

int             DD_GetKeyCode(const char *key);

#endif
