/** @file defs/ded.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "doomsday/defs/ded.h"
#include "doomsday/defs/parser.h"
#include "doomsday/filesys/fs_main.h"
#include <de/Log>

extern char dedReadError[512]; // in defs/parser.cpp

void Def_ReadProcessDED(char const* path)
{
    LOG_AS("Def_ReadProcessDED");

    if(!path || !path[0]) return;

    de::Uri path_ = de::Uri(path, RC_NULL);
    if(!App_FileSystem().accessFile(path_))
    {
        LOG_RES_WARNING("\"%s\" not found!") << NativePath(path_.asText()).pretty();
        return;
    }

    // We use the File Ids to prevent loading the same files multiple times.
    if(!App_FileSystem().checkFileId(path_))
    {
        // Already handled.
        LOG_RES_XVERBOSE("\"%s\" has already been read") << NativePath(path_.asText()).pretty();
        return;
    }

    if(!DED_Read(&defs, path))
    {
        App_FatalError("Def_ReadProcessDED: %s\n", dedReadError);
    }
}
