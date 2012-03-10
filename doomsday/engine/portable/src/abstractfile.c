/**\file abstractfile.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
#include "de_filesys.h"

#include "abstractfile.h"

abstractfile_t* AbstractFile_Init(abstractfile_t* af, filetype_t type,
    const char* path, DFile* file, const LumpInfo* info)
{
    // Used to favor newer files when duplicates are pruned.
    static uint fileCounter = 0;
    assert(af && VALID_FILETYPE(type) && info);

    af->_order = fileCounter++;
    af->_type = type;
    af->_file = file;
    af->_flags.startup = false;
    af->_flags.custom = true;
    Str_Init(&af->_path); Str_Set(&af->_path, path);
    F_CopyLumpInfo(&af->_info, info);

    return af;
}

void AbstractFile_Destroy(abstractfile_t* af)
{
    assert(af);
    Str_Free(&af->_path);
    F_DestroyLumpInfo(&af->_info);
    if(af->_file)
        DFile_Delete(af->_file, true);
}

filetype_t AbstractFile_Type(const abstractfile_t* af)
{
    assert(af);
    return af->_type;
}

const LumpInfo* AbstractFile_Info(abstractfile_t* af)
{
    assert(af);
    return &af->_info;
}

abstractfile_t* AbstractFile_Container(const abstractfile_t* af)
{
    assert(af);
    return af->_info.container;
}

size_t AbstractFile_BaseOffset(const abstractfile_t* af)
{
    assert(af);
    return (af->_file? DFile_BaseOffset(af->_file) : 0);
}

DFile* AbstractFile_Handle(abstractfile_t* af)
{
    assert(af);
    return af->_file;
}

const ddstring_t* AbstractFile_Path(const abstractfile_t* af)
{
    assert(af);
    return &af->_path;
}

uint AbstractFile_LoadOrderIndex(const abstractfile_t* af)
{
    assert(af);
    return af->_order;
}

uint AbstractFile_LastModified(const abstractfile_t* af)
{
    assert(af);
    return af->_info.lastModified;
}

boolean AbstractFile_HasStartup(const abstractfile_t* af)
{
    assert(af);
    return (af->_flags.startup != 0);
}

void AbstractFile_SetStartup(abstractfile_t* af, boolean yes)
{
    assert(af);
    af->_flags.startup = yes;
}

boolean AbstractFile_HasCustom(const abstractfile_t* af)
{
    assert(af);
    return (af->_flags.custom != 0);
}

void AbstractFile_SetCustom(abstractfile_t* af, boolean yes)
{
    assert(af);
    af->_flags.custom = yes;
}
