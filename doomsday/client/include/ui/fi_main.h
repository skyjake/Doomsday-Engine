/** @file
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_INFINE_MAIN_H
#define LIBDENG_INFINE_MAIN_H

#include "finaleinterpreter.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Called during pre-init to register cvars and ccmds for the finale system.
 */
void FI_Register(void);

void FI_Init(void);
void FI_Shutdown(void);

void FI_Ticker(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_INFINE_MAIN_H */
