/**\file lumpfile.c
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
#include "de_console.h"
#include "de_filesys.h"

#include "lumpdirectory.h"
#include "lumpfile.h"

lumpfile_t* LumpFile_New(DFile* file, const lumpinfo_t* info)
{
    lumpfile_t* lf = (lumpfile_t*)malloc(sizeof *lf);
    if(!lf) Con_Error("LumpFile::Construct:: Failed on allocation of %lu bytes for new LumpFile.",
                (unsigned long) sizeof *lf);

    AbstractFile_Init((abstractfile_t*)lf, FT_LUMPFILE, file, info);
    lf->_cacheData = NULL;
    return lf;
}

void LumpFile_Delete(lumpfile_t* lf)
{
    assert(lf);
    F_ReleaseFile((abstractfile_t*)lf);
    LumpFile_ClearLumpCache(lf);
    AbstractFile_Destroy((abstractfile_t*)lf);
    free(lf);
}

int LumpFile_PublishLumpsToDirectory(lumpfile_t* lf, lumpdirectory_t* directory)
{
    assert(lf && directory);
    // Insert the lumps into their rightful places in the directory.
    LumpDirectory_Append(directory, (abstractfile_t*)lf, 0, 1);
    return 1;
}

const lumpinfo_t* LumpFile_LumpInfo(lumpfile_t* lf, int lumpIdx)
{
    assert(lf);
    /// Lump files are special cases for this *is* the lump.
    return AbstractFile_Info((abstractfile_t*)lf);
}

void LumpFile_ClearLumpCache(lumpfile_t* lf)
{
    assert(lf);
    if(lf->_cacheData)
    {
        // If the block has a user, it must be explicitly freed.
        if(Z_GetTag(lf->_cacheData) < PU_MAP)
            Z_ChangeTag(lf->_cacheData, PU_MAP);
        // Mark the memory pointer in use, but unowned.
        Z_ChangeUser(lf->_cacheData, (void*) 0x2);
    }
}

size_t LumpFile_ReadLumpSection2(lumpfile_t* lf, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length, boolean tryCache)
{
    assert(lf);
    {
    const lumpinfo_t* info = LumpFile_LumpInfo(lf, lumpIdx);
    size_t readBytes;

    VERBOSE2(
        Con_Printf("LumpFile::ReadLumpSection: \"%s:%s\" (%lu bytes%s) [%lu +%lu]",
                F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)lf))),
                (info->name[0]? info->name : F_PrettyPath(Str_Text(&info->path))), (unsigned long) info->size,
                (info->compressedSize != info->size? ", compressed" : ""),
                (unsigned long) startOffset, (unsigned long)length) )

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache && lf->_cacheData)
    {
        boolean isCached = (NULL != lf->_cacheData);
        void** cachePtr = (void**)&lf->_cacheData;
        if(isCached)
        {
            VERBOSE2( Con_Printf(" from cache\n") )
            readBytes = MIN_OF(info->size, length);
            memcpy(buffer, (char*)*cachePtr + startOffset, readBytes);
            return readBytes;
        }
    }

    VERBOSE2( Con_Printf("\n") )
    DFile_Seek(lf->_base._file, startOffset, SEEK_SET);
    readBytes = DFile_Read(lf->_base._file, buffer, length);
    if(readBytes < length)
    {
        /// \todo Do not do this here.
        Con_Error("LumpFile::ReadLumpSection: Only read %lu of %lu bytes of lump #%i.",
                  (unsigned long) readBytes, (unsigned long) length, lumpIdx);
    }
    return readBytes;
    }
}

size_t LumpFile_ReadLumpSection(lumpfile_t* lf, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    return LumpFile_ReadLumpSection2(lf, lumpIdx, buffer, startOffset, length, true);
}

size_t LumpFile_ReadLump2(lumpfile_t* lf, int lumpIdx, uint8_t* buffer, boolean tryCache)
{
    const lumpinfo_t* info = LumpFile_LumpInfo(lf, lumpIdx);
    if(!info) return 0;
    return LumpFile_ReadLumpSection2(lf, lumpIdx, buffer, 0, info->size, tryCache);
}

size_t LumpFile_ReadLump(lumpfile_t* lf, int lumpIdx, uint8_t* buffer)
{
    return LumpFile_ReadLump2(lf, lumpIdx, buffer, true);
}

const uint8_t* LumpFile_CacheLump(lumpfile_t* lf, int lumpIdx, int tag)
{
    assert(lf);
    {
    const lumpinfo_t* info = LumpFile_LumpInfo(lf, lumpIdx);
    boolean isCached = (NULL != lf->_cacheData);
    void** cachePtr = (void**)&lf->_cacheData;

    VERBOSE2(
        Con_Printf("LumpFile::CacheLump: \"%s:%s\" (%lu bytes%s) %s\n",
                F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)lf))),
                (info->name[0]? info->name : F_PrettyPath(Str_Text(&info->path))),
                (unsigned long) info->size, (info->compressedSize != info->size? ", compressed" : ""),
                isCached? "hit":"miss") )

    if(!isCached)
    {
        uint8_t* ptr;
        assert(NULL != info);
        ptr = (uint8_t*)Z_Malloc(info->size, tag, cachePtr);
        if(NULL == ptr)
            Con_Error("LumpFile::CacheLump: Failed on allocation of %lu bytes for "
                "cache copy of lump #%i.", (unsigned long) info->size, lumpIdx);
        LumpFile_ReadLump2(lf, lumpIdx, ptr, false);
    }
    else
    {
        Z_ChangeTag(*cachePtr, tag);
    }

    return (uint8_t*)(*cachePtr);
    }
}

void LumpFile_ChangeLumpCacheTag(lumpfile_t* lf, int lumpIdx, int tag)
{
    assert(lf);
    {
    boolean isCached = (NULL != lf->_cacheData);
    void** cachePtr = (void**)&lf->_cacheData;
    if(isCached)
    {
        Z_ChangeTag2(*cachePtr, tag);
    }
    }
}

int LumpFile_LumpCount(lumpfile_t* lf)
{
    return 1; // Always.
}
