/** @file basepath.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/paths.h"
#include "doomsday/filesys/sys_direc.h"
#include "doomsday/filesys/fs_util.h"
#include <string>

using namespace de;

static std::string ddBasePath; // Doomsday root directory is at...?
static std::string ddRuntimePath;
static std::string ddBinPath;

char const *DD_BasePath()
{
    return ddBasePath.c_str();
}

void DD_SetBasePath(char const *path)
{
    /// @todo Unfortunately Dir/fs_util assumes fixed-size strings, so we
    /// can't take advantage of std::string. -jk
    filename_t temp;
    strncpy(temp, path, FILENAME_T_MAXLEN);

    Dir_CleanPath(temp, FILENAME_T_MAXLEN);
    Dir_MakeAbsolutePath(temp, FILENAME_T_MAXLEN);

    // Ensure it ends with a directory separator.
    F_AppendMissingSlashCString(temp, FILENAME_T_MAXLEN);

    ddBasePath = temp;
}

char const *DD_RuntimePath()
{
    return ddRuntimePath.c_str();
}

void DD_SetRuntimePath(char const *path)
{
    ddRuntimePath = path;
}

char const *DD_BinPath()
{
    return ddBinPath.c_str();
}

void DD_SetBinPath(char const *path)
{
    ddBinPath = path;
}
