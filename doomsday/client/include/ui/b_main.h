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
struct dbinding_t;

void B_Init();

/**
 * Deallocates memory for the commands and bindings.
 */
void B_Shutdown();

void B_BindDefaults();

void B_BindGameDefaults();

dbinding_t *B_GetControlBindings(int localNum, int control, BindContext **context);

#endif // CLIENT_INPUTSYSTEM_BINDINGS_H
