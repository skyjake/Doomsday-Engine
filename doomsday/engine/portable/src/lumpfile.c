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

lumpfile_t* LumpFile_New(const lumpinfo_t* info)
{
    lumpfile_t* file = (lumpfile_t*)malloc(sizeof(*file));
    if(NULL == file)
        Con_Error("LumpFile::Construct:: Failed on allocation of %lu bytes for new LumpFile.",
            (unsigned long) sizeof(*file));

    AbstractFile_Init((abstractfile_t*)file, FT_LUMPFILE, info);
    file->_cacheData = NULL;

    return file;
}

void LumpFile_Delete(lumpfile_t* file)
{
    assert(NULL != file);
    LumpFile_Close(file);
    F_ReleaseFile((abstractfile_t*)file);
    LumpFile_ClearLumpCache(file);
    F_DestroyLumpInfo(&file->_base._info);
    free(file);
}

int LumpFile_PublishLumpsToDirectory(lumpfile_t* file, lumpdirectory_t* directory)
{
    assert(NULL != file && NULL != directory);
    // Insert the lumps into their rightful places in the directory.
    LumpDirectory_Append(directory, (abstractfile_t*)file, 0, 1);
    return 1;
}

const lumpinfo_t* LumpFile_LumpInfo(lumpfile_t* file, int lumpIdx)
{
    assert(NULL != file);
    /// Lump files are special cases for this *is* the lump.
    return AbstractFile_Info((abstractfile_t*)file);
}

void LumpFile_ClearLumpCache(lumpfile_t* file)
{
    assert(NULL != file);
    if(file->_cacheData)
    {
        // If the block has a user, it must be explicitly freed.
        if(Z_GetTag(file->_cacheData) < PU_MAP)
            Z_ChangeTag(file->_cacheData, PU_MAP);
        // Mark the memory pointer in use, but unowned.
        Z_ChangeUser(file->_cacheData, (void*) 0x2);
    }
}

size_t LumpFile_ReadLumpSection2(lumpfile_t* file, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length, boolean tryCache)
{
    assert(NULL != file);
    {
    const lumpinfo_t* info = LumpFile_LumpInfo(file, lumpIdx);
    size_t readBytes;

    VERBOSE2(
        Con_Printf("LumpFile::ReadLumpSection: \"%s:%s\" (%lu bytes%s) [%lu +%lu]",
                F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)file))),
                (info->name[0]? info->name : F_PrettyPath(Str_Text(&info->path))), (unsigned long) info->size,
                (info->compressedSize != info->size? ", compressed" : ""),
                (unsigned long) startOffset, (unsigned long)length) )

    // Try to avoid a file system read by checking for a cached copy.
    if(tryCache && NULL != file->_cacheData)
    {
        boolean isCached = (NULL != file->_cacheData);
        void** cachePtr = (void**)&file->_cacheData;
        if(isCached)
        {
            VERBOSE2( Con_Printf(" from cache\n") )
            readBytes = MIN_OF(info->size, length);
            memcpy(buffer, (char*)*cachePtr + startOffset, readBytes);
            return readBytes;
        }
    }

    VERBOSE2( Con_Printf("\n") )
    F_Seek(&file->_base._stream, info->baseOffset + startOffset, SEEK_SET);
    readBytes = F_Read(&file->_base._stream, buffer, length);
    if(readBytes < length)
    {
        /// \todo Do not do this here.
        Con_Error("LumpFile::ReadLumpSection: Only read %lu of %lu bytes of lump #%i.",
                  (unsigned long) readBytes, (unsigned long) length, lumpIdx);
    }
    return readBytes;
    }
}

size_t LumpFile_ReadLumpSection(lumpfile_t* file, int lumpIdx, uint8_t* buffer,
    size_t startOffset, size_t length)
{
    return LumpFile_ReadLumpSection2(file, lumpIdx, buffer, startOffset, length, true);
}

size_t LumpFile_ReadLump2(lumpfile_t* file, int lumpIdx, uint8_t* buffer, boolean tryCache)
{
    assert(NULL != file);
    {
    const lumpinfo_t* info = LumpFile_LumpInfo(file, lumpIdx);
    assert(NULL != info);
    return LumpFile_ReadLumpSection2(file, lumpIdx, buffer, 0, info->size, tryCache);
    }
}

size_t LumpFile_ReadLump(lumpfile_t* file, int lumpIdx, uint8_t* buffer)
{
    return LumpFile_ReadLump2(file, lumpIdx, buffer, true);
}

const uint8_t* LumpFile_CacheLump(lumpfile_t* file, int lumpIdx, int tag)
{
    assert(NULL != file);
    {
    const lumpinfo_t* info = LumpFile_LumpInfo(file, lumpIdx);
    boolean isCached = (NULL != file->_cacheData);
    void** cachePtr = (void**)&file->_cacheData;

    VERBOSE2(
        Con_Printf("LumpFile::CacheLump: \"%s:%s\" (%lu bytes%s) %s\n",
                F_PrettyPath(Str_Text(AbstractFile_Path((abstractfile_t*)file))),
                (info->name[0]? info->name : F_PrettyPath(Str_Text(&info->path))), (unsigned long) info->size,
                (info->compressedSize != info->size? ", compressed" : ""),
                isCached? "hit":"miss") )

    if(!isCached)
    {
        uint8_t* ptr;
        assert(NULL != info);
        ptr = (uint8_t*)Z_Malloc(info->size, tag, cachePtr);
        if(NULL == ptr)
            Con_Error("LumpFile::CacheLump: Failed on allocation of %lu bytes for "
                "cache copy of lump #%i.", (unsigned long) info->size, lumpIdx);
        LumpFile_ReadLump2(file, lumpIdx, ptr, false);
    }
    else
    {
        Z_ChangeTag(*cachePtr, tag);
    }

    return (uint8_t*)(*cachePtr);
    }
}

void LumpFile_ChangeLumpCacheTag(lumpfile_t* file, int lumpIdx, int tag)
{
    assert(NULL != file);
    {
    boolean isCached = (NULL != file->_cacheData);
    void** cachePtr = (void**)&file->_cacheData;
    if(isCached)
    {
        Z_ChangeTag2(*cachePtr, tag);
    }
    }
}

void LumpFile_Close(lumpfile_t* file)
{
    assert(NULL != file);
    if(file->_base._flags.open)
        F_CloseFile(&file->_base._stream);
    file->_base._flags.open = false;
}

int LumpFile_LumpCount(lumpfile_t* file)
{
    return 1; // Always.
}
