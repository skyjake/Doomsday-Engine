/** @file inputdebug.h  Input debug visualization.
 *
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

#ifndef CLIENT_INPUTDEBUG_H
#define CLIENT_INPUTDEBUG_H

#include <de/libcore.h>

#ifdef DE_DEBUG

/**
 * Render a visual representation of the current state of all input devices.
 */
void I_DebugDrawer();

/**
 * Register the commands and variables of this module.
 */
void I_DebugDrawerConsoleRegister();

#endif // DE_DEBUG

#endif // CLIENT_INPUTDEBUG_H

