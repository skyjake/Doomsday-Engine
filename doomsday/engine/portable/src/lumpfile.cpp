/**\file lumpfile.c
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
#include "de_console.h"
#include "de_filesys.h"

#include "lumpdirectory.h"
#include "lumpfile.h"

LumpFile* LumpFile_New(DFile* file, const char* path, const LumpInfo* info)
{
    LumpFile* lump = (LumpFile*)malloc(sizeof *lump);
    if(!lump) Con_Error("LumpFile::Construct: Failed on allocation of %lu bytes for new LumpFile.",
                (unsigned long) sizeof *lump);

    AbstractFile_Init((abstractfile_t*)lump, FT_LUMPFILE, path, file, info);
    return lump;
}

void LumpFile_Delete(LumpFile* lump)
{
    assert(lump);
    F_ReleaseFile((abstractfile_t*)lump);
    AbstractFile_Destroy((abstractfile_t*)lump);
    free(lump);
}

const LumpInfo* LumpFile_LumpInfo(LumpFile* lump, int lumpIdx)
{
    assert(lump);
    /// Lump files are special cases for this *is* the lump.
    return AbstractFile_Info((abstractfile_t*)lump);
}

int LumpFile_LumpCount(LumpFile* lump)
{
    return 1; // Always.
}
