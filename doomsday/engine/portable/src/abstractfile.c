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

void AbstractFile_Init(abstractfile_t* file, filetype_t type, FILE* handle,
    const char* absolutePath)
{
    // Used to favor newer files when duplicates are pruned.
    static uint fileCounter = 0;
    assert(NULL != file && NULL != absolutePath);
    file->_order = fileCounter++;
    file->_type = type;
    Str_Init(&file->_absolutePath);
    if(absolutePath)
    {
        Str_Set(&file->_absolutePath, absolutePath);
        Str_Strip(&file->_absolutePath);
        F_FixSlashes(&file->_absolutePath, &file->_absolutePath);
    }
    file->hndl = handle;
    file->flags.eof = 0;
    file->flags.open = 0;
    file->size = 0;
    file->data = NULL;
    file->pos = 0;
    file->lastModified = 0;
}

filetype_t AbstractFile_Type(const abstractfile_t* file)
{
    assert(NULL != file);
    return file->_type;
}

FILE* AbstractFile_Handle(abstractfile_t* file)
{
    assert(NULL != file);
    return file->hndl;
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
