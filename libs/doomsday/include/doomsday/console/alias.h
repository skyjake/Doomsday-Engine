/** @file alias.h
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDOOMSDAY_CONSOLE_ALIAS_H
#define LIBDOOMSDAY_CONSOLE_ALIAS_H

#include "../libdoomsday.h"
#include <de/string.h>

typedef struct calias_s {
    /// Name of this alias.
    char *name;

    /// Aliased command string.
    char *command;
} calias_t;

void Con_InitAliases();
void Con_ClearAliases();
void Con_AddKnownWordsForAliases();

LIBDOOMSDAY_PUBLIC calias_t *Con_AddAlias(const char *name, const char *command);

/**
 * @return  @c 0 if the specified alias can't be found.
 */
LIBDOOMSDAY_PUBLIC calias_t *Con_FindAlias(const char *name);

LIBDOOMSDAY_PUBLIC void Con_DeleteAlias(calias_t *cal);

LIBDOOMSDAY_PUBLIC de::String Con_AliasAsStyledText(calias_t *alias);

#endif // LIBDOOMSDAY_CONSOLE_ALIAS_H
