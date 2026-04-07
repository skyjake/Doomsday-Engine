/** @file world/actions.h
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_WORLD_ACTIONS_H
#define LIBDOOMSDAY_WORLD_ACTIONS_H

#include "../libdoomsday.h"

typedef void (C_DECL *acfnptr_t) ();

typedef struct actionlink_s {
    char *name;             ///< Name of the routine.
    void (C_DECL *func)();  ///< Pointer to the function.
} actionlink_t;

#ifdef __cplusplus
extern "C" {
#endif

LIBDOOMSDAY_PUBLIC void      P_GetGameActions();
LIBDOOMSDAY_PUBLIC acfnptr_t P_GetAction(const char *name);
LIBDOOMSDAY_PUBLIC void      P_SetCurrentActionState(int state);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus

#include <de/string.h>
LIBDOOMSDAY_PUBLIC acfnptr_t P_GetAction(const de::String &name);
LIBDOOMSDAY_PUBLIC void      P_SetCurrentAction(const de::String &name);

#endif // __cplusplus

#endif // LIBDOOMSDAY_WORLD_ACTIONS_H

