/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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

void B_Bind(event_t *event, char *command);
void B_EventBuilder(char *buff, event_t *ev, boolean to_event);
int B_BindingsForCommand(char *command, char *buffer);
void B_ClearBinding(char *command);
boolean B_Responder(event_t *ev);
void B_WriteToFile(FILE *file);
void B_Shutdown();

int DD_GetKeyCode(const char *key);

#endif 
