/** @file dedfile.h  Definition files.
 * @ingroup defs
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDOOMSDAY_DEFS_DED_H
#define LIBDOOMSDAY_DEFS_DED_H

#include "../libdoomsday.h"
#include "ded.h"
#include <de/string.h>

LIBDOOMSDAY_PUBLIC void Def_ReadProcessDED(ded_t *defs, const de::String& path);

/**
 * Reads definitions from the given lump.
 */
LIBDOOMSDAY_PUBLIC int DED_ReadLump(ded_t *ded, lumpnum_t lumpNum);

/**
 * Reads definitions from the given buffer.
 * The definition is being loaded from @a _sourcefile (DED or WAD).
 *
 * @param buffer          The data to be read, must be null-terminated.
 * @param sourceFile      Just FYI.
 * @param sourceIsCustom  @c true= source is a user supplied add-on.
 */
LIBDOOMSDAY_PUBLIC int DED_ReadData(ded_t *ded, const char *buffer, de::String sourceFile,
    bool sourceIsCustom);

/**
 * @return  @c true, if the file was successfully loaded.
 */
int DED_Read(ded_t *ded, const de::String& path);

void DED_SetError(const de::String &message);

LIBDOOMSDAY_PUBLIC const char *DED_Error();

#endif // LIBDOOMSDAY_DEFS_DED_H
