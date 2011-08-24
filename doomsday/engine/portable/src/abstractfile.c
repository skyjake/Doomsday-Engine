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

void AbstractFile_Init(abstractfile_t* file, filetype_t type,
    DFILE* handle, const char* absolutePath)
{
    // Used with to favor newer files when duplicates are pruned.
    static uint fileCounter = 0;
    assert(NULL != file && NULL != handle && NULL != absolutePath);
    file->_order = fileCounter++;
    file->_type = type;
    file->_handle = handle;
    Str_Init(&file->_absolutePath);
    Str_Set(&file->_absolutePath, absolutePath);
    Str_Strip(&file->_absolutePath);
    F_FixSlashes(&file->_absolutePath, &file->_absolutePath);
}

filetype_t AbstractFile_Type(const abstractfile_t* file)
{
    assert(NULL != file);
    return file->_type;
}

DFILE* AbstractFile_Handle(abstractfile_t* file)
{
    assert(NULL != file);
    return file->_handle;
}

const ddstring_t* AbstractFile_AbsolutePath(abstractfile_t* file)
{
    assert(NULL != file);
    return &file->_absolutePath;
}

uint AbstractFile_LoadOrderIndex(abstractfile_t* file)
{
    assert(NULL != file);
    return file->_order;
}
