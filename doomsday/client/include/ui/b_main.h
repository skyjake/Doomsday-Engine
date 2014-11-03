/** @file b_main.h  Event and device state bindings system.
 *
 * @authors Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2014 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef CLIENT_INPUTSYSTEM_BINDINGS_H
#define CLIENT_INPUTSYSTEM_BINDINGS_H

#include <de/types.h>
#include "dd_input.h"

class BindContext;

#define DEFAULT_BINDING_CONTEXT_NAME    "game"
#define CONSOLE_BINDING_CONTEXT_NAME    "console"
#define UI_BINDING_CONTEXT_NAME         "deui"
#define GLOBAL_BINDING_CONTEXT_NAME     "global"

extern int symbolicEchoMode;

void B_ConsoleRegister();

void B_Init();

/**
 * Deallocates memory for the commands and bindings.
 */
void B_Shutdown();

dd_bool B_Delete(int bid);

/**
 * Checks to see if we need to respond to the given input event in some way
 * and then if so executes the action associated to the event.
 *
 * @param ev  ddevent_t we may need to respond to.
 *
 * @return  @c true if an action was executed.
 */
dd_bool B_Responder(ddevent_t *ev);

/**
 * Dump all the bindings to a text (cfg) file. Outputs console commands.
 */
void B_WriteToFile(FILE *file);

/**
 * Enable the contexts for the initial state.
 */
void B_InitialContextActivations();

void B_BindDefaults();

void B_BindGameDefaults();

struct evbinding_s *B_BindCommand(char const *eventDesc, char const *command);

struct dbinding_s *B_BindControl(char const *controlDesc, char const *device);

struct dbinding_s *B_GetControlDeviceBindings(int localNum, int control, BindContext **bContext);

bool B_UnbindCommand(char const *command);

/// Utils: @todo move to b_util.h ----------------------------------------------

/**
 * @return  Never returns zero, as that is reserved for list roots.
 */
int B_NewIdentifier();

char const *B_ShortNameForKey(int ddKey, dd_bool forceLowercase = true);

int B_KeyForShortName(char const *key);

#endif // CLIENT_INPUTSYSTEM_BINDINGS_H
