/**\file abstractfile.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "de_base.h"

#include "sys_reslocator.h"
#include "abstractfile.h"

void AbstractFile_Init(abstractfile_t* file, filetype_t type, const char* absolutePath)
{
    // Used to favor newer files when duplicates are pruned.
    static uint fileCounter = 0;
    assert(NULL != file && NULL != absolutePath);

    file->_order = fileCounter++;
    file->_type = type;
    file->_flags.startup = false;
    file->_flags.iwad = false;
    Str_Init(&file->_path);
    Str_Set(&file->_path, absolutePath);
    Str_Strip(&file->_path);
    F_FixSlashes(&file->_path, &file->_path);

    file->_dfile.flags.eof = false;
    file->_dfile.flags.open = false;
    file->_dfile.size = 0;
    file->_dfile.hndl = NULL;
    file->_dfile.data = NULL;
    file->_dfile.pos = 0;
    file->_dfile.lastModified = 0;
}

filetype_t AbstractFile_Type(const abstractfile_t* file)
{
    assert(NULL != file);
    return file->_type;
}

const ddstring_t* AbstractFile_Path(abstractfile_t* file)
{
    assert(NULL != file);
    return &file->_path;
}

uint AbstractFile_LoadOrderIndex(abstractfile_t* file)
{
    assert(NULL != file);
    return file->_order;
}

uint AbstractFile_LastModified(abstractfile_t* file)
{
    assert(NULL != file);
    return file->_dfile.lastModified;
}

DFILE* AbstractFile_Handle(abstractfile_t* file)
{
    assert(NULL != file);
    return &file->_dfile;
}

boolean AbstractFile_HasStartup(abstractfile_t* file)
{
    assert(NULL != file);
    return (file->_flags.startup != 0);
}

void AbstractFile_SetStartup(abstractfile_t* file, boolean yes)
{
    assert(NULL != file);
    file->_flags.startup = yes;
}

boolean AbstractFile_HasIWAD(abstractfile_t* file)
{
    assert(NULL != file);
    return (file->_flags.iwad != 0);
}

void AbstractFile_SetIWAD(abstractfile_t* file, boolean yes)
{
    assert(NULL != file);
    file->_flags.iwad = yes;
}
