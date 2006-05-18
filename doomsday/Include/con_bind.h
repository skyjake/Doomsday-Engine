/* DE1: $Id$
 * Copyright (C) 2003, 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * con_bind.h: Event/Command Binding
 */

#ifndef __DOOMSDAY_CONSOLE_BIND_H__
#define __DOOMSDAY_CONSOLE_BIND_H__

#include <stdio.h>
#include "de_base.h"

typedef struct {
    char *command;
} command_t;

typedef struct {
    event_t event;
    int     flags;
    command_t *commands;
} binding_t;

void            B_Bind(event_t *event, char *command, int bindClass);
void            DD_AddBindClass(struct bindclass_s *);
boolean         B_SetBindClass(int classID, int type);
void            B_RegisterBindClasses(void);
void            B_EventBuilder(char *buff, event_t *ev, boolean to_event);
int             B_BindingsForCommand(char *command, char *buffer, int bindClass);
void            B_ClearBinding(char *command, int bindClass);
boolean         B_Responder(event_t *ev);
void            B_WriteToFile(FILE * file);
void            B_Shutdown();

int             DD_GetKeyCode(const char *key);

#endif
